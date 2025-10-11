#include "scene/renderer/vulkan/path_renderer.h"
#include <ranges>
#define GLM_ENABLE_EXPERIMENTAL
#include "gtx/vector_angle.hpp"
#include "tesselator.h"
#include <algorithm>
#include <cmath>
#include <cstring>

namespace Bess::Renderer2D::Vulkan {
    constexpr size_t primitiveResetIndex = 0xFFFFFFFF;

    PathRenderer::PathRenderer(const std::shared_ptr<VulkanDevice> &device,
                               const std::shared_ptr<VulkanOffscreenRenderPass> &renderPass,
                               VkExtent2D extent)
        : m_device(device), m_renderPass(renderPass), m_extent(extent) {
        m_pathStrokePipeline = std::make_unique<Pipelines::PathStrokePipeline>(device, renderPass, extent);
        m_pathFillPipeline = std::make_unique<Pipelines::PathFillPipeline>(device, renderPass, extent);
    }

    PathRenderer::~PathRenderer() = default;

    void PathRenderer::drawPath(Renderer::Path &path, ContoursDrawInfo info) {
        // Try to use cached geometry without copying
        const PathGeometryCacheEntry *cachedPtr = nullptr;
        const std::vector<std::vector<CommonVertex>> *strokeVerticesRef = nullptr;
        const std::vector<CommonVertex> *fillVerticesRef = nullptr;
        std::vector<std::vector<CommonVertex>> generatedStrokeVertices;
        std::vector<CommonVertex> generatedFillVertices;
        if (m_cache.getEntryPtr(path.uuid, cachedPtr)) {
            strokeVerticesRef = &cachedPtr->strokeVertices;
            fillVerticesRef = &cachedPtr->fillVertices;
        } else {
            auto &contours = path.getContours();
            if (info.genStroke) {
                for (const auto &points : contours) {
                    auto vertices = generateStrokeGeometry(points, info.strokeColor, info.closePath);
                    generatedStrokeVertices.emplace_back(std::move(vertices));
                }
                strokeVerticesRef = &generatedStrokeVertices;
            }

            if (info.genFill) {
                generatedFillVertices = generateFillGeometry(contours, info.fillColor);
                fillVerticesRef = &generatedFillVertices;
            }
            m_cache.cacheEntry({
                .pathId = path.uuid,
                .strokeVertices = generatedStrokeVertices,
                .fillVertices = generatedFillVertices,
            });
        }

        // For stroke rendering, still use the old approach for now
        if (strokeVerticesRef) {
            for (const auto &vertices : *strokeVerticesRef) {
                auto translated = vertices; // copy once into frame heap, not cache
                for (auto &p : translated) p.position += info.translate;
                m_strokeVertices.insert(m_strokeVertices.end(), translated.begin(), translated.end());
                auto s = m_strokeVertices.size() - translated.size();
                auto indices = std::views::iota(s, m_strokeVertices.size());
                m_strokeIndices.insert(m_strokeIndices.end(), indices.begin(), indices.end());
                m_strokeIndices.emplace_back(primitiveResetIndex);
            }
        }

        // For fill rendering, store vertices without translation and batch draw call
        if (fillVerticesRef && !fillVerticesRef->empty()) {
            // Cache geometry in per-frame atlas (once per unique UUID)
            auto glyphId = path.uuid;
            auto found = m_glyphIdToMesh.find(glyphId);
            if (found == m_glyphIdToMesh.end()) {
                uint32_t firstIndex = static_cast<uint32_t>(m_fillIndices.size());
                uint32_t baseVertex = static_cast<uint32_t>(m_fillVertices.size());
                m_fillVertices.insert(m_fillVertices.end(), fillVerticesRef->begin(), fillVerticesRef->end());
                auto localCount = static_cast<uint32_t>(fillVerticesRef->size());
                for (uint32_t i = 0; i < localCount; ++i) m_fillIndices.emplace_back(baseVertex + i);
                m_glyphIdToMesh[glyphId] = {firstIndex, localCount};
            }
            m_glyphIdToInstances[glyphId].emplace_back(FillInstance{glm::vec2(info.translate.x, info.translate.y), info.scale});
            m_glyphIdToInstances[glyphId].back().pickId = (int)glyphId;
        }

        // We already appended translated stroke above; nothing else to do here for stroke
    }

    void PathRenderer::addPathGeometries(const std::vector<std::vector<CommonVertex>> &strokeGeometry, const std::vector<CommonVertex> &fillGeometry) {
        if (!strokeGeometry.empty()) {
            for (const auto &vertices : strokeGeometry) {
                auto s = m_strokeVertices.size();
                m_strokeVertices.insert(m_strokeVertices.end(), vertices.begin(), vertices.end());
                auto indices = std::views::iota(s, m_strokeVertices.size());
                m_strokeIndices.insert(m_strokeIndices.end(), indices.begin(), indices.end());
                m_strokeIndices.emplace_back(primitiveResetIndex);
            }
        }

        if (!fillGeometry.empty()) {
            auto s = m_fillVertices.size();
            m_fillVertices.insert(m_fillVertices.end(), fillGeometry.begin(), fillGeometry.end());
            auto indices = std::views::iota(s, m_fillVertices.size());
            m_fillIndices.insert(m_fillIndices.end(), indices.begin(), indices.end());
        }
    }

    void PathRenderer::drawContours(const std::vector<std::vector<PathPoint>> &contours, ContoursDrawInfo info) {
        // auto transform = glm::translate(glm::mat4(1.f), {m_pathData.startPos.x, m_pathData.startPos.y, m_pathData.startPos.y});
        // transform = glm::scale(transform, {20.f / 48.f, 20.f / 48.f, 1.f});
        std::vector<std::vector<CommonVertex>> strokeVertices;
        std::vector<CommonVertex> fillVertices;
        if (info.genStroke) {
            for (const auto &points : contours) {
                auto vertices = generateStrokeGeometry(points, info.strokeColor, info.closePath);

                for (auto &p : vertices) {
                    p.position += info.translate;
                }
                strokeVertices.emplace_back(vertices);
            }
        }

        if (info.genFill) {
            fillVertices = generateFillGeometry(contours, info.fillColor);
            // Instanced path fill: append geometry once per glyphId and add an instance for this draw
            if (!fillVertices.empty()) {
                uint64_t glyphId = info.glyphId;
                if (glyphId == 0) {
                    // Fallback: hash bounds as glyph id if path API did not supply
                    glyphId = reinterpret_cast<uint64_t>(this); // simple stable fallback per renderer
                }
                auto found = m_glyphIdToMesh.find(glyphId);
                if (found == m_glyphIdToMesh.end()) {
                    uint32_t firstIndex = static_cast<uint32_t>(m_fillIndices.size());
                    uint32_t baseVertex = static_cast<uint32_t>(m_fillVertices.size());
                    m_fillVertices.insert(m_fillVertices.end(), fillVertices.begin(), fillVertices.end());
                    auto localCount = static_cast<uint32_t>(fillVertices.size());
                    for (uint32_t i = 0; i < localCount; ++i) m_fillIndices.emplace_back(baseVertex + i);
                    m_glyphIdToMesh[glyphId] = {firstIndex, localCount};
                }
                m_glyphIdToInstances[glyphId].emplace_back(FillInstance{glm::vec2(info.translate.x, info.translate.y), info.scale});
                m_glyphIdToInstances[glyphId].back().pickId = (int)glyphId;
            }
        }

        // Only stroke goes via immediate path; fill is handled by instancing batch above
        addPathGeometries(strokeVertices, {});
    }

    void PathRenderer::beginFrame(VkCommandBuffer commandBuffer) {
        m_currentCommandBuffer = commandBuffer;
    }

    void PathRenderer::endFrame() {
        if (!m_pathData.ended) {
            endPathMode();
        }

        // Batched fill using GPU instancing
        if (!m_fillVertices.empty() && !m_fillIndices.empty() && !m_glyphIdToMesh.empty()) {
            // Build packed instance array and draw calls per unique glyph
            m_tempInstances.clear();
            m_fillDrawCalls.clear();
            uint32_t firstInstance = 0;
            for (auto &kv : m_glyphIdToInstances) {
                auto glyphId = kv.first;
                auto &instances = kv.second;
                auto mesh = m_glyphIdToMesh[glyphId];
                Pipelines::PathFillPipeline::FillDrawCall dc{};
                dc.firstIndex = mesh.firstIndex;
                dc.indexCount = mesh.indexCount;
                dc.firstInstance = firstInstance;
                dc.instanceCount = (uint32_t)instances.size();
                m_fillDrawCalls.emplace_back(dc);
                m_tempInstances.insert(m_tempInstances.end(), instances.begin(), instances.end());
                firstInstance += dc.instanceCount;
            }

            m_pathFillPipeline->beginPipeline(m_currentCommandBuffer);
            m_pathFillPipeline->setBatchedPathData(m_fillVertices, m_fillIndices, m_fillDrawCalls);
            m_pathFillPipeline->setInstanceData(m_tempInstances);
            m_pathFillPipeline->endPipeline();
        } else if (!m_fillVertices.empty() && !m_fillIndices.empty()) {
            // Fallback: draw as a single non-instanced mesh with a default instance (translation=0, scale=1)
            m_tempInstances.clear();
            m_fillDrawCalls.clear();
            m_tempInstances.emplace_back(FillInstance{glm::vec2(0.0f), glm::vec2(1.0f), (int)m_pathData.id});
            Pipelines::PathFillPipeline::FillDrawCall dc{};
            dc.firstIndex = 0;
            dc.indexCount = static_cast<uint32_t>(m_fillIndices.size());
            dc.firstInstance = 0;
            dc.instanceCount = 1;
            m_fillDrawCalls.emplace_back(dc);

            m_pathFillPipeline->beginPipeline(m_currentCommandBuffer);
            m_pathFillPipeline->setBatchedPathData(m_fillVertices, m_fillIndices, m_fillDrawCalls);
            m_pathFillPipeline->setInstanceData(m_tempInstances);
            m_pathFillPipeline->endPipeline();
        }

        // Draw stroke after fill so it appears on top
        m_pathStrokePipeline->beginPipeline(m_currentCommandBuffer);
        m_pathStrokePipeline->setPathData(m_strokeVertices, m_strokeIndices);
        m_pathStrokePipeline->endPipeline();

        m_fillVertices.clear();
        m_fillIndices.clear();
        m_fillDrawCalls.clear();
        m_tempInstances.clear();
        m_glyphIdToMesh.clear();
        m_glyphIdToInstances.clear();
        m_strokeVertices.clear();
        m_strokeIndices.clear();
    }

    void PathRenderer::setCurrentFrameIndex(uint32_t frameIndex) {
        m_pathStrokePipeline->setCurrentFrameIndex(frameIndex);
        m_pathFillPipeline->setCurrentFrameIndex(frameIndex);
    }

    void PathRenderer::beginPathMode(const glm::vec3 &startPos, float weight, const glm::vec4 &color, uint64_t id) {
        m_pathData.ended = false;
        m_pathData.startPos = startPos;
        m_pathData.currentPos = startPos;
        m_pathData.points.emplace_back(PathPoint{startPos, weight, (int64_t)id});
        m_pathData.color = color;
        m_pathData.id = (int64_t)id;
    }

    void PathRenderer::endPathMode(bool closePath, bool genFill, const glm::vec4 &fillColor, bool genStroke) {
        if (m_pathData.ended)
            return;

        if (!m_pathData.points.empty()) {
            m_pathData.contours.emplace_back(std::move(m_pathData.points));
            m_pathData.points.clear();
        }

        ContoursDrawInfo info{};
        info.closePath = closePath;
        info.genFill = genFill;
        info.genStroke = genStroke;
        info.fillColor = fillColor;
        info.strokeColor = m_pathData.color;
        info.translate = glm::vec3(0.0f); // path API positions are absolute, avoid double-translation
        info.scale = glm::vec2(1.0f);
        info.glyphId = static_cast<uint64_t>(m_pathData.id);

        drawContours(m_pathData.contours, info);

        m_pathData = {};
    }

    void PathRenderer::pathLineTo(const glm::vec3 &pos, float size, const glm::vec4 &color, int id) {
        if (m_pathData.ended)
            return;
        m_pathData.points.emplace_back(PathPoint{pos, size, id});
        m_pathData.setCurrentPos(pos);
    }

    void PathRenderer::pathCubicBeizerTo(const glm::vec3 &end, const glm::vec2 &controlPoint1, const glm::vec2 &controlPoint2,
                                         float weight, const glm::vec4 &color, int id) {
        if (m_pathData.ended)
            return;
        auto positions = generateCubicBezierPoints(m_pathData.currentPos, controlPoint1, controlPoint2, end);
        m_pathData.points.reserve(m_pathData.points.size() + positions.size());
        for (auto pos : positions) {
            m_pathData.points.emplace_back(PathPoint{pos, weight, id});
        }
        m_pathData.setCurrentPos(end);
    }

    void PathRenderer::pathQuadBeizerTo(const glm::vec3 &end, const glm::vec2 &controlPoint, float weight, const glm::vec4 &color, int id) {
        if (m_pathData.ended)
            return;
        auto positions = generateQuadBezierPoints(m_pathData.currentPos, controlPoint, end);
        m_pathData.points.reserve(m_pathData.points.size() + positions.size());
        for (auto pos : positions) {
            m_pathData.points.emplace_back(PathPoint{pos, weight, id});
        }
        m_pathData.setCurrentPos(end);
    }

    void PathRenderer::updateUniformBuffer(const UniformBufferObject &ubo) {
        m_pathStrokePipeline->updateUniformBuffer(ubo);
        m_pathFillPipeline->updateUniformBuffer(ubo);
    }

    std::vector<CommonVertex> PathRenderer::generateStrokeGeometry(const std::vector<PathPoint> &points,
                                                                   const glm::vec4 &color, bool isClosed) {
        if (points.size() < (isClosed ? 3 : 2)) {
            return {};
        }

        auto makeVertex = [&](const glm::vec2 &pos, float z, uint64_t id, const glm::vec2 &uv) {
            CommonVertex v;
            v.position = glm::vec3(pos, z);
            v.color = color;
            v.id = static_cast<int>(id & 0x7FFFFFFF);
            v.texCoord = uv;
            v.texSlotIdx = -1;
            return v;
        };

        // --- 1. Pre-calculate total path length for correct UV mapping ---
        float totalLength = 0.0f;
        float maxWeight = 0.f;
        for (size_t i = 0; i < points.size() - 1; ++i) {
            totalLength += glm::distance(glm::vec2(points[i].pos), glm::vec2(points[i + 1].pos));
            maxWeight = std::max(maxWeight, points[i].weight);
        }

        float miterLimit = maxWeight * 1.5f;

        if (isClosed) {
            totalLength += glm::distance(glm::vec2(points.back().pos), glm::vec2(points.front().pos));
        }

        std::vector<CommonVertex> stripVertices;
        float cumulativeLength = 0.0f;
        const size_t pointCount = points.size();
        const size_t n = pointCount;

        size_t ni = 0;
        // --- 2. Generate Joins and Caps ---
        for (size_t i = 0; i < n; i = ni) {
            // skipping continuous same points
            ni = i + 1;
            while (ni < n && points[ni].pos == points[i].pos) {
                ni++;
            }

            const auto &pCurr = points[i];
            const auto &pPrev = isClosed ? points[(i + pointCount - 1) % pointCount] : points[(i == 0) ? 0 : (i - 1)];
            const auto &pNext = isClosed ? points[ni % pointCount] : points[std::min(pointCount - 1, ni)];

            // Update cumulative length for UVs. For the first point, it's 0.
            if (i > 0) {
                cumulativeLength += glm::distance(glm::vec2(pCurr.pos), glm::vec2(pPrev.pos));
            }
            float u = (totalLength > 0) ? (cumulativeLength / totalLength) : 0;

            // --- Handle Caps for Open Paths ---
            if (!isClosed && i == 0) { // Start Cap
                glm::vec2 dir = glm::normalize(glm::vec2(pNext.pos) - glm::vec2(pCurr.pos));
                glm::vec2 normalVec = glm::vec2(-dir.y, dir.x) * pCurr.weight / 2.f;
                stripVertices.push_back(makeVertex(glm::vec2(pCurr.pos) - normalVec, pCurr.pos.z, pNext.id, {u, 1.f}));
                stripVertices.push_back(makeVertex(glm::vec2(pCurr.pos) + normalVec, pCurr.pos.z, pNext.id, {u, 0.f}));
                continue;
            }

            if (!isClosed && i == n - 1) { // End Cap
                glm::vec2 dir = glm::normalize(glm::vec2(pCurr.pos) - glm::vec2(pPrev.pos));
                glm::vec2 normalVec = glm::vec2(-dir.y, dir.x) * pCurr.weight / 2.f;
                stripVertices.push_back(makeVertex(glm::vec2(pCurr.pos) - normalVec, pCurr.pos.z, pCurr.id, {u, 1.f}));
                stripVertices.push_back(makeVertex(glm::vec2(pCurr.pos) + normalVec, pCurr.pos.z, pCurr.id, {u, 0.f}));
                continue;
            }

            // --- Handle Interior and Closed Path Joins ---
            glm::vec2 dirIn = glm::normalize(glm::vec2(pCurr.pos) - glm::vec2(pPrev.pos));
            glm::vec2 dirOut = glm::normalize(glm::vec2(pNext.pos) - glm::vec2(pCurr.pos));
            glm::vec2 dirOutToPrev = glm::normalize(glm::vec2(pPrev.pos - pCurr.pos));
            glm::vec2 normalIn(-dirIn.y, dirIn.x);
            glm::vec2 normalOut(-dirOut.y, dirOut.x);
            float dotProduct = glm::dot(dirIn, dirOut);

            // Handle straight lines
            if (std::abs(dotProduct) == 1.f) {
                glm::vec2 normal = normalIn * pCurr.weight / 2.f;
                stripVertices.push_back(makeVertex(glm::vec2(pCurr.pos) - normal, pCurr.pos.z, pNext.id, {u, 1.f}));
                stripVertices.push_back(makeVertex(glm::vec2(pCurr.pos) + normal, pCurr.pos.z, pNext.id, {u, 0.f}));
                continue;
            }

            // Joint
            {
                auto uVec = dirOutToPrev;
                auto vVec = dirOut;
                float angle = glm::angle(uVec, vVec);

                float uMag = pNext.weight / (2 * glm::sin(angle));
                float vMag = pCurr.weight / (2 * glm::sin(angle));

                uVec *= uMag;
                vVec *= vMag;

                auto disp = uVec + vVec;

                float crossProductZ = (dirIn.x * dirOut.y) - (dirIn.y * dirOut.x);

                if (glm::length(disp) > miterLimit) {
                    glm::vec2 normalIn = glm::normalize(glm::vec2(-dirIn.y, dirIn.x));

                    glm::vec2 pos = pCurr.pos;
                    float halfWidth = pCurr.weight / 2.f;

                    glm::vec2 outerPrev = pos + normalIn * halfWidth;
                    glm::vec2 innerPrev = pos - normalIn * halfWidth;

                    glm::vec2 outerNext = pos + normalOut * halfWidth;
                    glm::vec2 innerNext = pos - normalOut * halfWidth;
                    stripVertices.push_back(makeVertex(innerPrev, pCurr.pos.z, pCurr.id, {u, 1.f}));
                    stripVertices.push_back(makeVertex(outerPrev, pCurr.pos.z, pCurr.id, {u, 0.f}));

                    stripVertices.push_back(makeVertex(innerNext, pCurr.pos.z, pCurr.id, {u, 1.f}));
                    stripVertices.push_back(makeVertex(outerNext, pCurr.pos.z, pCurr.id, {u, 0.f}));
                } else {
                    auto D = glm::vec2(pCurr.pos) + disp;
                    auto E = glm::vec2(pCurr.pos) - disp;

                    if (crossProductZ > 0) { // Left turn
                        stripVertices.push_back(makeVertex(E, pCurr.pos.z, pNext.id, {u, 1.f}));
                        stripVertices.push_back(makeVertex(D, pCurr.pos.z, pNext.id, {u, 0.f}));
                    } else { // Right turn
                        stripVertices.push_back(makeVertex(D, pCurr.pos.z, pNext.id, {u, 1.f}));
                        stripVertices.push_back(makeVertex(E, pCurr.pos.z, pNext.id, {u, 0.f}));
                    }
                }
            }
        }

        if (isClosed && !stripVertices.empty()) {
            CommonVertex finalRight = stripVertices[0];
            finalRight.texCoord.x = 1.0f;
            CommonVertex finalLeft = stripVertices[1];
            finalLeft.texCoord.x = 1.0f;

            stripVertices.push_back(finalRight);
            stripVertices.push_back(finalLeft);
        }

        return stripVertices;
    }

    std::vector<CommonVertex> PathRenderer::generateFillGeometry(const std::vector<PathPoint> &points, const glm::vec4 &color) {
        std::vector<std::vector<PathPoint>> contours;
        contours.push_back(points);
        return generateFillGeometry(contours, color);
    }

    std::vector<CommonVertex> PathRenderer::generateFillGeometry(
        const std::vector<std::vector<PathPoint>> &contours,
        const glm::vec4 &color) {
        std::vector<CommonVertex> out;
        if (contours.empty())
            return out;

        glm::vec2 minPt(std::numeric_limits<float>::max());
        glm::vec2 maxPt(-std::numeric_limits<float>::max());
        for (const auto &c : contours) {
            for (const auto &p : c) {
                glm::vec2 v(p.pos.x, p.pos.y);
                minPt = glm::min(minPt, v);
                maxPt = glm::max(maxPt, v);
            }
        }
        glm::vec2 size = glm::max(maxPt - minPt, glm::vec2(0.001f));
        auto uvOf = [&](const glm::vec3 &pos) -> glm::vec2 {
            glm::vec2 p(pos.x, pos.y);
            return (p - minPt) / size;
        };

        TESStesselator *tess = tessNewTess(nullptr);
        if (!tess)
            return out;

        for (const auto &c : contours) {
            if (c.size() < 3)
                continue;
            std::vector<TESSreal> coords;
            coords.reserve(c.size() * 2);
            for (const auto &pt : c) {
                coords.push_back((TESSreal)pt.pos.x);
                coords.push_back((TESSreal)pt.pos.y);
            }
            tessAddContour(tess, 2, coords.data(), sizeof(TESSreal) * 2, (int)c.size());
        }

        constexpr int polySize = 3;
        if (!tessTesselate(tess,
                           TESS_WINDING_NONZERO,
                           TESS_POLYGONS,
                           polySize, // number of indices per element
                           2,        // vertex size (x,y)
                           nullptr)) // normal
        {
            tessDeleteTess(tess);
            return out;
        }

        const TESSreal *verts = tessGetVertices(tess);
        const int *vindex = tessGetVertexIndices(tess); // maps tess vertex -> original point index (or -1)
        const int nverts = tessGetVertexCount(tess);
        const int *elems = tessGetElements(tess);
        const int nelems = tessGetElementCount(tess);

        std::vector<const PathPoint *> flatPoints;
        flatPoints.reserve(1024);
        for (const auto &c : contours)
            for (const auto &p : c)
                flatPoints.push_back(&p);

        auto makeVertex = [&](int vi) -> CommonVertex {
            glm::vec3 pos((float)verts[(vi * 2) + 0], (float)verts[(vi * 2) + 1], 0.0f);

            int mapped = vindex ? vindex[vi] : -1;
            float z = 0.0f;
            int id = 0;
            if (mapped >= 0 && mapped < (int)flatPoints.size()) {
                z = flatPoints[mapped]->pos.z;
                id = (int)flatPoints[mapped]->id;
            } else if (!flatPoints.empty()) {
                z = flatPoints.front()->pos.z;
                id = (int)flatPoints.front()->id;
            }
            pos.z = z;

            CommonVertex v;
            v.position = pos;
            v.color = color;
            v.id = (id & 0x7FFFFFFF);
            v.texCoord = uvOf(pos);
            return v;
        };

        out.reserve(static_cast<size_t>(nelems) * 3);
        for (int i = 0; i < nelems; ++i) {
            const int *poly = elems + (static_cast<ptrdiff_t>(i) * polySize);

            int i0 = poly[0], i1 = poly[1], i2 = poly[2];
            if (i0 < 0 || i1 < 0 || i2 < 0)
                continue;
            if (i0 >= nverts || i1 >= nverts || i2 >= nverts)
                continue;

            out.push_back(makeVertex(i0));
            out.push_back(makeVertex(i1));
            out.push_back(makeVertex(i2));
        }

        tessDeleteTess(tess);
        return out;
    }

    QuadBezierCurvePoints PathRenderer::generateSmoothBendPoints(const glm::vec2 &prevPoint, const glm::vec2 &joinPoint, const glm::vec2 &nextPoint, float curveRadius) {
        glm::vec2 dir1 = glm::normalize(joinPoint - prevPoint);
        glm::vec2 dir2 = glm::normalize(nextPoint - joinPoint);
        glm::vec2 bisector = glm::normalize(dir1 + dir2);
        float offset = glm::dot(bisector, dir1);
        glm::vec2 controlPoint = joinPoint + bisector * offset;
        glm::vec2 startPoint = joinPoint - dir1 * curveRadius;
        glm::vec2 endPoint = joinPoint + dir2 * curveRadius;
        return {startPoint, controlPoint, endPoint};
    }

    int PathRenderer::calculateCubicBezierSegments(const glm::vec2 &p0,
                                                   const glm::vec2 &p1,
                                                   const glm::vec2 &p2,
                                                   const glm::vec2 &p3) {
        glm::vec2 d1 = p0 - (2.0f * p1) + p2;
        glm::vec2 d2 = p1 - (2.0f * p2) + p3;

        float m1 = glm::length(d1);
        float m2 = glm::length(d2);
        float M = 6.0f * std::max(m1, m2);

        float Nf = std::sqrt(M / (8.0f * 0.0001f)); // BEZIER_EPSILON

        return std::max(1, (int)std::ceil(Nf));
    }

    int PathRenderer::calculateQuadBezierSegments(const glm::vec2 &p0,
                                                  const glm::vec2 &p1,
                                                  const glm::vec2 &p2) {
        glm::vec2 d = p0 - (2.0f * p1) + p2;

        float M = 2.0f * glm::length(d);

        float Nf = std::sqrt(M / (8.0f * 0.0001f)); // BEZIER_EPSILON

        return std::max(1, (int)std::ceil(Nf));
    }

    glm::vec2 PathRenderer::nextBernstinePointCubicBezier(const glm::vec2 &p0, const glm::vec2 &p1,
                                                          const glm::vec2 &p2, const glm::vec2 &p3, const float t) {
        auto t_ = 1 - t;

        glm::vec2 B0 = static_cast<float>(std::pow(t_, 3)) * p0;
        glm::vec2 B1 = static_cast<float>(3 * t * std::pow(t_, 2)) * p1;
        glm::vec2 B2 = static_cast<float>(3 * t * t * std::pow(t_, 1)) * p2;
        glm::vec2 B3 = static_cast<float>(t * t * t) * p3;

        return B0 + B1 + B2 + B3;
    }

    glm::vec2 PathRenderer::nextBernstinePointQuadBezier(const glm::vec2 &p0, const glm::vec2 &p1, const glm::vec2 &p2, const float t) {
        float t_ = 1.0f - t;
        glm::vec2 point = ((t_ * t_) * p0) + (2.0f * (t_ * t) * p1) + ((t * t) * p2);
        return point;
    }

    std::vector<glm::vec3> PathRenderer::generateCubicBezierPoints(const glm::vec3 &start, const glm::vec2 &controlPoint1, const glm::vec2 &controlPoint2, const glm::vec3 &end) {
        std::vector<glm::vec3> points;
        int segments = calculateCubicBezierSegments(start, controlPoint1, controlPoint2, end);

        for (int i = 1; i <= segments; i++) {
            float t = (float)i / (float)segments;
            glm::vec2 bP = nextBernstinePointCubicBezier(start, controlPoint1, controlPoint2, end, t);
            glm::vec3 p = {bP.x, bP.y, glm::mix(start.z, end.z, t)};
            points.emplace_back(p);
        }

        return points;
    }

    std::vector<glm::vec3> PathRenderer::generateQuadBezierPoints(const glm::vec3 &start, const glm::vec2 &controlPoint, const glm::vec3 &end) {
        std::vector<glm::vec3> points;
        int segments = calculateQuadBezierSegments(start, controlPoint, end);

        for (int i = 1; i <= segments; i++) {
            float t = (float)i / (float)segments;
            glm::vec2 bP = nextBernstinePointQuadBezier(start, controlPoint, end, t);
            glm::vec3 p = {bP.x, bP.y, glm::mix(start.z, end.z, t)};
            points.emplace_back(p);
        }

        return points;
    }

    std::vector<uint32_t> PathRenderer::generateFillIndices(size_t vertexCount) {
        std::vector<uint32_t> indices;
        // The tessellated geometry already generates vertices in triangle format
        // So we need sequential indices for the vertices
        indices.reserve(vertexCount);
        for (size_t i = 0; i < vertexCount; ++i) {
            indices.push_back(static_cast<uint32_t>(i));
        }
        return indices;
    }

    void PathRenderer::resize(VkExtent2D extent) {
        m_pathStrokePipeline->resize(extent);
        m_pathFillPipeline->resize(extent);
    }

    void PathRenderer::pathMoveTo(const glm::vec3 &pos) {
        m_pathData.currentPos = pos;

        if (m_pathData.ended) {
            return;
        }

        const bool hasAnyPoints = !m_pathData.points.empty();
        if (!hasAnyPoints) {
            // Start a new subpath with previous weight/id fallback to defaults
            float weight = 1.0f;
            int64_t pid = 0;
            if (!m_pathData.contours.empty() && !m_pathData.contours.back().empty()) {
                const auto &pp = m_pathData.contours.back().back();
                weight = pp.weight;
                pid = pp.id;
            }
            m_pathData.points.emplace_back(PathPoint{pos, weight, pid});
            return;
        }

        auto prevPoint = m_pathData.points.back();

        if (m_pathData.points.size() == 1) {
            m_pathData.points[0] = PathPoint{pos, prevPoint.weight, prevPoint.id};
            return;
        }

        m_pathData.contours.emplace_back(std::move(m_pathData.points));
        m_pathData.points.clear();
        m_pathData.points.emplace_back(PathPoint{pos, prevPoint.weight, prevPoint.id});
    }
} // namespace Bess::Renderer2D::Vulkan
