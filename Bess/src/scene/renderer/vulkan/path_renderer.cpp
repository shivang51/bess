#include "scene/renderer/vulkan/path_renderer.h"
#include <algorithm>
#include <cmath>
#include <cstring>

namespace Bess::Renderer2D::Vulkan {

    PathRenderer::PathRenderer(const std::shared_ptr<VulkanDevice> &device,
                               const std::shared_ptr<VulkanOffscreenRenderPass> &renderPass,
                               VkExtent2D extent)
        : m_device(device), m_renderPass(renderPass), m_extent(extent) {
        m_pathPipeline = std::make_unique<Pipelines::PathPipeline>(device, renderPass, extent);
    }

    PathRenderer::~PathRenderer() = default;

    PathRenderer::PathRenderer(PathRenderer &&other) noexcept
        : m_device(std::move(other.m_device)),
          m_renderPass(std::move(other.m_renderPass)),
          m_extent(other.m_extent),
          m_pathPipeline(std::move(other.m_pathPipeline)),
          m_currentCommandBuffer(other.m_currentCommandBuffer),
          m_pathData(std::move(other.m_pathData)) {
        other.m_currentCommandBuffer = VK_NULL_HANDLE;
    }

    PathRenderer &PathRenderer::operator=(PathRenderer &&other) noexcept {
        if (this != &other) {
            m_device = std::move(other.m_device);
            m_renderPass = std::move(other.m_renderPass);
            m_extent = other.m_extent;
            m_pathPipeline = std::move(other.m_pathPipeline);
            m_currentCommandBuffer = other.m_currentCommandBuffer;
            m_pathData = std::move(other.m_pathData);

            other.m_currentCommandBuffer = VK_NULL_HANDLE;
        }
        return *this;
    }

    void PathRenderer::beginFrame(VkCommandBuffer commandBuffer) {
        m_currentCommandBuffer = commandBuffer;
        // Frame index is set by the parent system
    }

    void PathRenderer::endFrame() {
        // End any active path - this will render and clear the path data
        if (!m_pathData.ended) {
            endPathMode();
        }
    }

    void PathRenderer::setCurrentFrameIndex(uint32_t frameIndex) {
        m_pathPipeline->setCurrentFrameIndex(frameIndex);
    }

    void PathRenderer::beginPathMode(const glm::vec3 &startPos, float weight, const glm::vec4 &color, uint64_t id) {
        m_pathData.ended = false;
        m_pathData.currentPos = startPos;
        m_pathData.points.emplace_back(PathPoint{startPos, weight, id});
        m_pathData.color = color;
    }

    void PathRenderer::endPathMode(bool closePath, bool genFill, const glm::vec4 &fillColor, bool genStroke) {
        if (m_pathData.ended) return;

        std::vector<CommonVertex> strokeVertices;
        std::vector<CommonVertex> fillVertices;
        std::vector<uint32_t> strokeIndices;
        std::vector<uint32_t> fillIndices;

        if (genStroke) {
            strokeVertices = generateStrokeGeometry(m_pathData.points, m_pathData.color, 4.f, closePath);
            for (uint32_t i = 0; i < strokeVertices.size(); ++i) {
                strokeIndices.push_back(i);
            }
        }

        if (genFill) {
            if (closePath) {
                m_pathData.points.push_back(m_pathData.points.front());
            }
            fillVertices = generateFillGeometry(m_pathData.points, fillColor);
            for (uint32_t i = 0; i < fillVertices.size(); ++i) {
                fillIndices.push_back(i);
            }
        }

        // Render the path
        m_pathPipeline->beginPipeline(m_currentCommandBuffer);
        m_pathPipeline->setPathData(strokeVertices, strokeIndices, fillVertices, fillIndices);
        m_pathPipeline->endPipeline();

        // Reset path data for next path
        m_pathData.ended = true;
        m_pathData.points.clear();
        m_pathData.color = glm::vec4(1.f);
        m_pathData.currentPos = glm::vec3(0.f);
    }

    void PathRenderer::pathLineTo(const glm::vec3 &pos, float size, const glm::vec4 &color, int id) {
        if (m_pathData.ended) return;
        m_pathData.points.emplace_back(PathPoint{pos, size, static_cast<uint64_t>(id)});
        m_pathData.setCurrentPos(pos);
    }

    void PathRenderer::pathCubicBeizerTo(const glm::vec3 &end, const glm::vec2 &controlPoint1, const glm::vec2 &controlPoint2,
                                         float weight, const glm::vec4 &color, int id) {
        if (m_pathData.ended) return;
        auto positions = generateCubicBezierPoints(m_pathData.currentPos, controlPoint1, controlPoint2, end);
        m_pathData.points.reserve(m_pathData.points.size() + positions.size());
        for (auto pos : positions) {
            m_pathData.points.emplace_back(PathPoint{pos, weight, static_cast<uint64_t>(id)});
        }
        m_pathData.setCurrentPos(end);
    }

    void PathRenderer::pathQuadBeizerTo(const glm::vec3 &end, const glm::vec2 &controlPoint, float weight, const glm::vec4 &color, int id) {
        if (m_pathData.ended) return;
        auto positions = generateQuadBezierPoints(m_pathData.currentPos, controlPoint, end);
        m_pathData.points.reserve(m_pathData.points.size() + positions.size());
        for (auto pos : positions) {
            m_pathData.points.emplace_back(PathPoint{pos, weight, static_cast<uint64_t>(id)});
        }
        m_pathData.setCurrentPos(end);
    }

    void PathRenderer::updateUniformBuffer(const UniformBufferObject &ubo) {
        m_pathPipeline->updateUniformBuffer(ubo);
    }

    void PathRenderer::updateZoomUniformBuffer(float zoom) {
        // This would be implemented in the path pipeline
        // For now, we'll just pass it through
    }

    // Path generation functions (adapted from old GL renderer)
    std::vector<CommonVertex> PathRenderer::generateStrokeGeometry(const std::vector<PathPoint> &points,
                                                                   const glm::vec4 &color, float miterLimit, bool isClosed) {
        if (points.size() < (isClosed ? 3 : 2)) {
            return {};
        }

        auto makeVertex = [&](const glm::vec2 &pos, float z, uint64_t id, const glm::vec2 &uv) {
            CommonVertex v;
            v.position = glm::vec3(pos, z);
            v.color = color;
            v.id = static_cast<int>(id);
            v.texSlotIdx = 0;
            v.texCoord = uv;
            return v;
        };

        // Pre-calculate total path length for correct UV mapping
        float totalLength = 0.0f;
        for (size_t i = 0; i < points.size() - 1; ++i) {
            totalLength += glm::distance(glm::vec2(points[i].pos), glm::vec2(points[i + 1].pos));
        }

        if (isClosed) {
            totalLength += glm::distance(glm::vec2(points.back().pos), glm::vec2(points.front().pos));
        }

        std::vector<CommonVertex> stripVertices;
        float cumulativeLength = 0.0f;
        const size_t pointCount = points.size();
        const size_t n = pointCount;

        size_t ni = 0;
        // Generate Joins and Caps
        for (size_t i = 0; i < n; i = ni) {
            // skipping continuous same points
            ni = i + 1;
            while (ni < n && points[ni].pos == points[i].pos) {
                ni++;
            }

            const auto &pCurr = points[i];
            
            // Handle previous point access safely
            const PathPoint *pPrev = nullptr;
            if (isClosed) {
                pPrev = &points[(i + pointCount - 1) % pointCount];
            } else if (i > 0) {
                pPrev = &points[i - 1];
            }
            
            // Handle next point access safely
            const PathPoint *pNext = nullptr;
            if (isClosed) {
                pNext = &points[ni % pointCount];
            } else if (ni < pointCount) {
                pNext = &points[ni];
            }

            // Update cumulative length for UVs
            if (i > 0 && pPrev) {
                cumulativeLength += glm::distance(glm::vec2(pCurr.pos), glm::vec2(pPrev->pos));
            }
            float u = (totalLength > 0) ? (cumulativeLength / totalLength) : 0;

            // Handle Caps for Open Paths
            if (!isClosed && i == 0 && pNext) { // Start Cap
                glm::vec2 dir = glm::normalize(glm::vec2(pNext->pos) - glm::vec2(pCurr.pos));
                glm::vec2 normalVec = glm::vec2(-dir.y, dir.x) * pCurr.weight / 2.f;
                stripVertices.push_back(makeVertex(glm::vec2(pCurr.pos) - normalVec, pCurr.pos.z, pCurr.id, {u, 1.f}));
                stripVertices.push_back(makeVertex(glm::vec2(pCurr.pos) + normalVec, pCurr.pos.z, pCurr.id, {u, 0.f}));
                continue;
            }

            if (!isClosed && i == n - 1 && pPrev) { // End Cap
                glm::vec2 dir = glm::normalize(glm::vec2(pCurr.pos) - glm::vec2(pPrev->pos));
                glm::vec2 normalVec = glm::vec2(-dir.y, dir.x) * pCurr.weight / 2.f;
                stripVertices.push_back(makeVertex(glm::vec2(pCurr.pos) - normalVec, pCurr.pos.z, pCurr.id, {u, 1.f}));
                stripVertices.push_back(makeVertex(glm::vec2(pCurr.pos) + normalVec, pCurr.pos.z, pCurr.id, {u, 0.f}));
                continue;
            }

            // Handle Interior and Closed Path Joins
            if (!pPrev || !pNext) {
                continue; // Skip if we don't have both previous and next points
            }
            
            glm::vec2 dirIn = glm::normalize(glm::vec2(pCurr.pos) - glm::vec2(pPrev->pos));
            glm::vec2 dirOut = glm::normalize(glm::vec2(pNext->pos) - glm::vec2(pCurr.pos));
            glm::vec2 dirOutToPrev = glm::normalize(glm::vec2(pPrev->pos - pCurr.pos));
            glm::vec2 normalIn(-dirIn.y, dirIn.x);
            glm::vec2 normalOut(-dirOut.y, dirOut.x);
            float dotProduct = glm::dot(dirIn, dirOut);

            // Handle straight lines
            if (std::abs(dotProduct) == 1.f) {
                glm::vec2 normal = normalIn * pCurr.weight / 2.f;
                stripVertices.push_back(makeVertex(glm::vec2(pCurr.pos) - normal, pCurr.pos.z, pCurr.id, {u, 1.f}));
                stripVertices.push_back(makeVertex(glm::vec2(pCurr.pos) + normal, pCurr.pos.z, pCurr.id, {u, 0.f}));
                continue;
            }

            // Joint
            {
                auto uVec = dirOutToPrev;
                auto vVec = dirOut;
                float angle = std::acos(glm::dot(uVec, vVec));

                float uMag = pNext->weight / (2.0f * glm::sin(angle));
                float vMag = pCurr.weight / (2.0f * glm::sin(angle));

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
                        stripVertices.push_back(makeVertex(E, pCurr.pos.z, pCurr.id, {u, 1.f}));
                        stripVertices.push_back(makeVertex(D, pCurr.pos.z, pCurr.id, {u, 0.f}));
                    } else { // Right turn
                        stripVertices.push_back(makeVertex(D, pCurr.pos.z, pCurr.id, {u, 1.f}));
                        stripVertices.push_back(makeVertex(E, pCurr.pos.z, pCurr.id, {u, 0.f}));
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
        if (points.size() < 3) {
            return {};
        }

        std::vector<CommonVertex> fillVertices;

        std::vector<glm::vec2> polygonVertices;
        for (const auto &p : points) {
            if (!polygonVertices.empty() && glm::vec2(p.pos) == polygonVertices.back())
                continue;
            polygonVertices.push_back(glm::vec2(p.pos));
        }

        float area = 0.0f;
        for (size_t i = 0; i < polygonVertices.size(); ++i) {
            const auto &p1 = polygonVertices[i];
            const auto &p2 = polygonVertices[(i + 1) % polygonVertices.size()];
            area += (p1.x * p2.y - p2.x * p1.y);
        }

        if (area < 0) { // Ensure Counter-Clockwise winding for Y-up systems
            std::reverse(polygonVertices.begin(), polygonVertices.end());
        }

        glm::vec2 minBounds = polygonVertices[0];
        glm::vec2 maxBounds = polygonVertices[0];
        for (const auto &v : polygonVertices) {
            minBounds = glm::min(minBounds, v);
            maxBounds = glm::max(maxBounds, v);
        }
        glm::vec2 extent = maxBounds - minBounds;
        if (extent.x == 0)
            extent.x = 1;
        if (extent.y == 0)
            extent.y = 1;

        auto makeVertex = [&](const glm::vec2 &pos) {
            CommonVertex v;
            v.position = glm::vec3(pos, points[0].pos.z);
            v.color = color;
            v.id = static_cast<int>(points[0].id);
            v.texSlotIdx = 0;
            v.texCoord = (pos - minBounds) / extent;
            return v;
        };

        std::vector<int> indices;
        indices.reserve(polygonVertices.size());
        for (size_t i = 0; i < polygonVertices.size(); ++i) {
            indices.push_back(static_cast<int>(i));
        }

        int iterations = 0;
        const int maxIterations = static_cast<int>(indices.size()) + 5; // Failsafe for invalid polygons

        while (indices.size() > 2 && iterations++ < maxIterations) {
            bool earFound = false;
            for (size_t i = 0; i < indices.size(); ++i) {
                int prev_idx = indices[(i + indices.size() - 1) % indices.size()];
                int curr_idx = indices[i];
                int next_idx = indices[(i + 1) % indices.size()];

                const auto &a = polygonVertices[prev_idx];
                const auto &b = polygonVertices[curr_idx];
                const auto &c = polygonVertices[next_idx];

                if (cross_product(a, b, c) < 0) {
                    continue;
                }

                bool isEar = true;

                for (int p_idx : indices) {
                    if (p_idx == prev_idx || p_idx == curr_idx || p_idx == next_idx) {
                        continue;
                    }
                    const auto &p = polygonVertices[p_idx];

                    float d1 = cross_product(a, b, p);
                    float d2 = cross_product(b, c, p);
                    float d3 = cross_product(c, a, p);
                    if (d1 >= 0 && d2 >= 0 && d3 >= 0) {
                        isEar = false;
                        break;
                    }
                }

                if (isEar) {
                    fillVertices.push_back(makeVertex(a));
                    fillVertices.push_back(makeVertex(b));
                    fillVertices.push_back(makeVertex(c));
                    indices.erase(indices.begin() + static_cast<long>(i));
                    earFound = true;
                    break;
                }
            }
            if (!earFound) {
                break;
            }
        }

        return fillVertices;
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

    float PathRenderer::cross_product(const glm::vec2 &O, const glm::vec2 &A, const glm::vec2 &B) {
        return ((A.x - O.x) * (B.y - O.y)) - ((A.y - O.y) * (B.x - O.x));
    }

    int PathRenderer::calculateCubicBezierSegments(const glm::vec2 &p0,
                                                   const glm::vec2 &p1,
                                                   const glm::vec2 &p2,
                                                   const glm::vec2 &p3) {
        // Simplified calculation - in a real implementation, you'd use viewport size
        glm::vec2 d1 = p0 - 2.0f * p1 + p2;
        glm::vec2 d2 = p1 - 2.0f * p2 + p3;

        float m1 = glm::length(d1);
        float m2 = glm::length(d2);
        float M = 6.0f * std::max(m1, m2);

        float Nf = std::sqrt(M / (8.0f * 0.0001f)); // BEZIER_EPSILON

        return std::max(1, (int)std::ceil(Nf));
    }

    int PathRenderer::calculateQuadBezierSegments(const glm::vec2 &p0,
                                                  const glm::vec2 &p1,
                                                  const glm::vec2 &p2) {
        glm::vec2 d = p0 - 2.0f * p1 + p2;

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
        glm::vec2 point = (t_ * t_) * p0 + 2.0f * (t_ * t) * p1 + (t * t) * p2;
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

} // namespace Bess::Renderer2D::Vulkan
