#include "scene/renderer/vulkan/path_renderer.h"
#include "common/bess_uuid.h"
#include "geometric.hpp"
#include <algorithm>
#include <cstdint>
#include <ranges>
#define GLM_ENABLE_EXPERIMENTAL
#include "gtx/vector_angle.hpp"
#include "tesselator.h"
#include <algorithm>
#include <cmath>
#include <cstring>

namespace Bess::Renderer2D::Vulkan {
    static inline void hash_combine(uint64_t &seed, uint64_t v) {
        // 64-bit boost::hash_combine-like
        seed ^= v + 0x9e3779b97f4a7c15ULL + (seed << 6) + (seed >> 2);
    }

    glm::uvec2 encodeId(uint64_t id) {
        glm::uvec2 encoded;
        encoded.x = static_cast<uint32_t>(id & 0xFFFFFFFFULL);
        encoded.y = static_cast<uint32_t>((id >> 32));
        return encoded;
    }

    uint64_t decodeId(const glm::uvec2 &encoded) {
        uint64_t id = static_cast<uint64_t>(encoded.y);
        id = (id << 32) | static_cast<uint64_t>(encoded.x);
        return id;
    }

    uint64_t PathRenderer::hashContours(const uint64_t &id,
                                        const std::vector<std::vector<PathPoint>> &contours,
                                        const glm::vec4 &color,
                                        bool isClosed, bool rounded) {
        uint64_t seed = 1469598103934665603ULL; // FNV-ish base
        auto h = [&seed](uint64_t v) { hash_combine(seed, v); };
        h(id);
        h(static_cast<uint64_t>(contours.size()));
        for (const auto &c : contours) {
            h(static_cast<uint64_t>(c.size()));
            for (const auto &p : c) {
                // quantize to reduce churn from float noise
                auto qf = [](float f) -> int32_t { return (int32_t)std::llround(f * 1000.0f); };
                h(static_cast<uint64_t>(qf(p.pos.x)));
                h(static_cast<uint64_t>(qf(p.pos.y)));
                h(static_cast<uint64_t>(qf(p.pos.z)));
                h(static_cast<uint64_t>(qf(p.weight)));
                h(static_cast<uint64_t>(p.id & 0xFFFFFFFFULL));
            }
        }
        auto qf = [](float f) -> int32_t { return (int32_t)std::llround(f * 1000.0f); };
        h(static_cast<uint64_t>(qf(color.r)));
        h(static_cast<uint64_t>(qf(color.g)));
        h(static_cast<uint64_t>(qf(color.b)));
        h(static_cast<uint64_t>(qf(color.a)));
        h(isClosed ? 1ULL : 0ULL);
        h(rounded ? 1ULL : 0ULL);
        return seed;
    }

    constexpr size_t primitiveResetIndex = 0xFFFFFFFF;
    constexpr float kRoundedJoinRadius = 8.0f;
    constexpr int kMaxRoundedSegments = 24;

    PathRenderer::PathRenderer(const std::shared_ptr<VulkanDevice> &device,
                               const std::shared_ptr<VulkanOffscreenRenderPass> &renderPass,
                               VkExtent2D extent)
        : m_device(device), m_renderPass(renderPass), m_extent(extent) {
        m_pathStrokePipeline = std::make_unique<Pipelines::PathStrokePipeline>(device, renderPass, extent);
        m_pathFillPipeline = std::make_unique<Pipelines::PathFillPipeline>(device, renderPass, extent);
    }

    PathRenderer::~PathRenderer() = default;

    void PathRenderer::drawPath(Renderer::Path &path, ContoursDrawInfo info, bool invalidateCache) {
        const PathGeometryCacheEntry *cachedPtr = nullptr;
        const auto cacheKey = PathGeometryCache::generateCacheKey(path.uuid,
                                                                  info.scale,
                                                                  info.rounedJoint);
        bool cacheHit = m_cache.getEntryPtr(cacheKey, cachedPtr);

        if (invalidateCache || path.scale(info.scale)) {
            m_cache.invalidateCacheEntry(cacheKey);
            cacheHit = false;
        }

        const auto &pathProps = path.getProps();

        if (!cacheHit) {
            const auto &pathProps = path.getProps();
            auto &contours = path.getContours();

            PathGeometryCacheEntry newEntry;
            newEntry.pathId = path.uuid;
            newEntry.rounded = pathProps.roundedJoints;

            if (pathProps.renderStroke) {
                for (const auto &points : contours) {
                    newEntry.strokeVertices.emplace_back(generateStrokeGeometry(
                        points, info.strokeColor, pathProps.isClosed, pathProps.roundedJoints));
                }
            }
            if (pathProps.renderFill) {
                newEntry.fillVertices = generateFillGeometry(contours, info.fillColor, pathProps.roundedJoints);
            }

            cachedPtr = m_cache.cacheEntry(cacheKey, newEntry);
        }

        if (cachedPtr && !cachedPtr->strokeVertices.empty()) {
            for (const auto &vertices : cachedPtr->strokeVertices) {

                size_t offset = m_strokeVertices.size();
                m_strokeVertices.reserve(offset + vertices.size());

                auto encodedId = encodeId(info.glyphId);
                for (const auto &v : vertices) {
                    auto &newV = m_strokeVertices.emplace_back(v);
                    newV.position += info.translate; // Move this to GPU if possible!
                    newV.id = encodedId;
                }

                m_strokeIndices.reserve(m_strokeIndices.size() + vertices.size() + 1);
                for (size_t i = 0; i < vertices.size(); ++i) {
                    m_strokeIndices.push_back(static_cast<uint32_t>(offset + i));
                }
                m_strokeIndices.push_back(primitiveResetIndex);
            }
        }

        if (cachedPtr && !cachedPtr->fillVertices.empty()) {
            auto [it, inserted] = m_glyphIdToMesh.try_emplace(cacheKey);

            if (inserted) {
                uint32_t firstIndex = static_cast<uint32_t>(m_fillIndices.size());
                uint32_t baseVertex = static_cast<uint32_t>(m_fillVertices.size());
                uint32_t localCount = static_cast<uint32_t>(cachedPtr->fillVertices.size());

                m_fillVertices.insert(m_fillVertices.end(),
                                      cachedPtr->fillVertices.begin(),
                                      cachedPtr->fillVertices.end());

                m_fillIndices.reserve(m_fillIndices.size() + localCount);
                for (uint32_t i = 0; i < localCount; ++i) {
                    m_fillIndices.push_back(baseVertex + i);
                }

                it->second = {firstIndex, localCount};
            }

            m_glyphIdToInstances[cacheKey].push_back({info.translate,
                                                      glm::vec2(1.f),
                                                      info.fillColor,
                                                      encodeId(info.glyphId)});
        }
    }

    void PathRenderer::addPathGeometries(const std::vector<std::vector<CommonVertex>> &strokeGeometry) {
        if (!strokeGeometry.empty()) {
            for (const auto &vertices : strokeGeometry) {
                auto s = m_strokeVertices.size();
                m_strokeVertices.insert(m_strokeVertices.end(), vertices.begin(), vertices.end());
                auto indices = std::views::iota(s, m_strokeVertices.size());
                m_strokeIndices.insert(m_strokeIndices.end(), indices.begin(), indices.end());
                m_strokeIndices.emplace_back(primitiveResetIndex);
            }
        }
    }

    void PathRenderer::drawContours(const std::vector<std::vector<PathPoint>> &contours,
                                    ContoursDrawInfo info, bool invalidateCache) {
        std::vector<std::vector<CommonVertex>> strokeVertices;
        std::vector<CommonVertex> fillVertices;
        if (info.genStroke) {
            uint64_t key = hashContours(info.glyphId,
                                        contours,
                                        info.strokeColor,
                                        info.closePath,
                                        info.rounedJoint);
            auto dynIt = m_dynamicStrokeCache.find(key);
            if (dynIt != m_dynamicStrokeCache.end() && !invalidateCache) {
                for (auto vertices : dynIt->second.strokeVertices) {
                    for (auto &p : vertices)
                        p.position += info.translate;
                    strokeVertices.emplace_back(std::move(vertices));
                }
            } else {
                if (invalidateCache && dynIt != m_dynamicStrokeCache.end()) {
                    m_dynamicStrokeCache.erase(key);
                    m_dynamicStrokeCacheOrder.erase(std::ranges::remove(m_dynamicStrokeCacheOrder, key).begin(),
                                                    m_dynamicStrokeCacheOrder.end());
                }

                std::vector<std::vector<CommonVertex>> built;
                built.reserve(contours.size());
                for (const auto &points : contours) {
                    auto vertices = generateStrokeGeometry(points, info.strokeColor, info.closePath, info.rounedJoint);
                    for (auto &p : vertices)
                        p.position += info.translate;
                    built.emplace_back(vertices);
                    strokeVertices.emplace_back(vertices);
                }

                constexpr size_t kMaxDynEntries = 2048;
                PathGeometryCacheEntry entry{};
                entry.pathId = UUID{};
                entry.strokeVertices = built;
                entry.fillVertices = {};
                entry.rounded = info.rounedJoint;
                m_dynamicStrokeCache[key] = std::move(entry);
                m_dynamicStrokeCacheOrder.push_back(key);
                if (m_dynamicStrokeCacheOrder.size() > kMaxDynEntries) {
                    uint64_t old = m_dynamicStrokeCacheOrder.front();
                    m_dynamicStrokeCacheOrder.pop_front();
                    m_dynamicStrokeCache.erase(old);
                }
            }
        }

        if (info.genFill) {
            uint64_t keyFill = hashContours(info.glyphId,
                                            contours,
                                            info.fillColor,
                                            info.closePath,
                                            info.rounedJoint);
            const std::vector<CommonVertex> *fillGeomRef = nullptr;
            auto dynItF = m_dynamicStrokeCache.find(keyFill);
            if (dynItF != m_dynamicStrokeCache.end() &&
                !dynItF->second.fillVertices.empty() &&
                !invalidateCache) {
                fillGeomRef = &dynItF->second.fillVertices;
            } else {
                if (invalidateCache && dynItF != m_dynamicStrokeCache.end()) {
                    m_dynamicStrokeCache.erase(keyFill);
                    m_dynamicStrokeCacheOrder.erase(std::ranges::remove(m_dynamicStrokeCacheOrder, keyFill).begin(),
                                                    m_dynamicStrokeCacheOrder.end());
                }

                fillVertices = generateFillGeometry(contours, info.fillColor, info.rounedJoint);
                if (!fillVertices.empty()) {
                    constexpr size_t kMaxDynEntries = 1024;
                    PathGeometryCacheEntry entry{};
                    entry.pathId = UUID{};
                    entry.strokeVertices = {};
                    entry.fillVertices = fillVertices;
                    entry.rounded = false;
                    m_dynamicStrokeCache[keyFill] = entry;
                    m_dynamicStrokeCacheOrder.push_back(keyFill);
                    if (m_dynamicStrokeCacheOrder.size() > kMaxDynEntries) {
                        uint64_t old = m_dynamicStrokeCacheOrder.front();
                        m_dynamicStrokeCacheOrder.pop_front();
                        m_dynamicStrokeCache.erase(old);
                    }
                    fillGeomRef = &m_dynamicStrokeCache[keyFill].fillVertices;
                }
            }

            if (fillGeomRef && !fillGeomRef->empty()) {
                uint64_t glyphId = info.glyphId;
                if (glyphId == 0) {
                    glyphId = reinterpret_cast<uint64_t>(this);
                }
                const auto key = PathGeometryCache::generateCacheKey(glyphId, info.scale, info.rounedJoint);
                auto found = m_glyphIdToMesh.find(key);

                if (found != m_glyphIdToMesh.end() && invalidateCache) {
                    m_glyphIdToMesh.erase(found);
                    m_glyphIdToInstances.erase(key);
                }

                if (found == m_glyphIdToMesh.end()) {
                    uint32_t firstIndex = static_cast<uint32_t>(m_fillIndices.size());
                    uint32_t baseVertex = static_cast<uint32_t>(m_fillVertices.size());
                    m_fillVertices.insert(m_fillVertices.end(), fillGeomRef->begin(), fillGeomRef->end());
                    auto localCount = static_cast<uint32_t>(fillGeomRef->size());
                    for (uint32_t i = 0; i < localCount; ++i)
                        m_fillIndices.emplace_back(baseVertex + i);
                    m_glyphIdToMesh[key] = {firstIndex, localCount};
                }
                m_glyphIdToInstances[key].emplace_back(
                    FillInstance{.translation = info.translate,
                                 .scale = info.scale,
                                 .color = info.fillColor,
                                 .pickId = encodeId(m_pathData.id)});
            }
        }

        addPathGeometries(strokeVertices);
    }

    void PathRenderer::beginFrame(VkCommandBuffer commandBuffer) {
        m_currentCommandBuffer = commandBuffer;
    }

    void PathRenderer::endFrame() {
        if (!m_pathData.ended) {
            endPathMode();
        }

        if (!m_fillVertices.empty() && !m_fillIndices.empty() && !m_glyphIdToMesh.empty()) {
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

            m_pathFillPipeline->beginPipeline(m_currentCommandBuffer, false);
            m_pathFillPipeline->setBatchedPathData(m_fillVertices, m_fillIndices, m_fillDrawCalls);
            m_pathFillPipeline->setInstanceData(m_tempInstances);
            m_pathFillPipeline->endPipeline();
        } else if (!m_fillVertices.empty() && !m_fillIndices.empty()) {
            m_tempInstances.clear();
            m_fillDrawCalls.clear();
            m_tempInstances.emplace_back(FillInstance{glm::vec3(0.0f), glm::vec2(1.0f), glm::vec4(1.f), encodeId(m_pathData.id)});
            Pipelines::PathFillPipeline::FillDrawCall dc{};
            dc.firstIndex = 0;
            dc.indexCount = static_cast<uint32_t>(m_fillIndices.size());
            dc.firstInstance = 0;
            dc.instanceCount = 1;
            m_fillDrawCalls.emplace_back(dc);

            m_pathFillPipeline->beginPipeline(m_currentCommandBuffer, false);
            m_pathFillPipeline->setBatchedPathData(m_fillVertices, m_fillIndices, m_fillDrawCalls);
            m_pathFillPipeline->setInstanceData(m_tempInstances);
            m_pathFillPipeline->endPipeline();
        }

        m_pathStrokePipeline->beginPipeline(m_currentCommandBuffer, false);
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
        m_pathData.points.emplace_back(PathPoint{startPos, weight, id});
        m_pathData.color = color;
        m_pathData.id = id;
    }

    void PathRenderer::endPathMode(bool closePath, bool genFill,
                                   const glm::vec4 &fillColor, bool genStroke,
                                   bool genRoundedJoints, bool invalidateCache) {
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
        info.rounedJoint = genRoundedJoints;
        info.strokeColor = m_pathData.color;
        info.translate = glm::vec3(0.0f);
        info.scale = glm::vec2(1.0f);
        info.glyphId = m_pathData.id;

        drawContours(m_pathData.contours, info, invalidateCache);

        m_pathData = {};
    }

    void PathRenderer::pathLineTo(const glm::vec3 &pos, float size, const glm::vec4 &color, uint64_t id) {
        if (m_pathData.ended)
            return;
        m_pathData.points.emplace_back(PathPoint{pos, size, id});
        m_pathData.setCurrentPos(pos);
    }

    void PathRenderer::pathCubicBeizerTo(const glm::vec3 &end, const glm::vec2 &controlPoint1, const glm::vec2 &controlPoint2,
                                         float weight, const glm::vec4 &color, uint64_t id) {
        if (m_pathData.ended)
            return;
        auto positions = generateCubicBezierPoints(m_pathData.currentPos, controlPoint1, controlPoint2, end);
        m_pathData.points.reserve(m_pathData.points.size() + positions.size());
        for (auto pos : positions) {
            m_pathData.points.emplace_back(PathPoint{pos, weight, id});
        }
        m_pathData.setCurrentPos(end);
    }

    void PathRenderer::pathQuadBeizerTo(const glm::vec3 &end, const glm::vec2 &controlPoint, float weight, const glm::vec4 &color, uint64_t id) {
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
                                                                   const glm::vec4 &color, bool isClosed,
                                                                   const bool rounedJoint) {
        if (points.size() < 2) {
            return {};
        }

        if (rounedJoint) {
            std::vector<PathPoint> smoothed;
            smoothed.reserve(points.size() * 3);

            auto pushPoint = [&](const glm::vec2 &p, float z, float w, uint64_t id) {
                smoothed.emplace_back(PathPoint{glm::vec3(p, z), w, id});
            };

            auto effectiveRadius = [&](const glm::vec2 &prevP, const glm::vec2 &currP, const glm::vec2 &nextP) -> float {
                float d1 = glm::distance(prevP, currP);
                float d2 = glm::distance(currP, nextP);
                float r = std::min(kRoundedJoinRadius, std::min(d1, d2) * 0.45f);
                return std::max(0.0f, r);
            };

            if (!isClosed) {
                smoothed.push_back(points.front());
                for (size_t i = 1; i + 1 < points.size(); i++) {
                    while (i + 1 < points.size() && glm::distance(glm::vec2(points[i - 1].pos), glm::vec2(points[i].pos)) < smoothed.back().weight) {
                        i++;
                    }
                    if (i + 1 >= points.size())
                        break;

                    const auto &prev = points[i - 1];
                    const auto &curr = points[i];
                    const auto &next = points[i + 1];

                    if ((prev.pos.y == curr.pos.y && curr.pos.y == next.pos.y) ||
                        (prev.pos.x == curr.pos.x && curr.pos.x == next.pos.x)) {
                        smoothed.push_back(curr);
                        continue;
                    }

                    float r = effectiveRadius(glm::vec2(prev.pos), glm::vec2(curr.pos), glm::vec2(next.pos));
                    if (r <= 0.0f) {
                        smoothed.push_back(curr);
                        continue;
                    }

                    auto bend = generateSmoothBendPoints(glm::vec2(prev.pos), glm::vec2(curr.pos), glm::vec2(next.pos), r);
                    glm::vec3 start3(bend.startPoint.x, bend.startPoint.y, curr.pos.z);
                    glm::vec3 end3(bend.endPoint.x, bend.endPoint.y, curr.pos.z);

                    pushPoint(bend.startPoint, curr.pos.z, curr.weight, curr.id);
                    auto samples = generateQuadBezierPoints(start3, bend.controlPoint, end3);
                    for (int i = 0; i < samples.size(); i++) {
                        auto &p = samples[i];
                        auto id = i < (samples.size() / 2) ? curr.id : next.id;
                        auto weight = i < (samples.size() / 2) ? curr.weight : next.weight;
                        smoothed.emplace_back(PathPoint{p, weight, id});
                    }
                }

                smoothed.push_back(points.back());
            } else {
                const size_t n = points.size();
                smoothed.reserve(n * 4);
                for (size_t i = 0; i < n; ++i) {
                    const auto &prev = points[(i + n - 1) % n];
                    const auto &curr = points[i];
                    const auto &next = points[(i + 1) % n];

                    float r = effectiveRadius(glm::vec2(prev.pos), glm::vec2(curr.pos), glm::vec2(next.pos));
                    if (r <= 0.0f) {
                        smoothed.push_back(curr);
                        continue;
                    }

                    auto bend = generateSmoothBendPoints(glm::vec2(prev.pos), glm::vec2(curr.pos), glm::vec2(next.pos), r);
                    glm::vec3 start3(bend.startPoint.x, bend.startPoint.y, curr.pos.z);
                    glm::vec3 end3(bend.endPoint.x, bend.endPoint.y, curr.pos.z);

                    pushPoint(bend.startPoint, curr.pos.z, curr.weight, curr.id);
                    auto samples = generateQuadBezierPoints(start3, bend.controlPoint, end3);
                    for (const auto &p : samples) {
                        smoothed.emplace_back(PathPoint{p, curr.weight, curr.id});
                    }
                }
            }

            return generateStrokeGeometry(smoothed, color, isClosed, false);
        }

        float t = 0;
        auto makeVertex = [&](const glm::vec2 &pos, float z, uint64_t id) {
            CommonVertex v;
            v.position = glm::vec3(pos, z);
            v.color = color;
            v.id = encodeId(id);
            v.texCoord = glm::mix(glm::vec2(0.f), glm::vec2(1.), t);
            v.texSlotIdx = -1;
            return v;
        };

        float maxWeight = 0.f;
        for (size_t i = 0; i < points.size() - 1; ++i) {
            maxWeight = std::max(maxWeight, points[i].weight);
        }

        float miterLimit = maxWeight * 1.5f;

        std::vector<CommonVertex> stripVertices;
        const size_t pointCount = points.size();
        const size_t n = isClosed ? pointCount + 1 : pointCount;

        size_t ni = 0;
        size_t skipped = 0;
        // --- 2. Generate Joins and Caps ---
        for (size_t i = 0; i < n; i = ni) {
            // skipping continuous small steps
            size_t idx = i % pointCount;
            ni = i + 1;
            while (ni < n && std::abs(glm::distance(points[ni % pointCount].pos, points[idx].pos)) < points[idx].weight / 2.f) {
                ni++;
            }

            skipped += (ni - i - 1);
            t = (float)i / (float)(n - skipped - 1);

            const auto &pCurr = points[idx];
            const auto &pNext = points[ni % pointCount];

            uint64_t segmentId = (!isClosed && i == n - 1) ? pCurr.id : pNext.id;

            // --- Handle Caps for Open Paths ---
            if (i == 0) { // Start Cap
                glm::vec2 dir = glm::normalize(glm::vec2(pNext.pos) - glm::vec2(pCurr.pos));
                glm::vec2 normalVec = glm::vec2(-dir.y, dir.x) * pCurr.weight / 2.f;
                stripVertices.push_back(makeVertex(glm::vec2(pCurr.pos) - normalVec, pCurr.pos.z, segmentId));
                stripVertices.push_back(makeVertex(glm::vec2(pCurr.pos) + normalVec, pCurr.pos.z, segmentId));
                continue;
            }

            const auto &pPrev = points[i - 1];

            if (i == n - 1) { // End Cap
                glm::vec2 dir = glm::normalize(glm::vec2(pCurr.pos) - glm::vec2(pPrev.pos));
                glm::vec2 normalVec = glm::vec2(-dir.y, dir.x) * pCurr.weight / 2.f;
                stripVertices.push_back(makeVertex(glm::vec2(pCurr.pos) - normalVec, pCurr.pos.z, segmentId));
                stripVertices.push_back(makeVertex(glm::vec2(pCurr.pos) + normalVec, pCurr.pos.z, segmentId));
                continue;
            }

            // --- Handle Interior and Closed Path Joins ---
            glm::vec2 dirIn = glm::normalize(glm::vec2(pCurr.pos) - glm::vec2(pPrev.pos));
            glm::vec2 dirOut = glm::normalize(glm::vec2(pNext.pos) - glm::vec2(pCurr.pos));
            glm::vec2 dirOutToPrev = glm::normalize(glm::vec2(pPrev.pos - pCurr.pos));
            glm::vec2 normalIn(-dirIn.y, dirIn.x);
            glm::vec2 normalOut(-dirOut.y, dirOut.x);
            float dotProduct = glm::dot(dirIn, dirOut);

            // straight lines (Just hacking it right now)
            if (std::abs(dotProduct) == 1.f ||
                (pPrev.pos.y == pCurr.pos.y && pCurr.pos.y == pNext.pos.y) ||
                (pPrev.pos.x == pCurr.pos.x && pCurr.pos.x == pNext.pos.x)) {
                glm::vec2 normal = normalIn * pCurr.weight / 2.f;
                stripVertices.push_back(makeVertex(glm::vec2(pCurr.pos) - normal, pCurr.pos.z, segmentId));
                stripVertices.push_back(makeVertex(glm::vec2(pCurr.pos) + normal, pCurr.pos.z, segmentId));
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
                    stripVertices.push_back(makeVertex(innerPrev, pCurr.pos.z, segmentId));
                    stripVertices.push_back(makeVertex(outerPrev, pCurr.pos.z, segmentId));

                    stripVertices.push_back(makeVertex(innerNext, pCurr.pos.z, segmentId));
                    stripVertices.push_back(makeVertex(outerNext, pCurr.pos.z, segmentId));
                } else {
                    auto D = glm::vec2(pCurr.pos) + disp;
                    auto E = glm::vec2(pCurr.pos) - disp;

                    if (crossProductZ > 0) { // Left turn
                        stripVertices.push_back(makeVertex(E, pCurr.pos.z, segmentId));
                        stripVertices.push_back(makeVertex(D, pCurr.pos.z, segmentId));
                    } else { // Right turn
                        stripVertices.push_back(makeVertex(D, pCurr.pos.z, segmentId));
                        stripVertices.push_back(makeVertex(E, pCurr.pos.z, segmentId));
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

    std::vector<CommonVertex> PathRenderer::generateFillGeometry(const std::vector<PathPoint> &points, const glm::vec4 &color, bool rounedJoint) {
        std::vector<std::vector<PathPoint>> contours;
        contours.push_back(points);
        return generateFillGeometry(contours, color, rounedJoint);
    }

    std::vector<CommonVertex> PathRenderer::generateFillGeometry(
        const std::vector<std::vector<PathPoint>> &contours,
        const glm::vec4 &color,
        bool rounedJoint) {
        std::vector<CommonVertex> out;
        if (contours.empty())
            return out;

        // 1) Optional contour smoothing with quadratic Bezier fillets
        std::vector<std::vector<PathPoint>> roundedContours;
        const auto &useContours = [&]() -> const std::vector<std::vector<PathPoint>> & {
            if (!rounedJoint)
                return contours;

            auto nearlyEqual = [](const glm::vec2 &a, const glm::vec2 &b) {
                return glm::length(a - b) < 1e-4f;
            };
            roundedContours.clear();
            roundedContours.reserve(contours.size());

            for (const auto &c : contours) {
                if (c.size() < 3) {
                    roundedContours.push_back(c);
                    continue;
                }

                std::vector<PathPoint> smoothed;
                smoothed.reserve(c.size() * 3);

                auto pushPoint = [&](const glm::vec2 &p, float z, float w, uint64_t id) {
                    smoothed.emplace_back(PathPoint{glm::vec3(p, z), w, id});
                };

                bool isClosed = nearlyEqual(glm::vec2(c.front().pos), glm::vec2(c.back().pos));

                if (!isClosed) {
                    smoothed.push_back(c.front());
                    for (size_t i = 1; i + 1 < c.size(); ++i) {
                        const auto &prev = c[i - 1];
                        const auto &curr = c[i];
                        const auto &next = c[i + 1];

                        glm::vec2 v1 = glm::normalize(glm::vec2(curr.pos) - glm::vec2(prev.pos));
                        glm::vec2 v2 = glm::normalize(glm::vec2(next.pos) - glm::vec2(curr.pos));
                        float len1 = glm::distance(glm::vec2(prev.pos), glm::vec2(curr.pos));
                        float len2 = glm::distance(glm::vec2(curr.pos), glm::vec2(next.pos));
                        float cosTheta = glm::clamp(glm::dot(-v1, v2), -1.0f, 1.0f);
                        float theta = std::acos(cosTheta);
                        if (theta < 1e-3f || std::abs(theta - glm::pi<float>()) < 1e-3f) {
                            smoothed.push_back(curr);
                            continue;
                        }

                        float r = std::min(kRoundedJoinRadius, std::min(len1, len2) * 0.45f);
                        float t = r / std::tan(theta * 0.5f);
                        t = std::min(t, std::min(len1, len2) - 1e-4f);
                        if (!(t > 0.0f)) {
                            smoothed.push_back(curr);
                            continue;
                        }

                        glm::vec2 start = glm::vec2(curr.pos) - v1 * t;
                        glm::vec2 end = glm::vec2(curr.pos) + v2 * t;

                        glm::vec2 bi = glm::normalize((-v1) + v2);
                        float sinHalf = std::sin(theta * 0.5f);
                        if (sinHalf < 1e-5f) {
                            smoothed.push_back(curr);
                            continue;
                        }
                        glm::vec2 center = glm::vec2(curr.pos) + bi * (r / sinHalf);

                        glm::vec2 from = glm::normalize(start - center);
                        glm::vec2 to = glm::normalize(end - center);
                        float a0 = std::atan2(from.y, from.x);
                        float a1 = std::atan2(to.y, to.x);
                        float da = a1 - a0;
                        auto wrap = [](float a) { while (a <= -glm::pi<float>()) a += 2*glm::pi<float>(); while (a > glm::pi<float>()) a -= 2*glm::pi<float>(); return a; };
                        da = wrap(da);
                        float crossZ = (v1.x * v2.y) - (v1.y * v2.x);
                        if (crossZ > 0 && da < 0)
                            da += 2 * glm::pi<float>();
                        if (crossZ < 0 && da > 0)
                            da -= 2 * glm::pi<float>();

                        // Insert trimmed start
                        pushPoint(start, curr.pos.z, curr.weight, curr.id);
                        // Sample arc from start to end along interior
                        float arcLen = std::abs(da) * r;
                        int segs = std::min(kMaxRoundedSegments, std::max(3, (int)std::ceil(arcLen / kRoundedJoinRadius)));
                        for (int s = 1; s <= segs; ++s) {
                            float tseg = (float)s / (float)segs;
                            float ang = a0 + (da * tseg);
                            glm::vec2 p = center + glm::vec2(std::cos(ang), std::sin(ang)) * r;
                            smoothed.emplace_back(PathPoint{glm::vec3(p, curr.pos.z), curr.weight, curr.id});
                        }
                    }
                    smoothed.push_back(c.back());
                } else {
                    const size_t n = c.size();
                    for (size_t i = 0; i < n; ++i) {
                        const auto &prev = c[(i + n - 1) % n];
                        const auto &curr = c[i];
                        const auto &next = c[(i + 1) % n];

                        glm::vec2 v1 = glm::normalize(glm::vec2(curr.pos) - glm::vec2(prev.pos));
                        glm::vec2 v2 = glm::normalize(glm::vec2(next.pos) - glm::vec2(curr.pos));
                        float len1 = glm::distance(glm::vec2(prev.pos), glm::vec2(curr.pos));
                        float len2 = glm::distance(glm::vec2(curr.pos), glm::vec2(next.pos));
                        float cosTheta = glm::clamp(glm::dot(-v1, v2), -1.0f, 1.0f);
                        float theta = std::acos(cosTheta);
                        if (theta < 1e-3f || std::abs(theta - glm::pi<float>()) < 1e-3f) {
                            smoothed.push_back(curr);
                            continue;
                        }

                        float r = std::min(kRoundedJoinRadius, std::min(len1, len2) * 0.45f);
                        float t = r / std::tan(theta * 0.5f);
                        t = std::min(t, std::min(len1, len2) - 1e-4f);
                        if (!(t > 0.0f)) {
                            smoothed.push_back(curr);
                            continue;
                        }

                        glm::vec2 start = glm::vec2(curr.pos) - v1 * t;
                        glm::vec2 end = glm::vec2(curr.pos) + v2 * t;

                        glm::vec2 bi = glm::normalize((-v1) + v2);
                        float sinHalf = std::sin(theta * 0.5f);
                        if (sinHalf < 1e-5f) {
                            smoothed.push_back(curr);
                            continue;
                        }
                        glm::vec2 center = glm::vec2(curr.pos) + bi * (r / sinHalf);

                        glm::vec2 from = glm::normalize(start - center);
                        glm::vec2 to = glm::normalize(end - center);
                        float a0 = std::atan2(from.y, from.x);
                        float a1 = std::atan2(to.y, to.x);
                        float da = a1 - a0;
                        auto wrap = [](float a) { while (a <= -glm::pi<float>()) a += 2*glm::pi<float>(); while (a > glm::pi<float>()) a -= 2*glm::pi<float>(); return a; };
                        da = wrap(da);
                        float crossZ = (v1.x * v2.y) - (v1.y * v2.x);
                        if (crossZ > 0 && da < 0)
                            da += 2 * glm::pi<float>();
                        if (crossZ < 0 && da > 0)
                            da -= 2 * glm::pi<float>();

                        pushPoint(start, curr.pos.z, curr.weight, curr.id);
                        float arcLen = std::abs(da) * r;
                        int segs = std::min(kMaxRoundedSegments, std::max(3, (int)std::ceil(arcLen / kRoundedJoinRadius)));
                        for (int s = 1; s <= segs; ++s) {
                            float tseg = (float)s / (float)segs;
                            float ang = a0 + (da * tseg);
                            glm::vec2 p = center + glm::vec2(std::cos(ang), std::sin(ang)) * r;
                            smoothed.emplace_back(PathPoint{glm::vec3(p, curr.pos.z), curr.weight, curr.id});
                        }
                    }
                }

                // Ensure closedness if original contour was closed
                if (isClosed && (smoothed.empty() || !nearlyEqual(glm::vec2(smoothed.front().pos), glm::vec2(smoothed.back().pos)))) {
                    smoothed.push_back(smoothed.front());
                }

                roundedContours.emplace_back(std::move(smoothed));
            }

            return roundedContours;
        }();

        // 2) Compute bounds from the chosen contours
        glm::vec2 minPt(std::numeric_limits<float>::max());
        glm::vec2 maxPt(-std::numeric_limits<float>::max());
        for (const auto &c : useContours) {
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

        for (const auto &c : useContours) {
            if (c.size() < 3)
                continue;
            std::vector<TESSreal> coords;
            coords.reserve(c.size() * 3);
            for (const auto &pt : c) {
                coords.push_back((TESSreal)pt.pos.x);
                coords.push_back((TESSreal)pt.pos.y);
                coords.push_back((TESSreal)pt.pos.z);
            }
            tessAddContour(tess, 3, coords.data(), sizeof(TESSreal) * 3, (int)c.size());
        }

        constexpr int polySize = 3;
        if (!tessTesselate(tess,
                           TESS_WINDING_NONZERO,
                           TESS_POLYGONS,
                           polySize, // number of indices per element
                           3,        // vertex size (x,y)
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
        for (const auto &c : useContours)
            for (const auto &p : c)
                flatPoints.push_back(&p);

        auto makeVertex = [&](int vi) -> CommonVertex {
            glm::vec3 pos(
                (float)verts[(vi * 3) + 0],
                (float)verts[(vi * 3) + 1],
                (float)verts[(vi * 3) + 2]);

            const int mapped = vindex ? vindex[vi] : -1;
            uint64_t id = 0;
            if (mapped >= 0 && mapped < (int)flatPoints.size()) {
                id = flatPoints[mapped]->id;
            } else if (!flatPoints.empty()) {
                id = flatPoints.front()->id;
            }

            CommonVertex v;
            v.position = pos;
            v.color = color;
            v.id = encodeId(id);
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

    std::vector<glm::vec3> PathRenderer::generateQuadBezierPointsSegments(const glm::vec3 &start, const glm::vec2 &controlPoint, const glm::vec3 &end, int segments) {
        std::vector<glm::vec3> points;
        segments = std::max(1, segments);
        points.reserve((size_t)segments);
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
            uint64_t pid = 0;
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
