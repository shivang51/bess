#include "bess_wgpu/path_baker.h"

#include "bess_core/renderer/renderer_types.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <numbers>
#include <span>
#include <vector>

namespace Bess::Wgpu {

    using Core::Renderer::Color;
    using Core::Renderer::PathCommand;
    using Core::Renderer::PathCommandKind;
    using Core::Renderer::PathCommandStroke;
    using Core::Renderer::PathFillRule;
    using Core::Renderer::PathLineCap;
    using Core::Renderer::PathLineJoin;
    using Core::Renderer::PathProps;
    using Core::Renderer::QuadRenderPass;
    using Core::Renderer::Renderer2DExtent;

    namespace {

        using Piplines::PathCoverVertex;
        using Piplines::PathStencilVertex;

        constexpr uint32_t kPathCurveTypeLine = 0;
        constexpr uint32_t kPathCurveTypeQuadratic = 1;

        struct StyledStrokeSegment {
            glm::vec2 from{0.f};
            glm::vec2 to{0.f};
            float fromHalfWidth = 0.5f;
            float toHalfWidth = 0.5f;
            PickingId id = PickingId::invalid();
        };

        bool hasPathFill(const PathProps &props);
        bool pathHasDrawableStroke(std::span<const PathCommand> commands,
                                   const PathProps &props);
        float strokeSizeForCommand(const PathCommand &command,
                                   const PathProps &props);
        PickingId pickingIdForCommand(const PathCommand &command,
                                      const PathProps &props);
        bool isFillTransparent(const PathProps &props);
        bool isStrokeTransparent(const PathProps &props);

        float halfWidthForCommand(const PathCommand &command,
                                  const PathProps &props,
                                  const PathBakeMetrics &metrics);
        void
        appendSolidStyledPolyline(std::vector<StyledStrokeSegment> &contour,
                                  const std::vector<glm::vec2> &points,
                                  float halfWidth, const PickingId &id);
        void appendDashedStyledPolyline(std::vector<PathCoverVertex> &vertices,
                                        const std::vector<glm::vec2> &points,
                                        float halfWidth, const PickingId &id,
                                        const PathCommandStroke &stroke,
                                        const PathProps &props,
                                        const StrokeMeshParams &mesh);

        bool hasPathFill(const PathProps &props) {
            return props.renderFill && props.fillColor.a > 0.f;
        }

        bool pathHasDrawableStroke(std::span<const PathCommand> commands,
                                   const PathProps &props) {
            if (props.strokeColor.a <= 0.f) {
                return false;
            }
            if (props.strokeSize > 0.f) {
                return true;
            }
            return std::any_of(commands.begin(), commands.end(),
                               [](const PathCommand &command) {
                                   return command.stroke.width > 0.f;
                               });
        }

        float strokeSizeForCommand(const PathCommand &command,
                                   const PathProps &props) {
            return command.stroke.width > 0.f ? command.stroke.width
                                              : props.strokeSize;
        }

        PickingId pickingIdForCommand(const PathCommand &command,
                                      const PathProps &props) {
            return command.stroke.hasIdOverride() ? command.stroke.id
                                                  : props.id;
        }

        bool commandNeedsStyledStrokeBaker(const PathCommand &command) {
            return command.kind != Core::Renderer::PathCommandKind::Move &&
                   command.stroke.isStyled();
        }

        bool pathNeedsStyledStrokeBaker(std::span<const PathCommand> commands,
                                        const PathProps &props) {
            if (props.strokeSize <= 0.f) {
                return true;
            }
            return std::any_of(commands.begin(), commands.end(),
                               commandNeedsStyledStrokeBaker);
        }

        bool isFillTransparent(const PathProps &props) {
            using Core::Renderer::QuadRenderPass;
            if (props.renderPass == QuadRenderPass::Opaque) {
                return false;
            }
            if (props.renderPass == QuadRenderPass::Transparent) {
                return true;
            }
            return props.fillColor.a < 0.999f;
        }

        bool isStrokeTransparent(const PathProps &props) {
            using Core::Renderer::QuadRenderPass;
            if (props.renderPass == QuadRenderPass::Opaque) {
                return false;
            }
            if (props.renderPass == QuadRenderPass::Transparent) {
                return true;
            }
            return props.strokeColor.a < 0.999f;
        }

        float signedArea2(const glm::vec2 &a, const glm::vec2 &b,
                          const glm::vec2 &c) {
            const glm::vec2 ab = b - a;
            const glm::vec2 ac = c - a;
            return (ab.x * ac.y) - (ab.y * ac.x);
        }

        bool nearlyDegenerateTriangle(const glm::vec2 &a, const glm::vec2 &b,
                                      const glm::vec2 &c) {
            return std::abs(signedArea2(a, b, c)) < 0.0001f;
        }

        void setStencilVertex(PathStencilVertex &vertex, const glm::vec2 &pos,
                              float z, const glm::vec2 &curveCoord,
                              uint32_t curveType) {
            vertex.position[0] = pos.x;
            vertex.position[1] = pos.y;
            vertex.position[2] = z;
            vertex.curveCoord[0] = curveCoord.x;
            vertex.curveCoord[1] = curveCoord.y;
            vertex.curveType = curveType;
        }

        void appendStencilTriangle(std::vector<PathStencilVertex> &vertices,
                                   const glm::vec2 &p0, const glm::vec2 &p1,
                                   const glm::vec2 &p2, float z,
                                   const glm::vec2 &c0, const glm::vec2 &c1,
                                   const glm::vec2 &c2, uint32_t curveType) {
            if (nearlyDegenerateTriangle(p0, p1, p2)) {
                return;
            }
            const size_t base = vertices.size();
            vertices.resize(base + 3);
            setStencilVertex(vertices[base + 0], p0, z, c0, curveType);
            setStencilVertex(vertices[base + 1], p1, z, c1, curveType);
            setStencilVertex(vertices[base + 2], p2, z, c2, curveType);
        }

        void appendLineAnchorTriangle(std::vector<PathStencilVertex> &vertices,
                                      const glm::vec2 &anchor,
                                      const glm::vec2 &from,
                                      const glm::vec2 &to, float z) {
            appendStencilTriangle(vertices, anchor, from, to, z, glm::vec2(0.f),
                                  glm::vec2(0.f), glm::vec2(0.f),
                                  kPathCurveTypeLine);
        }

        void appendQuadraticHull(std::vector<PathStencilVertex> &vertices,
                                 const glm::vec2 &from,
                                 const glm::vec2 &control, const glm::vec2 &to,
                                 float z) {
            appendStencilTriangle(vertices, from, control, to, z,
                                  glm::vec2(0.f, 0.f), glm::vec2(0.5f, 0.f),
                                  glm::vec2(1.f, 1.f), kPathCurveTypeQuadratic);
        }

        void growBounds(glm::vec2 &minPt, glm::vec2 &maxPt,
                        const glm::vec2 &p) {
            minPt = glm::min(minPt, p);
            maxPt = glm::max(maxPt, p);
        }

        void setCoverVertex(PathCoverVertex &vertex, const glm::vec2 &pos,
                            float z, const Core::Renderer::Color &color,
                            const PickingId &id) {
            vertex.position[0] = pos.x;
            vertex.position[1] = pos.y;
            vertex.position[2] = z;
            vertex.color[0] = color.r;
            vertex.color[1] = color.g;
            vertex.color[2] = color.b;
            vertex.color[3] = color.a;
            vertex.id[0] = id.runtimeId;
            vertex.id[1] = id.info;
        }

        std::array<PathCoverVertex, 6>
        makeCoverVertices(const glm::vec2 &minPt, const glm::vec2 &maxPt,
                          const PathProps &props) {
            std::array<PathCoverVertex, 6> vertices{};
            const glm::vec2 p0(minPt.x, minPt.y);
            const glm::vec2 p1(maxPt.x, minPt.y);
            const glm::vec2 p2(minPt.x, maxPt.y);
            const glm::vec2 p3(maxPt.x, maxPt.y);

            setCoverVertex(vertices[0], p0, props.zIndex, props.fillColor,
                           props.id);
            setCoverVertex(vertices[1], p1, props.zIndex, props.fillColor,
                           props.id);
            setCoverVertex(vertices[2], p2, props.zIndex, props.fillColor,
                           props.id);
            setCoverVertex(vertices[3], p2, props.zIndex, props.fillColor,
                           props.id);
            setCoverVertex(vertices[4], p1, props.zIndex, props.fillColor,
                           props.id);
            setCoverVertex(vertices[5], p3, props.zIndex, props.fillColor,
                           props.id);
            return vertices;
        }

        glm::vec2 evalQuadratic(const glm::vec2 &p0, const glm::vec2 &control,
                                const glm::vec2 &p1, float t) {
            const float invT = 1.f - t;
            return (invT * invT * p0) + (2.f * invT * t * control) +
                   (t * t * p1);
        }

        glm::vec2 evalCubic(const glm::vec2 &p0, const glm::vec2 &control1,
                            const glm::vec2 &control2, const glm::vec2 &p1,
                            float t) {
            const float invT = 1.f - t;
            const float invT2 = invT * invT;
            const float t2 = t * t;
            return (invT2 * invT * p0) + (3.f * invT2 * t * control1) +
                   (3.f * invT * t2 * control2) + (t2 * t * p1);
        }

        float curveTolerance(const PathProps &props,
                             const PathBakeMetrics &metrics) {
            return std::max(props.curveTolerance * metrics.pixelWorldSize,
                            0.001f);
        }

        int quadraticSegmentCount(const glm::vec2 &p0, const glm::vec2 &control,
                                  const glm::vec2 &p1, const PathProps &props,
                                  const PathBakeMetrics &metrics) {
            const float controlNet =
                glm::distance(p0, control) + glm::distance(control, p1);
            const float curvature = glm::length(p0 - (2.f * control) + p1);
            const float tolerance = curveTolerance(props, metrics);
            const float lengthSegments = std::ceil(controlNet / 24.f);
            const float curveSegments =
                std::ceil(std::sqrt(curvature / tolerance));
            return std::clamp(
                static_cast<int>(std::max(lengthSegments, curveSegments)), 1,
                128);
        }

        int cubicSegmentCount(const glm::vec2 &p0, const glm::vec2 &control1,
                              const glm::vec2 &control2, const glm::vec2 &p1,
                              const PathProps &props,
                              const PathBakeMetrics &metrics) {
            const float controlNet = glm::distance(p0, control1) +
                                     glm::distance(control1, control2) +
                                     glm::distance(control2, p1);
            const float curvature =
                std::max(glm::length(p0 - (2.f * control1) + control2),
                         glm::length(control1 - (2.f * control2) + p1));
            const float tolerance = curveTolerance(props, metrics);
            const float lengthSegments = std::ceil(controlNet / 18.f);
            const float curveSegments =
                std::ceil(std::sqrt((3.f * curvature) / tolerance));
            return std::clamp(
                static_cast<int>(std::max(lengthSegments, curveSegments)), 1,
                192);
        }

        glm::vec2 safeNormalize(const glm::vec2 &v) {
            const float len = glm::length(v);
            if (len < 0.0001f) {
                return {1.f, 0.f};
            }
            return v / len;
        }

        glm::vec2 perpendicular(const glm::vec2 &v) { return {-v.y, v.x}; }

        void appendStrokeVertex(std::vector<PathCoverVertex> &vertices,
                                const glm::vec2 &pos, const PathProps &props,
                                float alphaScale = 1.f) {
            auto &vertex = vertices.emplace_back();
            Core::Renderer::Color color = props.strokeColor;
            color.a *= std::clamp(alphaScale, 0.f, 1.f);
            setCoverVertex(vertex, pos, props.zIndex, color, props.id);
        }

        void appendStrokeTriangle(std::vector<PathCoverVertex> &vertices,
                                  const glm::vec2 &a, const glm::vec2 &b,
                                  const glm::vec2 &c, const PathProps &props,
                                  float alphaA = 1.f, float alphaB = 1.f,
                                  float alphaC = 1.f) {
            if (nearlyDegenerateTriangle(a, b, c)) {
                return;
            }
            appendStrokeVertex(vertices, a, props, alphaA);
            appendStrokeVertex(vertices, b, props, alphaB);
            appendStrokeVertex(vertices, c, props, alphaC);
        }

        void appendRoundCap(std::vector<PathCoverVertex> &vertices,
                            const glm::vec2 &center,
                            const glm::vec2 &capDirection,
                            const PathProps &props,
                            const StrokeMeshParams &mesh, float halfWidth) {
            constexpr float pi = std::numbers::pi_v<float>;
            const glm::vec2 dir = safeNormalize(capDirection);
            const float centerAngle = std::atan2(dir.y, dir.x);
            const int segments = std::clamp(
                static_cast<int>(
                    std::ceil(pi * halfWidth * mesh.metrics.screenScale / 4.f)),
                8, 96);

            glm::vec2 prevInner =
                center + glm::vec2(std::cos(centerAngle - (pi * 0.5f)),
                                   std::sin(centerAngle - (pi * 0.5f))) *
                             halfWidth;
            glm::vec2 prevOuter =
                center + glm::vec2(std::cos(centerAngle - (pi * 0.5f)),
                                   std::sin(centerAngle - (pi * 0.5f))) *
                             (halfWidth + mesh.fringe);
            for (int i = 1; i <= segments; ++i) {
                const float t =
                    static_cast<float>(i) / static_cast<float>(segments);
                const float angle = centerAngle - (pi * 0.5f) + (pi * t);
                glm::vec2 nextInner =
                    center +
                    glm::vec2(std::cos(angle), std::sin(angle)) * halfWidth;
                glm::vec2 nextOuter =
                    center + glm::vec2(std::cos(angle), std::sin(angle)) *
                                 (halfWidth + mesh.fringe);
                appendStrokeTriangle(vertices, center, prevInner, nextInner,
                                     props);
                appendStrokeTriangle(vertices, prevInner, prevOuter, nextInner,
                                     props, 1.f, 0.f, 1.f);
                appendStrokeTriangle(vertices, nextInner, prevOuter, nextOuter,
                                     props, 1.f, 0.f, 0.f);
                prevInner = nextInner;
                prevOuter = nextOuter;
            }
        }

        void appendRoundCap(std::vector<PathCoverVertex> &vertices,
                            const glm::vec2 &center,
                            const glm::vec2 &capDirection,
                            const PathProps &props,
                            const StrokeMeshParams &mesh) {
            appendRoundCap(vertices, center, capDirection, props, mesh,
                           mesh.halfWidth);
        }

        void appendRoundJoin(std::vector<PathCoverVertex> &vertices,
                             const glm::vec2 &center,
                             const glm::vec2 &prevOuterNormal,
                             const glm::vec2 &nextOuterNormal,
                             const PathProps &props,
                             const StrokeMeshParams &mesh) {
            const float a0 = std::atan2(prevOuterNormal.y, prevOuterNormal.x);
            float delta =
                std::atan2((prevOuterNormal.x * nextOuterNormal.y) -
                               (prevOuterNormal.y * nextOuterNormal.x),
                           glm::dot(prevOuterNormal, nextOuterNormal));
            if (std::abs(delta) < 0.0001f) {
                return;
            }

            const int segments = std::clamp(
                static_cast<int>(std::ceil(std::abs(delta) * mesh.halfWidth *
                                           mesh.metrics.screenScale / 4.f)),
                4, 96);
            glm::vec2 prevInner = center + prevOuterNormal * mesh.halfWidth;
            glm::vec2 prevOuter =
                center + prevOuterNormal * (mesh.halfWidth + mesh.fringe);
            for (int i = 1; i <= segments; ++i) {
                const float t =
                    static_cast<float>(i) / static_cast<float>(segments);
                const float angle = a0 + (delta * t);
                const glm::vec2 normal(std::cos(angle), std::sin(angle));
                glm::vec2 nextInner = center + normal * mesh.halfWidth;
                glm::vec2 nextOuter =
                    center + normal * (mesh.halfWidth + mesh.fringe);
                appendStrokeTriangle(vertices, center, prevInner, nextInner,
                                     props);
                appendStrokeTriangle(vertices, prevInner, prevOuter, nextInner,
                                     props, 1.f, 0.f, 1.f);
                appendStrokeTriangle(vertices, nextInner, prevOuter, nextOuter,
                                     props, 1.f, 0.f, 0.f);
                prevInner = nextInner;
                prevOuter = nextOuter;
            }
        }

        void appendRoundJoin(std::vector<PathCoverVertex> &vertices,
                             const glm::vec2 &center,
                             const glm::vec2 &prevOuterNormal,
                             const glm::vec2 &nextOuterNormal,
                             float prevHalfWidth, float nextHalfWidth,
                             const PathProps &props,
                             const StrokeMeshParams &mesh) {
            const float a0 = std::atan2(prevOuterNormal.y, prevOuterNormal.x);
            float delta =
                std::atan2((prevOuterNormal.x * nextOuterNormal.y) -
                               (prevOuterNormal.y * nextOuterNormal.x),
                           glm::dot(prevOuterNormal, nextOuterNormal));
            if (std::abs(delta) < 0.0001f) {
                return;
            }

            const float maxHalfWidth = std::max(prevHalfWidth, nextHalfWidth);
            const int segments = std::clamp(
                static_cast<int>(std::ceil(std::abs(delta) * maxHalfWidth *
                                           mesh.metrics.screenScale / 4.f)),
                4, 96);
            glm::vec2 prevInner = center + prevOuterNormal * prevHalfWidth;
            glm::vec2 prevOuter =
                center + prevOuterNormal * (prevHalfWidth + mesh.fringe);
            for (int i = 1; i <= segments; ++i) {
                const float t =
                    static_cast<float>(i) / static_cast<float>(segments);
                const float angle = a0 + (delta * t);
                const float halfWidth =
                    prevHalfWidth + ((nextHalfWidth - prevHalfWidth) * t);
                const glm::vec2 normal(std::cos(angle), std::sin(angle));
                glm::vec2 nextInner = center + normal * halfWidth;
                glm::vec2 nextOuter =
                    center + normal * (halfWidth + mesh.fringe);
                appendStrokeTriangle(vertices, center, prevInner, nextInner,
                                     props);
                appendStrokeTriangle(vertices, prevInner, prevOuter, nextInner,
                                     props, 1.f, 0.f, 1.f);
                appendStrokeTriangle(vertices, nextInner, prevOuter, nextOuter,
                                     props, 1.f, 0.f, 0.f);
                prevInner = nextInner;
                prevOuter = nextOuter;
            }
        }

        void appendStrokeJoin(std::vector<PathCoverVertex> &vertices,
                              const glm::vec2 &center, const glm::vec2 &prevDir,
                              const glm::vec2 &nextDir, const PathProps &props,
                              const StrokeMeshParams &mesh) {
            const float turn =
                (prevDir.x * nextDir.y) - (prevDir.y * nextDir.x);
            if (std::abs(turn) < 0.0001f) {
                return;
            }

            const glm::vec2 prevNormal = perpendicular(prevDir);
            const glm::vec2 nextNormal = perpendicular(nextDir);
            const glm::vec2 prevLeft = center + prevNormal * mesh.halfWidth;
            const glm::vec2 prevRight = center - prevNormal * mesh.halfWidth;
            const glm::vec2 nextLeft = center + nextNormal * mesh.halfWidth;
            const glm::vec2 nextRight = center - nextNormal * mesh.halfWidth;

            appendStrokeTriangle(vertices, center, prevLeft, nextLeft, props);
            appendStrokeTriangle(vertices, center, nextRight, prevRight, props);

            const float side = turn > 0.f ? -1.f : 1.f;
            const glm::vec2 prevOuterNormal = prevNormal * side;
            const glm::vec2 nextOuterNormal = nextNormal * side;
            const glm::vec2 prevOuter =
                center + prevOuterNormal * mesh.halfWidth;
            const glm::vec2 nextOuter =
                center + nextOuterNormal * mesh.halfWidth;

            if (props.lineJoin == Core::Renderer::PathLineJoin::Round) {
                appendRoundJoin(vertices, center, prevOuterNormal,
                                nextOuterNormal, props, mesh);
                return;
            }

            if (props.lineJoin == Core::Renderer::PathLineJoin::Miter) {
                glm::vec2 miter = prevOuterNormal + nextOuterNormal;
                if (glm::length(miter) > 0.0001f) {
                    miter = safeNormalize(miter);
                    const float denom = glm::dot(miter, nextOuterNormal);
                    if (std::abs(denom) > 0.0001f) {
                        const float miterLength = mesh.halfWidth / denom;
                        const float limit =
                            mesh.halfWidth * std::max(props.miterLimit, 1.f);
                        if (std::abs(miterLength) <= limit) {
                            const glm::vec2 miterPoint =
                                center + miter * miterLength;
                            appendStrokeTriangle(vertices, prevOuter,
                                                 miterPoint, nextOuter, props);
                            return;
                        }
                    }
                }
            }

            appendStrokeTriangle(vertices, center, prevOuter, nextOuter, props);
        }

        void appendStyledStrokeJoin(std::vector<PathCoverVertex> &vertices,
                                    const glm::vec2 &center,
                                    const glm::vec2 &prevDir,
                                    const glm::vec2 &nextDir,
                                    float prevHalfWidth, float nextHalfWidth,
                                    const PickingId &id, const PathProps &props,
                                    const StrokeMeshParams &mesh) {
            PathProps joinProps = props;
            joinProps.id = id;

            const float turn =
                (prevDir.x * nextDir.y) - (prevDir.y * nextDir.x);
            if (std::abs(turn) < 0.0001f) {
                return;
            }

            const glm::vec2 prevNormal = perpendicular(prevDir);
            const glm::vec2 nextNormal = perpendicular(nextDir);
            const glm::vec2 prevLeft = center + prevNormal * prevHalfWidth;
            const glm::vec2 prevRight = center - prevNormal * prevHalfWidth;
            const glm::vec2 nextLeft = center + nextNormal * nextHalfWidth;
            const glm::vec2 nextRight = center - nextNormal * nextHalfWidth;

            appendStrokeTriangle(vertices, center, prevLeft, nextLeft,
                                 joinProps);
            appendStrokeTriangle(vertices, center, nextRight, prevRight,
                                 joinProps);

            const float side = turn > 0.f ? -1.f : 1.f;
            const glm::vec2 prevOuterNormal = prevNormal * side;
            const glm::vec2 nextOuterNormal = nextNormal * side;
            const glm::vec2 prevOuter =
                center + prevOuterNormal * prevHalfWidth;
            const glm::vec2 nextOuter =
                center + nextOuterNormal * nextHalfWidth;

            if (props.lineJoin == Core::Renderer::PathLineJoin::Round) {
                appendRoundJoin(vertices, center, prevOuterNormal,
                                nextOuterNormal, prevHalfWidth, nextHalfWidth,
                                joinProps, mesh);
                return;
            }

            if (props.lineJoin == Core::Renderer::PathLineJoin::Miter &&
                std::abs(prevHalfWidth - nextHalfWidth) < 0.0001f) {
                glm::vec2 miter = prevOuterNormal + nextOuterNormal;
                if (glm::length(miter) > 0.0001f) {
                    miter = safeNormalize(miter);
                    const float denom = glm::dot(miter, nextOuterNormal);
                    if (std::abs(denom) > 0.0001f) {
                        const float miterLength = nextHalfWidth / denom;
                        const float limit =
                            nextHalfWidth * std::max(props.miterLimit, 1.f);
                        if (std::abs(miterLength) <= limit) {
                            const glm::vec2 miterPoint =
                                center + miter * miterLength;
                            appendStrokeTriangle(vertices, prevOuter,
                                                 miterPoint, nextOuter,
                                                 joinProps);
                            return;
                        }
                    }
                }
            }

            appendStrokeTriangle(vertices, center, prevOuter, nextOuter,
                                 joinProps);
        }

        void appendStrokeSegment(std::vector<PathCoverVertex> &vertices,
                                 const glm::vec2 &from, const glm::vec2 &to,
                                 const PathProps &props,
                                 const StrokeMeshParams &mesh, bool extendStart,
                                 bool extendEnd) {
            const glm::vec2 delta = to - from;
            if (glm::length(delta) < 0.0001f) {
                return;
            }

            const glm::vec2 dir = safeNormalize(delta);
            const glm::vec2 segmentFrom =
                from - dir * (extendStart ? mesh.overlap : 0.f);
            const glm::vec2 segmentTo =
                to + dir * (extendEnd ? mesh.overlap : 0.f);
            const glm::vec2 normal = perpendicular(dir);
            const glm::vec2 innerNormal = normal * mesh.halfWidth;
            const glm::vec2 outerNormal =
                normal * (mesh.halfWidth + mesh.fringe);
            const glm::vec2 leftFrom = segmentFrom + innerNormal;
            const glm::vec2 rightFrom = segmentFrom - innerNormal;
            const glm::vec2 leftTo = segmentTo + innerNormal;
            const glm::vec2 rightTo = segmentTo - innerNormal;
            const glm::vec2 outerLeftFrom = segmentFrom + outerNormal;
            const glm::vec2 outerLeftTo = segmentTo + outerNormal;
            const glm::vec2 outerRightFrom = segmentFrom - outerNormal;
            const glm::vec2 outerRightTo = segmentTo - outerNormal;

            appendStrokeTriangle(vertices, leftFrom, rightFrom, leftTo, props);
            appendStrokeTriangle(vertices, leftTo, rightFrom, rightTo, props);
            appendStrokeTriangle(vertices, leftFrom, outerLeftFrom, leftTo,
                                 props, 1.f, 0.f, 1.f);
            appendStrokeTriangle(vertices, leftTo, outerLeftFrom, outerLeftTo,
                                 props, 1.f, 0.f, 0.f);
            appendStrokeTriangle(vertices, rightFrom, rightTo, outerRightFrom,
                                 props, 1.f, 1.f, 0.f);
            appendStrokeTriangle(vertices, rightTo, outerRightTo,
                                 outerRightFrom, props, 1.f, 0.f, 0.f);
        }

        void appendStyledStrokeSegment(std::vector<PathCoverVertex> &vertices,
                                       const StyledStrokeSegment &segment,
                                       const PathProps &props,
                                       const StrokeMeshParams &mesh,
                                       bool extendStart, bool extendEnd) {
            PathProps segmentProps = props;
            segmentProps.id = segment.id;

            const glm::vec2 delta = segment.to - segment.from;
            if (glm::length(delta) < 0.0001f || segment.fromHalfWidth <= 0.f ||
                segment.toHalfWidth <= 0.f) {
                return;
            }

            const glm::vec2 dir = safeNormalize(delta);
            const glm::vec2 segmentFrom =
                segment.from - dir * (extendStart ? mesh.overlap : 0.f);
            const glm::vec2 segmentTo =
                segment.to + dir * (extendEnd ? mesh.overlap : 0.f);
            const glm::vec2 normal = perpendicular(dir);
            const glm::vec2 fromInnerNormal = normal * segment.fromHalfWidth;
            const glm::vec2 toInnerNormal = normal * segment.toHalfWidth;
            const glm::vec2 fromOuterNormal =
                normal * (segment.fromHalfWidth + mesh.fringe);
            const glm::vec2 toOuterNormal =
                normal * (segment.toHalfWidth + mesh.fringe);

            const glm::vec2 leftFrom = segmentFrom + fromInnerNormal;
            const glm::vec2 rightFrom = segmentFrom - fromInnerNormal;
            const glm::vec2 leftTo = segmentTo + toInnerNormal;
            const glm::vec2 rightTo = segmentTo - toInnerNormal;
            const glm::vec2 outerLeftFrom = segmentFrom + fromOuterNormal;
            const glm::vec2 outerLeftTo = segmentTo + fromOuterNormal;
            const glm::vec2 outerRightFrom = segmentFrom - fromOuterNormal;
            const glm::vec2 outerRightTo = segmentTo - fromOuterNormal;

            appendStrokeTriangle(vertices, leftFrom, rightFrom, leftTo,
                                 segmentProps);
            appendStrokeTriangle(vertices, leftTo, rightFrom, rightTo,
                                 segmentProps);
            appendStrokeTriangle(vertices, leftFrom, outerLeftFrom, leftTo,
                                 segmentProps, 1.f, 0.f, 1.f);
            appendStrokeTriangle(vertices, leftTo, outerLeftFrom, outerLeftTo,
                                 segmentProps, 1.f, 0.f, 0.f);
            appendStrokeTriangle(vertices, rightFrom, rightTo, outerRightFrom,
                                 segmentProps, 1.f, 1.f, 0.f);
            appendStrokeTriangle(vertices, rightTo, outerRightTo,
                                 outerRightFrom, segmentProps, 1.f, 0.f, 0.f);
        }

        void appendStrokeContour(std::vector<PathCoverVertex> &vertices,
                                 std::vector<glm::vec2> points, bool closed,
                                 const PathProps &props,
                                 const StrokeMeshParams &mesh) {
            constexpr float epsilon = 0.0001f;
            if (points.size() < 2) {
                return;
            }

            std::vector<glm::vec2> compacted;
            compacted.reserve(points.size());
            for (const auto &point : points) {
                if (compacted.empty() ||
                    glm::distance(compacted.back(), point) > epsilon) {
                    compacted.push_back(point);
                }
            }
            points = std::move(compacted);
            if (points.size() < 2) {
                return;
            }

            const bool explicitlyClosed =
                glm::distance(points.front(), points.back()) < epsilon;
            if (closed && explicitlyClosed) {
                points.pop_back();
            }
            if (closed && points.size() < 3) {
                closed = false;
            }

            const size_t count = points.size();

            glm::vec2 startCapCenter = points.front();
            glm::vec2 endCapCenter = points.back();
            glm::vec2 startDir = safeNormalize(points[1] - points[0]);
            glm::vec2 endDir =
                safeNormalize(points[count - 1] - points[count - 2]);

            if (!closed &&
                props.lineCap == Core::Renderer::PathLineCap::Square) {
                points.front() -= startDir * mesh.halfWidth;
                points.back() += endDir * mesh.halfWidth;
            }

            auto pointAt = [&](ptrdiff_t index) -> const glm::vec2 & {
                const auto wrapped = static_cast<size_t>(
                    (index + static_cast<ptrdiff_t>(count)) %
                    static_cast<ptrdiff_t>(count));
                return points[wrapped];
            };

            const size_t segmentCount = closed ? count : count - 1;
            for (size_t i = 0; i < segmentCount; ++i) {
                const size_t next = (i + 1) % count;
                const bool extendStart = closed || i > 0;
                const bool extendEnd = closed || i + 1 < segmentCount;
                appendStrokeSegment(vertices, points[i], points[next], props,
                                    mesh, extendStart, extendEnd);
            }

            const size_t joinCount = closed ? count : count > 2 ? count - 2 : 0;
            for (size_t join = 0; join < joinCount; ++join) {
                const size_t i = closed ? join : join + 1;
                const glm::vec2 prev = pointAt(static_cast<ptrdiff_t>(i) - 1);
                const glm::vec2 curr = points[i];
                const glm::vec2 next = pointAt(static_cast<ptrdiff_t>(i) + 1);
                appendStrokeJoin(vertices, curr, safeNormalize(curr - prev),
                                 safeNormalize(next - curr), props, mesh);
            }

            if (!closed &&
                props.lineCap == Core::Renderer::PathLineCap::Round) {
                appendRoundCap(vertices, startCapCenter, -startDir, props,
                               mesh);
                appendRoundCap(vertices, endCapCenter, endDir, props, mesh);
            }
        }

        void
        appendStyledStrokeContour(std::vector<PathCoverVertex> &vertices,
                                  std::vector<StyledStrokeSegment> segments,
                                  bool closed, const PathProps &props,
                                  const StrokeMeshParams &mesh) {
            constexpr float epsilon = 0.0001f;
            if (segments.empty()) {
                return;
            }

            std::vector<StyledStrokeSegment> compacted;
            compacted.reserve(segments.size());
            for (const auto &segment : segments) {
                if (glm::distance(segment.from, segment.to) <= epsilon ||
                    segment.fromHalfWidth <= 0.f ||
                    segment.toHalfWidth <= 0.f) {
                    continue;
                }
                compacted.push_back(segment);
            }
            segments = std::move(compacted);
            if (segments.empty()) {
                return;
            }

            if (closed && glm::distance(segments.back().to,
                                        segments.front().from) > epsilon) {
                segments.push_back(
                    {.from = segments.back().to,
                     .to = segments.front().from,
                     .fromHalfWidth = segments.back().toHalfWidth,
                     .toHalfWidth = segments.front().fromHalfWidth,
                     .id = segments.back().id});
            }

            if (closed && segments.size() < 2) {
                closed = false;
            }

            const glm::vec2 startCapCenter = segments.front().from;
            const glm::vec2 endCapCenter = segments.back().to;
            const glm::vec2 startDir =
                safeNormalize(segments.front().to - segments.front().from);
            const glm::vec2 endDir =
                safeNormalize(segments.back().to - segments.back().from);
            const float startHalfWidth = segments.front().fromHalfWidth;
            const float endHalfWidth = segments.back().toHalfWidth;

            if (!closed &&
                props.lineCap == Core::Renderer::PathLineCap::Square) {
                segments.front().from -= startDir * startHalfWidth;
                segments.back().to += endDir * endHalfWidth;
            }

            const size_t segmentCount = segments.size();
            for (size_t i = 0; i < segmentCount; ++i) {
                const bool extendStart = closed || i > 0;
                const bool extendEnd = closed || i + 1 < segmentCount;
                appendStyledStrokeSegment(vertices, segments[i], props, mesh,
                                          extendStart, extendEnd);
            }

            const size_t joinCount =
                closed ? segmentCount
                       : (segmentCount > 1 ? segmentCount - 1 : 0);
            for (size_t join = 0; join < joinCount; ++join) {
                const size_t prevIndex = join;
                const size_t nextIndex = (join + 1) % segmentCount;
                const StyledStrokeSegment &prev = segments[prevIndex];
                const StyledStrokeSegment &next = segments[nextIndex];
                if (glm::distance(prev.to, next.from) > epsilon) {
                    continue;
                }

                appendStyledStrokeJoin(
                    vertices, prev.to, safeNormalize(prev.to - prev.from),
                    safeNormalize(next.to - next.from), prev.toHalfWidth,
                    next.fromHalfWidth, next.id, props, mesh);
            }

            if (!closed &&
                props.lineCap == Core::Renderer::PathLineCap::Round) {
                PathProps startCapProps = props;
                startCapProps.id = segments.front().id;
                PathProps endCapProps = props;
                endCapProps.id = segments.back().id;
                appendRoundCap(vertices, startCapCenter, -startDir,
                               startCapProps, mesh, startHalfWidth);
                appendRoundCap(vertices, endCapCenter, endDir, endCapProps,
                               mesh, endHalfWidth);
            }
        }

        float halfWidthForCommand(const PathCommand &command,
                                  const PathProps &props,
                                  const PathBakeMetrics &metrics) {
            const float strokeSize = strokeSizeForCommand(command, props);
            if (strokeSize <= 0.f) {
                return 0.f;
            }

            const float requestedHalfWidth = strokeSize * 0.5f;
            const float pixelHalfWidth = metrics.pixelWorldSize * 0.5f;
            return std::max(requestedHalfWidth, pixelHalfWidth);
        }

        void
        appendSolidStyledPolyline(std::vector<StyledStrokeSegment> &contour,
                                  const std::vector<glm::vec2> &points,
                                  float halfWidth, const PickingId &id) {
            if (points.size() < 2 || halfWidth <= 0.f) {
                return;
            }

            for (size_t i = 1; i < points.size(); ++i) {
                contour.push_back({.from = points[i - 1],
                                   .to = points[i],
                                   .fromHalfWidth = halfWidth,
                                   .toHalfWidth = halfWidth,
                                   .id = id});
            }
        }

        void appendDashedStyledPolyline(
            std::vector<PathCoverVertex> &vertices,
            const std::vector<glm::vec2> &points, float halfWidth,
            const PickingId &id,
            const Core::Renderer::PathCommandStroke &stroke,
            const PathProps &props, const StrokeMeshParams &mesh) {
            constexpr float epsilon = 0.0001f;
            if (points.size() < 2 || halfWidth <= 0.f) {
                return;
            }

            const float dashLength = std::max(stroke.dashLength, 0.f);
            const float gapLength = std::max(stroke.gapLength, 0.f);
            const float patternLength = dashLength + gapLength;
            if (dashLength <= epsilon || gapLength <= epsilon ||
                patternLength <= epsilon) {
                std::vector<StyledStrokeSegment> solidContour;
                appendSolidStyledPolyline(solidContour, points, halfWidth, id);
                appendStyledStrokeContour(vertices, std::move(solidContour),
                                          false, props, mesh);
                return;
            }

            float phase = std::fmod(stroke.dashOffset, patternLength);
            if (phase < 0.f) {
                phase += patternLength;
            }
            bool drawing = phase < dashLength;
            float remaining =
                drawing ? dashLength - phase : patternLength - phase;
            if (remaining <= epsilon) {
                drawing = !drawing;
                remaining = drawing ? dashLength : gapLength;
            }

            std::vector<StyledStrokeSegment> dashContour;
            auto flushDash = [&]() {
                if (dashContour.empty()) {
                    return;
                }

                appendStyledStrokeContour(vertices, std::move(dashContour),
                                          false, props, mesh);
                dashContour.clear();
            };

            for (size_t i = 1; i < points.size(); ++i) {
                const glm::vec2 from = points[i - 1];
                const glm::vec2 to = points[i];
                const glm::vec2 delta = to - from;
                const float length = glm::length(delta);
                if (length <= epsilon) {
                    continue;
                }

                const glm::vec2 dir = delta / length;
                float consumed = 0.f;
                glm::vec2 cursor = from;
                while (consumed < length - epsilon) {
                    const float step = std::min(remaining, length - consumed);
                    const glm::vec2 next = cursor + (dir * step);
                    if (drawing && step > epsilon) {
                        dashContour.push_back({.from = cursor,
                                               .to = next,
                                               .fromHalfWidth = halfWidth,
                                               .toHalfWidth = halfWidth,
                                               .id = id});
                    }

                    consumed += step;
                    cursor = next;
                    remaining -= step;
                    if (remaining <= epsilon) {
                        if (drawing) {
                            flushDash();
                        }
                        drawing = !drawing;
                        remaining = drawing ? dashLength : gapLength;
                    }
                }
            }

            flushDash();
        }

        std::vector<PathCoverVertex>
        bakeStyledPathStroke(std::span<const PathCommand> commands,
                             const PathProps &props,
                             const PathBakeMetrics &metrics) {
            std::vector<PathCoverVertex> vertices;
            if (commands.empty() || !pathHasDrawableStroke(commands, props)) {
                return vertices;
            }

            const StrokeMeshParams mesh = makeStrokeMeshParams(props, metrics);
            std::vector<StyledStrokeSegment> contour;
            std::vector<glm::vec2> polyline;
            glm::vec2 current(0.f);
            glm::vec2 contourStart(0.f);
            bool contourOpen = false;

            auto flushContour = [&](bool closed) {
                if (contour.empty()) {
                    return;
                }

                appendStyledStrokeContour(vertices, std::move(contour), closed,
                                          props, mesh);
                contour.clear();
            };

            auto emitPolyline = [&](const PathCommand &command,
                                    const std::vector<glm::vec2> &points) {
                if (command.stroke.breakBefore) {
                    flushContour(false);
                }

                const float halfWidth =
                    halfWidthForCommand(command, props, metrics);
                if (halfWidth <= 0.f) {
                    flushContour(false);
                    return;
                }

                if (command.stroke.isDashed()) {
                    flushContour(false);
                    appendDashedStyledPolyline(
                        vertices, points, halfWidth,
                        pickingIdForCommand(command, props), command.stroke,
                        props, mesh);
                    return;
                }

                appendSolidStyledPolyline(contour, points, halfWidth,
                                          pickingIdForCommand(command, props));
                if (command.stroke.breakAfter) {
                    flushContour(false);
                }
            };

            auto startImplicitContour = [&]() {
                if (contourOpen) {
                    return;
                }

                contourStart = current;
                contourOpen = true;
            };

            for (const auto &cmd : commands) {
                switch (cmd.kind) {
                case Core::Renderer::PathCommandKind::Move:
                    flushContour(props.closePath);
                    current = cmd.p;
                    contourStart = cmd.p;
                    contourOpen = true;
                    break;
                case Core::Renderer::PathCommandKind::Line:
                    startImplicitContour();
                    polyline.clear();
                    polyline.push_back(current);
                    polyline.push_back(cmd.p);
                    emitPolyline(cmd, polyline);
                    current = cmd.p;
                    break;
                case Core::Renderer::PathCommandKind::Quad:
                    startImplicitContour();
                    polyline.clear();
                    polyline.push_back(current);
                    if (nearlyDegenerateTriangle(current, cmd.control, cmd.p)) {
                        polyline.push_back(cmd.p);
                    } else {
                        const int segments = quadraticSegmentCount(
                            current, cmd.control, cmd.p, props, metrics);
                        for (int i = 1; i <= segments; ++i) {
                            const float t = static_cast<float>(i) /
                                            static_cast<float>(segments);
                            polyline.push_back(
                                evalQuadratic(current, cmd.control, cmd.p, t));
                        }
                    }
                    emitPolyline(cmd, polyline);
                    current = cmd.p;
                    break;
                case Core::Renderer::PathCommandKind::Cubic:
                    startImplicitContour();
                    polyline.clear();
                    polyline.push_back(current);
                    {
                        const int segments = cubicSegmentCount(
                            current, cmd.control, cmd.control2, cmd.p, props,
                            metrics);
                        for (int i = 1; i <= segments; ++i) {
                            const float t = static_cast<float>(i) /
                                            static_cast<float>(segments);
                            polyline.push_back(evalCubic(
                                current, cmd.control, cmd.control2, cmd.p, t));
                        }
                    }
                    emitPolyline(cmd, polyline);
                    current = cmd.p;
                    break;
                case Core::Renderer::PathCommandKind::Close:
                    if (contourOpen) {
                        polyline.clear();
                        polyline.push_back(current);
                        polyline.push_back(contourStart);
                        emitPolyline(cmd, polyline);
                        current = contourStart;
                        flushContour(!cmd.stroke.breakBefore &&
                                     !cmd.stroke.breakAfter &&
                                     !cmd.stroke.isDashed());
                    }
                    contourOpen = false;
                    break;
                }
            }

            flushContour(props.closePath);
            return vertices;
        }

        std::vector<PathCoverVertex>
        bakePathStroke(std::span<const PathCommand> commands,
                       const PathProps &props, const PathBakeMetrics &metrics) {
            std::vector<PathCoverVertex> vertices;
            if (commands.empty() || !pathHasDrawableStroke(commands, props)) {
                return vertices;
            }
            if (pathNeedsStyledStrokeBaker(commands, props)) {
                return bakeStyledPathStroke(commands, props, metrics);
            }

            const StrokeMeshParams mesh = makeStrokeMeshParams(props, metrics);
            std::vector<glm::vec2> contour;
            glm::vec2 current(0.f);

            auto flushContour = [&](bool closed) {
                if (contour.empty()) {
                    return;
                }

                appendStrokeContour(vertices, std::move(contour), closed, props,
                                    mesh);
                contour.clear();
            };

            for (const auto &cmd : commands) {
                switch (cmd.kind) {
                case Core::Renderer::PathCommandKind::Move:
                    flushContour(props.closePath);
                    contour.push_back(cmd.p);
                    current = cmd.p;
                    break;
                case Core::Renderer::PathCommandKind::Line:
                    if (contour.empty()) {
                        contour.push_back(current);
                    }
                    contour.push_back(cmd.p);
                    current = cmd.p;
                    break;
                case Core::Renderer::PathCommandKind::Quad:
                    if (contour.empty()) {
                        contour.push_back(current);
                    }
                    if (nearlyDegenerateTriangle(current, cmd.control, cmd.p)) {
                        contour.push_back(cmd.p);
                    } else {
                        const int segments = quadraticSegmentCount(
                            current, cmd.control, cmd.p, props, metrics);
                        for (int i = 1; i <= segments; ++i) {
                            const float t = static_cast<float>(i) /
                                            static_cast<float>(segments);
                            contour.push_back(
                                evalQuadratic(current, cmd.control, cmd.p, t));
                        }
                    }
                    current = cmd.p;
                    break;
                case Core::Renderer::PathCommandKind::Cubic:
                    if (contour.empty()) {
                        contour.push_back(current);
                    }
                    {
                        const int segments = cubicSegmentCount(
                            current, cmd.control, cmd.control2, cmd.p, props,
                            metrics);
                        for (int i = 1; i <= segments; ++i) {
                            const float t = static_cast<float>(i) /
                                            static_cast<float>(segments);
                            contour.push_back(evalCubic(
                                current, cmd.control, cmd.control2, cmd.p, t));
                        }
                    }
                    current = cmd.p;
                    break;
                case Core::Renderer::PathCommandKind::Close:
                    if (!contour.empty()) {
                        current = contour.front();
                    }
                    flushContour(true);
                    break;
                }
            }

            flushContour(props.closePath);
            return vertices;
        }

    } // namespace
    std::vector<Piplines::PathCoverVertex>
    bakePathFillAntiAlias(std::span<const PathCommand> commands,
                          const PathProps &props,
                          const PathBakeMetrics &metrics, float fringeScale) {
        std::vector<PathCoverVertex> vertices;
        if (commands.empty() || !hasPathFill(props) || fringeScale <= 0.f) {
            return vertices;
        }

        PathProps fringeProps = props;
        fringeProps.strokeColor = props.fillColor;
        fringeProps.lineJoin = Core::Renderer::PathLineJoin::Round;
        fringeProps.lineCap = Core::Renderer::PathLineCap::Round;

        const StrokeMeshParams mesh{
            .metrics = metrics,
            .halfWidth = 0.f,
            .fringe = std::max(metrics.pixelWorldSize * fringeScale, 0.0001f),
            .overlap = 0.f};

        std::vector<glm::vec2> contour;
        glm::vec2 current(0.f);

        auto flushContour = [&](bool closed) {
            if (contour.empty()) {
                return;
            }

            appendStrokeContour(vertices, std::move(contour), closed,
                                fringeProps, mesh);
            contour.clear();
        };

        for (const auto &cmd : commands) {
            switch (cmd.kind) {
            case Core::Renderer::PathCommandKind::Move:
                flushContour(props.closePath);
                contour.push_back(cmd.p);
                current = cmd.p;
                break;
            case Core::Renderer::PathCommandKind::Line:
                if (contour.empty()) {
                    contour.push_back(current);
                }
                contour.push_back(cmd.p);
                current = cmd.p;
                break;
            case Core::Renderer::PathCommandKind::Quad:
                if (contour.empty()) {
                    contour.push_back(current);
                }
                if (nearlyDegenerateTriangle(current, cmd.control, cmd.p)) {
                    contour.push_back(cmd.p);
                } else {
                    const int segments = quadraticSegmentCount(
                        current, cmd.control, cmd.p, props, metrics);
                    for (int i = 1; i <= segments; ++i) {
                        const float t = static_cast<float>(i) /
                                        static_cast<float>(segments);
                        contour.push_back(
                            evalQuadratic(current, cmd.control, cmd.p, t));
                    }
                }
                current = cmd.p;
                break;
            case Core::Renderer::PathCommandKind::Cubic:
                if (contour.empty()) {
                    contour.push_back(current);
                }
                {
                    const int segments =
                        cubicSegmentCount(current, cmd.control, cmd.control2,
                                          cmd.p, props, metrics);
                    for (int i = 1; i <= segments; ++i) {
                        const float t = static_cast<float>(i) /
                                        static_cast<float>(segments);
                        contour.push_back(evalCubic(current, cmd.control,
                                                    cmd.control2, cmd.p, t));
                    }
                }
                current = cmd.p;
                break;
            case Core::Renderer::PathCommandKind::Close:
                if (!contour.empty()) {
                    current = contour.front();
                }
                flushContour(true);
                break;
            }
        }

        flushContour(props.closePath);
        return vertices;
    }

    BakedPath bakePath(std::span<const PathCommand> commands,
                       const PathProps &props, const PathBakeMetrics &metrics) {
        BakedPath baked{};
        if (commands.empty() || !hasPathFill(props)) {
            return baked;
        }

        glm::vec2 minPt(std::numeric_limits<float>::max());
        glm::vec2 maxPt(-std::numeric_limits<float>::max());
        bool hasBounds = false;
        auto recordPoint = [&](const glm::vec2 &p) {
            growBounds(minPt, maxPt, p);
            hasBounds = true;
        };

        bool contourOpen = false;
        glm::vec2 anchor(0.f);
        glm::vec2 current(0.f);

        auto closeContour = [&](bool explicitClose) {
            if (!contourOpen || (!explicitClose && !props.closePath)) {
                return;
            }
            appendLineAnchorTriangle(baked.stencilVertices, anchor, current,
                                     anchor, props.zIndex);
            current = anchor;
        };

        for (const auto &cmd : commands) {
            switch (cmd.kind) {
            case Core::Renderer::PathCommandKind::Move:
                closeContour(false);
                anchor = cmd.p;
                current = cmd.p;
                contourOpen = true;
                recordPoint(cmd.p);
                break;
            case Core::Renderer::PathCommandKind::Line:
                if (!contourOpen) {
                    anchor = current;
                    contourOpen = true;
                    recordPoint(anchor);
                }
                appendLineAnchorTriangle(baked.stencilVertices, anchor, current,
                                         cmd.p, props.zIndex);
                current = cmd.p;
                recordPoint(cmd.p);
                break;
            case Core::Renderer::PathCommandKind::Quad:
                if (!contourOpen) {
                    anchor = current;
                    contourOpen = true;
                    recordPoint(anchor);
                }
                if (nearlyDegenerateTriangle(current, cmd.control, cmd.p)) {
                    appendLineAnchorTriangle(baked.stencilVertices, anchor,
                                             current, cmd.p, props.zIndex);
                } else {
                    appendLineAnchorTriangle(baked.stencilVertices, anchor,
                                             current, cmd.p, props.zIndex);
                    appendQuadraticHull(baked.stencilVertices, current,
                                        cmd.control, cmd.p, props.zIndex);
                }
                current = cmd.p;
                recordPoint(cmd.control);
                recordPoint(cmd.p);
                break;
            case Core::Renderer::PathCommandKind::Cubic:
                if (!contourOpen) {
                    anchor = current;
                    contourOpen = true;
                    recordPoint(anchor);
                }
                {
                    const int segments =
                        cubicSegmentCount(current, cmd.control, cmd.control2,
                                          cmd.p, props, metrics);
                    glm::vec2 prev = current;
                    for (int i = 1; i <= segments; ++i) {
                        const float t = static_cast<float>(i) /
                                        static_cast<float>(segments);
                        glm::vec2 next = evalCubic(current, cmd.control,
                                                   cmd.control2, cmd.p, t);
                        appendLineAnchorTriangle(baked.stencilVertices, anchor,
                                                 prev, next, props.zIndex);
                        prev = next;
                        recordPoint(next);
                    }
                }
                current = cmd.p;
                recordPoint(cmd.control);
                recordPoint(cmd.control2);
                recordPoint(cmd.p);
                break;
            case Core::Renderer::PathCommandKind::Close:
                closeContour(true);
                contourOpen = false;
                break;
            }
        }

        closeContour(false);

        if (!hasBounds || baked.stencilVertices.empty()) {
            return baked;
        }

        constexpr float coverPadding = 1.f;
        minPt -= glm::vec2(coverPadding);
        maxPt += glm::vec2(coverPadding);
        if (maxPt.x <= minPt.x || maxPt.y <= minPt.y) {
            return baked;
        }

        baked.coverVertices = makeCoverVertices(minPt, maxPt, props);
        baked.evenOddFill =
            props.fillRule == Core::Renderer::PathFillRule::EvenOdd;
        baked.valid = true;
        return baked;
    }

    void PathBatch::clear() {
        m_stencilVertices.clear();
        m_coverVertices.clear();
        m_drawRanges.clear();
    }

    void PathBatch::push(BakedPath &&path, float zIndex) {
        if (!path.valid || path.stencilVertices.empty()) {
            return;
        }

        PathDrawRange range{};
        range.firstStencilVertex =
            static_cast<uint32_t>(m_stencilVertices.size());
        range.stencilVertexCount =
            static_cast<uint32_t>(path.stencilVertices.size());
        range.firstCoverVertex = static_cast<uint32_t>(m_coverVertices.size());
        range.coverVertexCount =
            static_cast<uint32_t>(path.coverVertices.size());
        range.evenOddFill = path.evenOddFill;
        range.zIndex = zIndex;

        m_stencilVertices.insert(m_stencilVertices.end(),
                                 path.stencilVertices.begin(),
                                 path.stencilVertices.end());
        m_coverVertices.insert(m_coverVertices.end(),
                               path.coverVertices.begin(),
                               path.coverVertices.end());
        m_drawRanges.push_back(range);
    }

    void PathBatch::prepareForRendering(bool sortBackToFront) {
        if (!sortBackToFront || m_drawRanges.size() <= 1) {
            return;
        }

        std::stable_sort(m_drawRanges.begin(), m_drawRanges.end(),
                         [](const PathDrawRange &a, const PathDrawRange &b) {
                             return a.zIndex < b.zIndex;
                         });
    }

    bool PathBatch::empty() const noexcept { return m_drawRanges.empty(); }

    uint32_t PathBatch::drawCount() const noexcept {
        return static_cast<uint32_t>(m_drawRanges.size());
    }

    uint32_t PathBatch::stencilVertexCount() const noexcept {
        return static_cast<uint32_t>(m_stencilVertices.size());
    }

    uint32_t PathBatch::coverVertexCount() const noexcept {
        return static_cast<uint32_t>(m_coverVertices.size());
    }

    uint64_t PathBatch::stencilByteSize() const noexcept {
        return static_cast<uint64_t>(m_stencilVertices.size()) *
               sizeof(Piplines::PathStencilVertex);
    }

    uint64_t PathBatch::coverByteSize() const noexcept {
        return static_cast<uint64_t>(m_coverVertices.size()) *
               sizeof(Piplines::PathCoverVertex);
    }

    const Piplines::PathStencilVertex *PathBatch::stencilData() const noexcept {
        return m_stencilVertices.data();
    }

    const Piplines::PathCoverVertex *PathBatch::coverData() const noexcept {
        return m_coverVertices.data();
    }

    const PathDrawRange *PathBatch::drawRanges() const noexcept {
        return m_drawRanges.data();
    }

    void PathStrokeBatch::clear() {
        m_vertices.clear();
        m_drawRanges.clear();
    }

    void
    PathStrokeBatch::push(std::vector<Piplines::PathCoverVertex> &&vertices,
                          float zIndex) {
        if (vertices.empty()) {
            return;
        }

        PathStrokeDrawRange range{};
        range.firstVertex = static_cast<uint32_t>(m_vertices.size());
        range.vertexCount = static_cast<uint32_t>(vertices.size());
        range.zIndex = zIndex;
        m_vertices.insert(m_vertices.end(), vertices.begin(), vertices.end());
        m_drawRanges.push_back(range);
    }

    void PathStrokeBatch::prepareForRendering(bool sortBackToFront) {
        if (!sortBackToFront || m_drawRanges.size() <= 1) {
            return;
        }

        std::stable_sort(
            m_drawRanges.begin(), m_drawRanges.end(),
            [](const PathStrokeDrawRange &a, const PathStrokeDrawRange &b) {
                return a.zIndex < b.zIndex;
            });
    }

    bool PathStrokeBatch::empty() const noexcept {
        return m_drawRanges.empty();
    }

    uint32_t PathStrokeBatch::drawCount() const noexcept {
        return static_cast<uint32_t>(m_drawRanges.size());
    }

    uint32_t PathStrokeBatch::vertexCount() const noexcept {
        return static_cast<uint32_t>(m_vertices.size());
    }

    uint64_t PathStrokeBatch::byteSize() const noexcept {
        return static_cast<uint64_t>(m_vertices.size()) *
               sizeof(Piplines::PathCoverVertex);
    }

    const Piplines::PathCoverVertex *PathStrokeBatch::data() const noexcept {
        return m_vertices.data();
    }

    const PathStrokeDrawRange *PathStrokeBatch::drawRanges() const noexcept {
        return m_drawRanges.data();
    }

    StrokeMeshParams makeStrokeMeshParams(const PathProps &props,
                                          const PathBakeMetrics &metrics) {
        const float requestedHalfWidth =
            std::max(props.strokeSize * 0.5f, 0.0001f);
        const float pixelHalfWidth = metrics.pixelWorldSize * 0.5f;
        return {.metrics = metrics,
                .halfWidth = std::max(requestedHalfWidth, pixelHalfWidth),
                .fringe = std::max(metrics.pixelWorldSize * 0.75f,
                                   requestedHalfWidth * 0.02f),
                .overlap = metrics.pixelWorldSize * 0.75f};
    }

    PathBakeMetrics makePathBakeMetrics(const float *cameraTransform,
                                        const Renderer2DExtent &extent) {
        if (cameraTransform == nullptr || extent.width == 0 ||
            extent.height == 0) {
            return {};
        }

        const float viewportWidth = static_cast<float>(extent.width);
        const float viewportHeight = static_cast<float>(extent.height);
        const glm::vec2 xAxis(cameraTransform[0] * viewportWidth * 0.5f,
                              cameraTransform[1] * viewportHeight * 0.5f);
        const glm::vec2 yAxis(cameraTransform[4] * viewportWidth * 0.5f,
                              cameraTransform[5] * viewportHeight * 0.5f);
        const float screenScale =
            std::max({glm::length(xAxis), glm::length(yAxis), 0.0001f});
        return {.screenScale = screenScale,
                .pixelWorldSize = 1.f / screenScale};
    }

    void submitPathCommands(std::span<const PathCommand> commands,
                            const PathProps &props,
                            const PathBakeMetrics &metrics,
                            PathBatch &opaquePathBatch,
                            PathBatch &transparentPathBatch,
                            PathStrokeBatch &opaquePathStrokeBatch,
                            PathStrokeBatch &transparentPathStrokeBatch) {
        if (commands.empty()) {
            return;
        }

        if (hasPathFill(props)) {
            BakedPath baked = bakePath(commands, props, metrics);
            if (baked.valid) {
                if (isFillTransparent(props)) {
                    transparentPathBatch.push(std::move(baked), props.zIndex);
                } else {
                    opaquePathBatch.push(std::move(baked), props.zIndex);
                }
            }
        }

        if (pathHasDrawableStroke(commands, props)) {
            const bool forceTransparentStroke =
                hasPathFill(props) && isFillTransparent(props);
            const bool strokeIsTransparent =
                forceTransparentStroke || isStrokeTransparent(props);
            PathStrokeBatch &strokeBatch = strokeIsTransparent
                                               ? transparentPathStrokeBatch
                                               : opaquePathStrokeBatch;
            strokeBatch.push(bakePathStroke(commands, props, metrics),
                             props.zIndex);
        }
    }

} // namespace Bess::Wgpu
