#include "bess_wgpu/wgpu_renderer_2d.h"
#include "bess_wgpu/piplines/path_pipeline.h"
#include "bess_wgpu/piplines/primitive_pipeline.h"
#include "bess_wgpu/wgpu_texture.h"
#include "common/bess_assert.h"
#include "common/logger.h"
#include "glfw3webgpu.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <limits>
#include <memory>
#include <png.h>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace Bess::Wgpu {
    using Core::Renderer::PathLineCap;
    using Core::Renderer::PathLineJoin;
    using Core::Renderer::PathProps;

    namespace {
        using Bess::Core::Renderer::Color;
        using Bess::Core::Renderer::QuadRenderPass;
        using Bess::Core::Renderer::Renderer2DExtent;
        using Bess::Core::Renderer::Renderer2DTargetFormat;
        using Bess::Wgpu::Piplines::PathCoverVertex;
        using Bess::Wgpu::Piplines::PathStencilVertex;
        using Bess::Wgpu::Piplines::PrimitiveInstance;

        constexpr wgpu::TextureFormat kDepthStencilFormat =
            wgpu::TextureFormat::Depth24PlusStencil8;
        constexpr uint32_t kPathCurveTypeLine = 0;
        constexpr uint32_t kPathCurveTypeQuadratic = 1;

        bool isTransparent(const Core::Renderer::QuadProps &props,
                           const Core::Renderer::RoundedBorderProps *rounded) {
            if (props.renderPass == QuadRenderPass::Opaque) {
                return false;
            }
            if (props.renderPass == QuadRenderPass::Transparent) {
                return true;
            }

            if (props.color.a < 0.999f) {
                return true;
            }
            if (rounded != nullptr && rounded->color.a < 0.999f) {
                return true;
            }
            return false;
        }

        bool hasPathFill(const PathProps &props) {
            return props.renderFill && props.fillColor.a > 0.f;
        }

        bool hasPathStroke(const PathProps &props) {
            return props.strokeSize > 0.f && props.strokeColor.a > 0.f;
        }

        bool isFillTransparent(const PathProps &props) {
            if (props.renderPass == QuadRenderPass::Opaque) {
                return false;
            }
            if (props.renderPass == QuadRenderPass::Transparent) {
                return true;
            }
            return props.fillColor.a < 0.999f;
        }

        bool isStrokeTransparent(const PathProps &props) {
            if (props.renderPass == QuadRenderPass::Opaque) {
                return false;
            }
            if (props.renderPass == QuadRenderPass::Transparent) {
                return true;
            }
            return props.strokeColor.a < 0.999f;
        }

        struct DrawRun {
            Core::Renderer::TextureHandle texture = 0;
            uint32_t firstInstance = 0;
            uint32_t instanceCount = 0;
        };

        wgpu::TextureFormat toWgpuFormat(Renderer2DTargetFormat format) {
            switch (format) {
            case Renderer2DTargetFormat::RGBA8Unorm:
                return wgpu::TextureFormat::RGBA8Unorm;
            case Renderer2DTargetFormat::RGBA16Float:
                return wgpu::TextureFormat::RGBA16Float;
            case Renderer2DTargetFormat::RG32Uint:
                return wgpu::TextureFormat::RG32Uint;
            case Renderer2DTargetFormat::None:
                return wgpu::TextureFormat::Undefined;
            case Renderer2DTargetFormat::BGRA8Unorm:
            default:
                return wgpu::TextureFormat::BGRA8Unorm;
            }
        }

        wgpu::Color toWgpuColor(const Color &color) {
            return {color.r, color.g, color.b, color.a};
        }

        uint32_t alignTo(uint32_t value, uint32_t alignment) {
            return ((value + alignment - 1) / alignment) * alignment;
        }

        bool isPngWritableFormat(wgpu::TextureFormat format) {
            return format == wgpu::TextureFormat::RGBA8Unorm ||
                   format == wgpu::TextureFormat::BGRA8Unorm;
        }

        struct FileDeleter {
            void operator()(FILE *file) const {
                if (file != nullptr) {
                    std::fclose(file);
                }
            }
        };

        void writePng(const std::string &path, const uint8_t *rgba,
                      uint32_t width, uint32_t height) {
            using FilePtr = std::unique_ptr<FILE, FileDeleter>;
            FilePtr file(std::fopen(path.c_str(), "wb"));
            if (!file) {
                throw std::runtime_error("Failed to open PNG for writing: " +
                                         path);
            }

            png_structp png = png_create_write_struct(
                PNG_LIBPNG_VER_STRING, nullptr, nullptr, nullptr);
            if (png == nullptr) {
                throw std::runtime_error("Failed to create PNG write struct");
            }

            png_infop info = png_create_info_struct(png);
            if (info == nullptr) {
                png_destroy_write_struct(&png, nullptr);
                throw std::runtime_error("Failed to create PNG info struct");
            }

            if (setjmp(png_jmpbuf(png))) {
                png_destroy_write_struct(&png, &info);
                throw std::runtime_error("Failed to write PNG: " + path);
            }

            png_init_io(png, file.get());
            png_set_IHDR(png, info, width, height, 8, PNG_COLOR_TYPE_RGBA,
                         PNG_INTERLACE_NONE, PNG_COMPRESSION_TYPE_DEFAULT,
                         PNG_FILTER_TYPE_DEFAULT);
            png_write_info(png, info);

            std::vector<png_bytep> rows(height);
            const auto rowBytes = static_cast<size_t>(width) * 4;
            for (uint32_t row = 0; row < height; ++row) {
                rows[row] = (unsigned char *)(rgba + (static_cast<size_t>(row) *
                                                      rowBytes));
            }

            png_write_image(png, rows.data());
            png_write_end(png, nullptr);
            png_destroy_write_struct(&png, &info);
        }

        void makePrimitiveInstanceInPlace(
            PrimitiveInstance &instance, const Core::Renderer::QuadProps &props,
            const Core::Renderer::RoundedBorderProps *roundedProps) {
            instance.position[0] = props.position.x;
            instance.position[1] = props.position.y;
            instance.position[2] = props.zIndex;
            instance.padding0 = 0.f;
            instance.color[0] = props.color.r;
            instance.color[1] = props.color.g;
            instance.color[2] = props.color.b;
            instance.color[3] = props.color.a;
            instance.texData[0] = props.uvRect.x;
            instance.texData[1] = props.uvRect.y;
            instance.texData[2] = props.uvRect.z;
            instance.texData[3] = props.uvRect.w;
            instance.size[0] = props.size.x;
            instance.size[1] = props.size.y;
            instance.id[0] = props.id.runtimeId;
            instance.id[1] = props.id.info;
            instance.primitiveType = 0; // Quad
            instance.isMica = 0;
            instance.texSlotIdx = props.texture == 0 ? 0 : 1;
            instance.angle = props.rotation;
            instance.primitiveData[0] = 0.f;
            instance.primitiveData[1] = 0.f;
            instance.primitiveData[2] = 0.f;
            instance.primitiveData[3] = 0.f;

            if (roundedProps != nullptr) {
                instance.borderRadius[0] = roundedProps->radius.x;
                instance.borderRadius[1] = roundedProps->radius.y;
                instance.borderRadius[2] = roundedProps->radius.z;
                instance.borderRadius[3] = roundedProps->radius.w;
                instance.borderSize[0] = roundedProps->thickness.x;
                instance.borderSize[1] = roundedProps->thickness.y;
                instance.borderSize[2] = roundedProps->thickness.z;
                instance.borderSize[3] = roundedProps->thickness.w;
                instance.borderColor[0] = roundedProps->color.r;
                instance.borderColor[1] = roundedProps->color.g;
                instance.borderColor[2] = roundedProps->color.b;
                instance.borderColor[3] = roundedProps->color.a;
            } else {
                instance.borderRadius[0] = 0.f;
                instance.borderRadius[1] = 0.f;
                instance.borderRadius[2] = 0.f;
                instance.borderRadius[3] = 0.f;
                instance.borderSize[0] = 0.f;
                instance.borderSize[1] = 0.f;
                instance.borderSize[2] = 0.f;
                instance.borderSize[3] = 0.f;
                instance.borderColor[0] = 0.f;
                instance.borderColor[1] = 0.f;
                instance.borderColor[2] = 0.f;
                instance.borderColor[3] = 0.f;
            }
        }

        void
        makeCircleInstanceInPlace(PrimitiveInstance &instance,
                                  const Core::Renderer::CircleProps &props) {
            instance.position[0] = props.position.x;
            instance.position[1] = props.position.y;
            instance.position[2] = props.zIndex;
            instance.padding0 = 0.f;
            instance.color[0] = props.color.r;
            instance.color[1] = props.color.g;
            instance.color[2] = props.color.b;
            instance.color[3] = props.color.a;
            instance.texData[0] = 0.f;
            instance.texData[1] = 0.f;
            instance.texData[2] = 1.f;
            instance.texData[3] = 1.f;
            instance.size[0] = props.radius * 2.f;
            instance.size[1] = props.radius * 2.f;
            instance.id[0] = props.id.runtimeId;
            instance.id[1] = props.id.info;
            instance.primitiveType = 1; // Circle
            instance.isMica = 0;
            instance.texSlotIdx = 0;
            instance.angle = 0.f;
            instance.primitiveData[0] = props.radius;
            instance.primitiveData[1] =
                props.thickness > 0.f ? props.radius - props.thickness : 0.f;
            instance.primitiveData[2] = 0.f;
            instance.primitiveData[3] = 0.f;

            instance.borderRadius[0] = 0.f;
            instance.borderRadius[1] = 0.f;
            instance.borderRadius[2] = 0.f;
            instance.borderRadius[3] = 0.f;
            instance.borderSize[0] = 0.f;
            instance.borderSize[1] = 0.f;
            instance.borderSize[2] = 0.f;
            instance.borderSize[3] = 0.f;
            instance.borderColor[0] = 0.f;
            instance.borderColor[1] = 0.f;
            instance.borderColor[2] = 0.f;
            instance.borderColor[3] = 0.f;
        }

        void makeLineInstanceInPlace(PrimitiveInstance &instance,
                                     const Core::Renderer::LineProps &props) {
            glm::vec2 diff = props.p1 - props.p0;
            float length = glm::length(diff);
            float angle = std::atan2(diff.y, diff.x);
            glm::vec2 pos = (props.p0 + props.p1) * 0.5f;
            constexpr float aaPadding = 2.f;
            const float thickness = std::max(props.thickness, 1.f);

            instance.position[0] = pos.x;
            instance.position[1] = pos.y;
            instance.position[2] = props.zIndex;
            instance.padding0 = 0.f;
            instance.color[0] = props.color.r;
            instance.color[1] = props.color.g;
            instance.color[2] = props.color.b;
            instance.color[3] = props.color.a;
            instance.texData[0] = 0.f;
            instance.texData[1] = 0.f;
            instance.texData[2] = 1.f;
            instance.texData[3] = 1.f;
            instance.size[0] = length + (aaPadding * 2.f);
            instance.size[1] = thickness + (aaPadding * 2.f);
            instance.id[0] = props.id.runtimeId;
            instance.id[1] = props.id.info;
            instance.primitiveType = 2; // Line
            instance.isMica = 0;
            instance.texSlotIdx = 0;
            instance.angle = angle;
            instance.primitiveData[0] = length;
            instance.primitiveData[1] = thickness;
            instance.primitiveData[2] = aaPadding;
            instance.primitiveData[3] = 0.f;

            instance.borderRadius[0] = 0.f;
            instance.borderRadius[1] = 0.f;
            instance.borderRadius[2] = 0.f;
            instance.borderRadius[3] = 0.f;
            instance.borderSize[0] = 0.f;
            instance.borderSize[1] = 0.f;
            instance.borderSize[2] = 0.f;
            instance.borderSize[3] = 0.f;
            instance.borderColor[0] = 0.f;
            instance.borderColor[1] = 0.f;
            instance.borderColor[2] = 0.f;
            instance.borderColor[3] = 0.f;
        }

        class PrimitiveBatch {
          public:
            void configure(uint32_t initialCapacity, uint32_t maxCapacity) {
                m_maxCapacity = std::max(1u, maxCapacity);
                // Pre-allocate to max capacity to avoid reallocations and debug
                // bounds checking overhead
                m_gpuInstances.resize(m_maxCapacity);
                m_drawRuns.resize(m_maxCapacity);
                m_gpuInstancesPtr = m_gpuInstances.data();
                m_drawRunsPtr = m_drawRuns.data();
                m_instanceCount = 0;
                m_drawRunsCount = 0;
            }

            void clear() {
                m_instanceCount = 0;
                m_drawRunsCount = 0;
            }

            PrimitiveInstance &push(Core::Renderer::TextureHandle texture) {
                if (m_instanceCount >= m_maxCapacity) {
                    throw std::runtime_error(
                        "WGPU quad batch capacity exceeded");
                }
                const uint32_t instanceIndex = m_instanceCount;
                if (m_drawRunsCount == 0 ||
                    m_drawRunsPtr[m_drawRunsCount - 1].texture != texture) {
                    m_drawRunsPtr[m_drawRunsCount++] = {.texture = texture,
                                                        .firstInstance =
                                                            instanceIndex,
                                                        .instanceCount = 1};
                } else {
                    m_drawRunsPtr[m_drawRunsCount - 1].instanceCount++;
                }
                return m_gpuInstancesPtr[m_instanceCount++];
            }

            void prepareForRendering(bool sortBackToFront) {
                if (sortBackToFront && m_instanceCount > 1) {
                    std::vector<uint32_t> indices(m_instanceCount);
                    uint32_t *indicesPtr = indices.data();
                    for (uint32_t i = 0; i < m_instanceCount; ++i) {
                        indicesPtr[i] = i;
                    }

                    std::stable_sort(
                        indices.begin(), indices.end(),
                        [this](uint32_t a, uint32_t b) {
                            if (m_gpuInstancesPtr[a].position[2] !=
                                m_gpuInstancesPtr[b].position[2]) {
                                return m_gpuInstancesPtr[a].position[2] <
                                       m_gpuInstancesPtr[b].position[2];
                            }
                            return a < b;
                        });

                    std::vector<Core::Renderer::TextureHandle> textures(
                        m_instanceCount);
                    Core::Renderer::TextureHandle *texPtr = textures.data();
                    for (uint32_t r = 0; r < m_drawRunsCount; ++r) {
                        const auto &run = m_drawRunsPtr[r];
                        for (uint32_t i = 0; i < run.instanceCount; ++i) {
                            texPtr[run.firstInstance + i] = run.texture;
                        }
                    }

                    std::vector<PrimitiveInstance> sortedInstances(
                        m_instanceCount);
                    PrimitiveInstance *sortedPtr = sortedInstances.data();
                    m_drawRunsCount = 0;

                    for (uint32_t i = 0; i < m_instanceCount; ++i) {
                        uint32_t oldIdx = indicesPtr[i];
                        sortedPtr[i] = m_gpuInstancesPtr[oldIdx];
                        Core::Renderer::TextureHandle tex = texPtr[oldIdx];

                        if (m_drawRunsCount == 0 ||
                            m_drawRunsPtr[m_drawRunsCount - 1].texture != tex) {
                            m_drawRunsPtr[m_drawRunsCount++] = {
                                .texture = tex,
                                .firstInstance = i,
                                .instanceCount = 1};
                        } else {
                            m_drawRunsPtr[m_drawRunsCount - 1].instanceCount++;
                        }
                    }
                    // Copy back to main buffer
                    for (uint32_t i = 0; i < m_instanceCount; ++i) {
                        m_gpuInstancesPtr[i] = sortedPtr[i];
                    }
                }
            }

            [[nodiscard]] bool empty() const noexcept {
                return m_instanceCount == 0;
            }

            [[nodiscard]] uint32_t count() const noexcept {
                return m_instanceCount;
            }

            [[nodiscard]] uint64_t byteSize() const noexcept {
                return static_cast<uint64_t>(m_instanceCount) *
                       sizeof(PrimitiveInstance);
            }

            [[nodiscard]] const PrimitiveInstance *data() const noexcept {
                return m_gpuInstancesPtr;
            }

            [[nodiscard]] const DrawRun *drawRunsData() const noexcept {
                return m_drawRunsPtr;
            }

            [[nodiscard]] uint32_t drawRunsCount() const noexcept {
                return m_drawRunsCount;
            }

          private:
            std::vector<PrimitiveInstance> m_gpuInstances;
            std::vector<DrawRun> m_drawRuns;
            PrimitiveInstance *m_gpuInstancesPtr = nullptr;
            DrawRun *m_drawRunsPtr = nullptr;
            uint32_t m_instanceCount = 0;
            uint32_t m_drawRunsCount = 0;
            uint32_t m_maxCapacity = 1;
        };

        enum class PathCommandKind : uint8_t {
            Move,
            Line,
            Quad,
            Cubic,
        };

        struct PathCommand {
            PathCommandKind kind = PathCommandKind::Move;
            glm::vec2 p{0.f};
            glm::vec2 control{0.f};
            glm::vec2 control2{0.f};
        };

        struct PathDrawRange {
            uint32_t firstStencilVertex = 0;
            uint32_t stencilVertexCount = 0;
            uint32_t firstCoverVertex = 0;
            uint32_t coverVertexCount = 0;
            float zIndex = 0.f;
        };

        struct BakedPath {
            std::vector<PathStencilVertex> stencilVertices;
            std::array<PathCoverVertex, 6> coverVertices{};
            bool valid = false;
        };

        class PathBatch {
          public:
            void clear() {
                m_stencilVertices.clear();
                m_coverVertices.clear();
                m_drawRanges.clear();
            }

            void push(BakedPath &&path, float zIndex) {
                if (!path.valid || path.stencilVertices.empty()) {
                    return;
                }

                PathDrawRange range{};
                range.firstStencilVertex =
                    static_cast<uint32_t>(m_stencilVertices.size());
                range.stencilVertexCount =
                    static_cast<uint32_t>(path.stencilVertices.size());
                range.firstCoverVertex =
                    static_cast<uint32_t>(m_coverVertices.size());
                range.coverVertexCount =
                    static_cast<uint32_t>(path.coverVertices.size());
                range.zIndex = zIndex;

                m_stencilVertices.insert(m_stencilVertices.end(),
                                         path.stencilVertices.begin(),
                                         path.stencilVertices.end());
                m_coverVertices.insert(m_coverVertices.end(),
                                       path.coverVertices.begin(),
                                       path.coverVertices.end());
                m_drawRanges.push_back(range);
            }

            void prepareForRendering(bool sortBackToFront) {
                if (!sortBackToFront || m_drawRanges.size() <= 1) {
                    return;
                }

                std::stable_sort(
                    m_drawRanges.begin(), m_drawRanges.end(),
                    [](const PathDrawRange &a, const PathDrawRange &b) {
                        return a.zIndex < b.zIndex;
                    });
            }

            [[nodiscard]] bool empty() const noexcept {
                return m_drawRanges.empty();
            }

            [[nodiscard]] uint32_t drawCount() const noexcept {
                return static_cast<uint32_t>(m_drawRanges.size());
            }

            [[nodiscard]] uint32_t stencilVertexCount() const noexcept {
                return static_cast<uint32_t>(m_stencilVertices.size());
            }

            [[nodiscard]] uint32_t coverVertexCount() const noexcept {
                return static_cast<uint32_t>(m_coverVertices.size());
            }

            [[nodiscard]] uint64_t stencilByteSize() const noexcept {
                return static_cast<uint64_t>(m_stencilVertices.size()) *
                       sizeof(PathStencilVertex);
            }

            [[nodiscard]] uint64_t coverByteSize() const noexcept {
                return static_cast<uint64_t>(m_coverVertices.size()) *
                       sizeof(PathCoverVertex);
            }

            [[nodiscard]] const PathStencilVertex *
            stencilData() const noexcept {
                return m_stencilVertices.data();
            }

            [[nodiscard]] const PathCoverVertex *coverData() const noexcept {
                return m_coverVertices.data();
            }

            [[nodiscard]] const PathDrawRange *drawRanges() const noexcept {
                return m_drawRanges.data();
            }

          private:
            std::vector<PathStencilVertex> m_stencilVertices;
            std::vector<PathCoverVertex> m_coverVertices;
            std::vector<PathDrawRange> m_drawRanges;
        };

        struct PathStrokeDrawRange {
            uint32_t firstVertex = 0;
            uint32_t vertexCount = 0;
            float zIndex = 0.f;
        };

        class PathStrokeBatch {
          public:
            void clear() {
                m_vertices.clear();
                m_drawRanges.clear();
            }

            void push(std::vector<PathCoverVertex> &&vertices, float zIndex) {
                if (vertices.empty()) {
                    return;
                }

                PathStrokeDrawRange range{};
                range.firstVertex = static_cast<uint32_t>(m_vertices.size());
                range.vertexCount = static_cast<uint32_t>(vertices.size());
                range.zIndex = zIndex;
                m_vertices.insert(m_vertices.end(), vertices.begin(),
                                  vertices.end());
                m_drawRanges.push_back(range);
            }

            void prepareForRendering(bool sortBackToFront) {
                if (!sortBackToFront || m_drawRanges.size() <= 1) {
                    return;
                }

                std::stable_sort(m_drawRanges.begin(), m_drawRanges.end(),
                                 [](const PathStrokeDrawRange &a,
                                    const PathStrokeDrawRange &b) {
                                     return a.zIndex < b.zIndex;
                                 });
            }

            [[nodiscard]] bool empty() const noexcept {
                return m_drawRanges.empty();
            }

            [[nodiscard]] uint32_t drawCount() const noexcept {
                return static_cast<uint32_t>(m_drawRanges.size());
            }

            [[nodiscard]] uint32_t vertexCount() const noexcept {
                return static_cast<uint32_t>(m_vertices.size());
            }

            [[nodiscard]] uint64_t byteSize() const noexcept {
                return static_cast<uint64_t>(m_vertices.size()) *
                       sizeof(PathCoverVertex);
            }

            [[nodiscard]] const PathCoverVertex *data() const noexcept {
                return m_vertices.data();
            }

            [[nodiscard]] const PathStrokeDrawRange *
            drawRanges() const noexcept {
                return m_drawRanges.data();
            }

          private:
            std::vector<PathCoverVertex> m_vertices;
            std::vector<PathStrokeDrawRange> m_drawRanges;
        };

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
                            float z, const Color &color, const PickingId &id) {
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

        float curveTolerance(const PathProps &props) {
            return std::max(props.curveTolerance, 0.01f);
        }

        int quadraticSegmentCount(const glm::vec2 &p0, const glm::vec2 &control,
                                  const glm::vec2 &p1, const PathProps &props) {
            const float controlNet =
                glm::distance(p0, control) + glm::distance(control, p1);
            const float curvature = glm::length(p0 - (2.f * control) + p1);
            const float tolerance = curveTolerance(props);
            const float lengthSegments = std::ceil(controlNet / 24.f);
            const float curveSegments =
                std::ceil(std::sqrt(curvature / tolerance));
            return std::clamp(
                static_cast<int>(std::max(lengthSegments, curveSegments)), 1,
                128);
        }

        int cubicSegmentCount(const glm::vec2 &p0, const glm::vec2 &control1,
                              const glm::vec2 &control2, const glm::vec2 &p1,
                              const PathProps &props) {
            const float controlNet = glm::distance(p0, control1) +
                                     glm::distance(control1, control2) +
                                     glm::distance(control2, p1);
            const float curvature =
                std::max(glm::length(p0 - (2.f * control1) + control2),
                         glm::length(control1 - (2.f * control2) + p1));
            const float tolerance = curveTolerance(props);
            const float lengthSegments = std::ceil(controlNet / 18.f);
            const float curveSegments =
                std::ceil(std::sqrt((3.f * curvature) / tolerance));
            return std::clamp(
                static_cast<int>(std::max(lengthSegments, curveSegments)), 1,
                192);
        }

        BakedPath bakePath(const std::vector<PathCommand> &commands,
                           const PathProps &props) {
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

            auto closeContour = [&]() {
                if (!contourOpen || !props.closePath) {
                    return;
                }
                appendLineAnchorTriangle(baked.stencilVertices, anchor, current,
                                         anchor, props.zIndex);
                current = anchor;
            };

            for (const auto &cmd : commands) {
                switch (cmd.kind) {
                case PathCommandKind::Move:
                    closeContour();
                    anchor = cmd.p;
                    current = cmd.p;
                    contourOpen = true;
                    recordPoint(cmd.p);
                    break;
                case PathCommandKind::Line:
                    if (!contourOpen) {
                        anchor = current;
                        contourOpen = true;
                        recordPoint(anchor);
                    }
                    appendLineAnchorTriangle(baked.stencilVertices, anchor,
                                             current, cmd.p, props.zIndex);
                    current = cmd.p;
                    recordPoint(cmd.p);
                    break;
                case PathCommandKind::Quad:
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
                case PathCommandKind::Cubic:
                    if (!contourOpen) {
                        anchor = current;
                        contourOpen = true;
                        recordPoint(anchor);
                    }
                    {
                        const int segments = cubicSegmentCount(
                            current, cmd.control, cmd.control2, cmd.p, props);
                        glm::vec2 prev = current;
                        for (int i = 1; i <= segments; ++i) {
                            const float t = static_cast<float>(i) /
                                            static_cast<float>(segments);
                            glm::vec2 next = evalCubic(current, cmd.control,
                                                       cmd.control2, cmd.p, t);
                            appendLineAnchorTriangle(baked.stencilVertices,
                                                     anchor, prev, next,
                                                     props.zIndex);
                            prev = next;
                            recordPoint(next);
                        }
                    }
                    current = cmd.p;
                    recordPoint(cmd.control);
                    recordPoint(cmd.control2);
                    recordPoint(cmd.p);
                    break;
                }
            }

            closeContour();

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
            baked.valid = true;
            return baked;
        }

        glm::vec2 safeNormalize(const glm::vec2 &v) {
            const float len = glm::length(v);
            if (len < 0.0001f) {
                return glm::vec2(1.f, 0.f);
            }
            return v / len;
        }

        glm::vec2 perpendicular(const glm::vec2 &v) {
            return glm::vec2(-v.y, v.x);
        }

        void appendStrokeVertex(std::vector<PathCoverVertex> &vertices,
                                const glm::vec2 &pos, const PathProps &props) {
            auto &vertex = vertices.emplace_back();
            setCoverVertex(vertex, pos, props.zIndex, props.strokeColor,
                           props.id);
        }

        void appendStrokeTriangle(std::vector<PathCoverVertex> &vertices,
                                  const glm::vec2 &a, const glm::vec2 &b,
                                  const glm::vec2 &c, const PathProps &props) {
            if (nearlyDegenerateTriangle(a, b, c)) {
                return;
            }
            appendStrokeVertex(vertices, a, props);
            appendStrokeVertex(vertices, b, props);
            appendStrokeVertex(vertices, c, props);
        }

        void appendRoundCap(std::vector<PathCoverVertex> &vertices,
                            const glm::vec2 &center,
                            const glm::vec2 &capDirection,
                            const PathProps &props) {
            constexpr float pi = 3.14159265358979323846f;
            const float halfWidth = props.strokeSize * 0.5f;
            const glm::vec2 dir = safeNormalize(capDirection);
            const float centerAngle = std::atan2(dir.y, dir.x);
            const int segments = std::clamp(
                static_cast<int>(std::ceil(pi * halfWidth / 4.f)), 8, 32);

            glm::vec2 prev =
                center + glm::vec2(std::cos(centerAngle - (pi * 0.5f)),
                                   std::sin(centerAngle - (pi * 0.5f))) *
                             halfWidth;
            for (int i = 1; i <= segments; ++i) {
                const float t =
                    static_cast<float>(i) / static_cast<float>(segments);
                const float angle = centerAngle - (pi * 0.5f) + (pi * t);
                glm::vec2 next =
                    center +
                    glm::vec2(std::cos(angle), std::sin(angle)) * halfWidth;
                appendStrokeTriangle(vertices, center, prev, next, props);
                prev = next;
            }
        }

        void appendRoundJoin(std::vector<PathCoverVertex> &vertices,
                             const glm::vec2 &center,
                             const glm::vec2 &prevOuterNormal,
                             const glm::vec2 &nextOuterNormal,
                             const PathProps &props) {
            const float halfWidth = props.strokeSize * 0.5f;
            const float a0 = std::atan2(prevOuterNormal.y, prevOuterNormal.x);
            float delta =
                std::atan2((prevOuterNormal.x * nextOuterNormal.y) -
                               (prevOuterNormal.y * nextOuterNormal.x),
                           glm::dot(prevOuterNormal, nextOuterNormal));
            if (std::abs(delta) < 0.0001f) {
                return;
            }

            const int segments = std::clamp(
                static_cast<int>(std::ceil(std::abs(delta) * halfWidth / 4.f)),
                4, 32);
            glm::vec2 prev = center + prevOuterNormal * halfWidth;
            for (int i = 1; i <= segments; ++i) {
                const float t =
                    static_cast<float>(i) / static_cast<float>(segments);
                const float angle = a0 + (delta * t);
                glm::vec2 next =
                    center +
                    glm::vec2(std::cos(angle), std::sin(angle)) * halfWidth;
                appendStrokeTriangle(vertices, center, prev, next, props);
                prev = next;
            }
        }

        void appendStrokeJoin(std::vector<PathCoverVertex> &vertices,
                              const glm::vec2 &center, const glm::vec2 &prevDir,
                              const glm::vec2 &nextDir,
                              const PathProps &props) {
            const float turn =
                (prevDir.x * nextDir.y) - (prevDir.y * nextDir.x);
            if (std::abs(turn) < 0.0001f) {
                return;
            }

            const float halfWidth = props.strokeSize * 0.5f;
            const glm::vec2 prevNormal = perpendicular(prevDir);
            const glm::vec2 nextNormal = perpendicular(nextDir);
            const glm::vec2 prevLeft = center + prevNormal * halfWidth;
            const glm::vec2 prevRight = center - prevNormal * halfWidth;
            const glm::vec2 nextLeft = center + nextNormal * halfWidth;
            const glm::vec2 nextRight = center - nextNormal * halfWidth;

            appendStrokeTriangle(vertices, center, prevLeft, nextLeft, props);
            appendStrokeTriangle(vertices, center, nextRight, prevRight, props);

            const float side = turn > 0.f ? -1.f : 1.f;
            const glm::vec2 prevOuterNormal = prevNormal * side;
            const glm::vec2 nextOuterNormal = nextNormal * side;
            const glm::vec2 prevOuter = center + prevOuterNormal * halfWidth;
            const glm::vec2 nextOuter = center + nextOuterNormal * halfWidth;

            if (props.lineJoin == PathLineJoin::Round) {
                appendRoundJoin(vertices, center, prevOuterNormal,
                                nextOuterNormal, props);
                return;
            }

            if (props.lineJoin == PathLineJoin::Miter) {
                glm::vec2 miter = prevOuterNormal + nextOuterNormal;
                if (glm::length(miter) > 0.0001f) {
                    miter = safeNormalize(miter);
                    const float denom = glm::dot(miter, nextOuterNormal);
                    if (std::abs(denom) > 0.0001f) {
                        const float miterLength = halfWidth / denom;
                        const float limit =
                            halfWidth * std::max(props.miterLimit, 1.f);
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

        void appendStrokeSegment(std::vector<PathCoverVertex> &vertices,
                                 const glm::vec2 &from, const glm::vec2 &to,
                                 const PathProps &props) {
            const glm::vec2 delta = to - from;
            if (glm::length(delta) < 0.0001f) {
                return;
            }

            const float halfWidth = props.strokeSize * 0.5f;
            const glm::vec2 normal =
                perpendicular(safeNormalize(delta)) * halfWidth;
            const glm::vec2 leftFrom = from + normal;
            const glm::vec2 rightFrom = from - normal;
            const glm::vec2 leftTo = to + normal;
            const glm::vec2 rightTo = to - normal;

            appendStrokeTriangle(vertices, leftFrom, rightFrom, leftTo, props);
            appendStrokeTriangle(vertices, leftTo, rightFrom, rightTo, props);
        }

        void appendStrokeContour(std::vector<PathCoverVertex> &vertices,
                                 std::vector<glm::vec2> points, bool closed,
                                 const PathProps &props) {
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

            if (closed &&
                glm::distance(points.front(), points.back()) < epsilon) {
                points.pop_back();
            }
            if (closed && points.size() < 3) {
                closed = false;
            }

            const size_t count = points.size();
            const float halfWidth = props.strokeSize * 0.5f;

            glm::vec2 startCapCenter = points.front();
            glm::vec2 endCapCenter = points.back();
            glm::vec2 startDir = safeNormalize(points[1] - points[0]);
            glm::vec2 endDir =
                safeNormalize(points[count - 1] - points[count - 2]);

            if (!closed && props.lineCap == PathLineCap::Square) {
                points.front() -= startDir * halfWidth;
                points.back() += endDir * halfWidth;
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
                appendStrokeSegment(vertices, points[i], points[next], props);
            }

            const size_t joinCount = closed ? count : count > 2 ? count - 2 : 0;
            for (size_t join = 0; join < joinCount; ++join) {
                const size_t i = closed ? join : join + 1;
                const glm::vec2 prev = pointAt(static_cast<ptrdiff_t>(i) - 1);
                const glm::vec2 curr = points[i];
                const glm::vec2 next = pointAt(static_cast<ptrdiff_t>(i) + 1);
                appendStrokeJoin(vertices, curr, safeNormalize(curr - prev),
                                 safeNormalize(next - curr), props);
            }

            if (!closed && props.lineCap == PathLineCap::Round) {
                appendRoundCap(vertices, startCapCenter, -startDir, props);
                appendRoundCap(vertices, endCapCenter, endDir, props);
            }
        }

        std::vector<PathCoverVertex>
        bakePathStroke(const std::vector<PathCommand> &commands,
                       const PathProps &props) {
            std::vector<PathCoverVertex> vertices;
            if (commands.empty() || !hasPathStroke(props)) {
                return vertices;
            }

            std::vector<glm::vec2> contour;
            glm::vec2 current(0.f);

            auto flushContour = [&]() {
                appendStrokeContour(vertices, std::move(contour),
                                    props.closePath, props);
                contour.clear();
            };

            for (const auto &cmd : commands) {
                switch (cmd.kind) {
                case PathCommandKind::Move:
                    flushContour();
                    contour.push_back(cmd.p);
                    current = cmd.p;
                    break;
                case PathCommandKind::Line:
                    if (contour.empty()) {
                        contour.push_back(current);
                    }
                    contour.push_back(cmd.p);
                    current = cmd.p;
                    break;
                case PathCommandKind::Quad:
                    if (contour.empty()) {
                        contour.push_back(current);
                    }
                    if (nearlyDegenerateTriangle(current, cmd.control, cmd.p)) {
                        contour.push_back(cmd.p);
                    } else {
                        const int segments = quadraticSegmentCount(
                            current, cmd.control, cmd.p, props);
                        for (int i = 1; i <= segments; ++i) {
                            const float t = static_cast<float>(i) /
                                            static_cast<float>(segments);
                            contour.push_back(
                                evalQuadratic(current, cmd.control, cmd.p, t));
                        }
                    }
                    current = cmd.p;
                    break;
                case PathCommandKind::Cubic:
                    if (contour.empty()) {
                        contour.push_back(current);
                    }
                    {
                        const int segments = cubicSegmentCount(
                            current, cmd.control, cmd.control2, cmd.p, props);
                        for (int i = 1; i <= segments; ++i) {
                            const float t = static_cast<float>(i) /
                                            static_cast<float>(segments);
                            contour.push_back(evalCubic(
                                current, cmd.control, cmd.control2, cmd.p, t));
                        }
                    }
                    current = cmd.p;
                    break;
                }
            }

            flushContour();
            return vertices;
        }

        class TextureSource final : public Core::Renderer::ITexture {
          public:
            explicit TextureSource(
                const Core::Renderer::TextureCreateInfo &createInfo)
                : ITexture(createInfo) {}

            void init() override {}
            void destroy() override {}
        };
    } // namespace

    struct WgpuRenderer2D::Impl {
        Core::Renderer::Renderer2DCreateInfo createInfo;
        Renderer2DExtent extent;
        Core::Renderer::Renderer2DTargetFormat targetFormatType =
            Core::Renderer::Renderer2DTargetFormat::BGRA8Unorm;
        wgpu::TextureFormat targetFormat = wgpu::TextureFormat::BGRA8Unorm;

        wgpu::Instance instance;
        wgpu::Adapter adapter;
        wgpu::Device device;
        wgpu::Queue queue;
        wgpu::Surface surface;
        wgpu::SurfaceConfiguration surfaceConfiguration;
        wgpu::TextureFormat surfaceFormat = wgpu::TextureFormat::BGRA8Unorm;
        wgpu::PresentMode surfacePresentMode = wgpu::PresentMode::Fifo;
        wgpu::CompositeAlphaMode surfaceAlphaMode =
            wgpu::CompositeAlphaMode::Opaque;
        GLFWwindow *windowHandle = nullptr;
        bool surfaceConfigured = false;
        bool frameUsesSurface = false;
        Core::Renderer::TextureHandle frameTargetTexture = 0;
        Core::Renderer::TextureHandle lastCompletedTargetTexture = 0;

        wgpu::Texture offscreenTarget;
        wgpu::TextureView offscreenTargetView;
        wgpu::Texture depthTarget;
        wgpu::TextureView depthTargetView;
        float *cameraTransform = nullptr;
        Piplines::SharedFrameBuffer sharedFrameBuffer;
        std::unique_ptr<Piplines::PrimitivePipeline> primitivePipeline;
        std::unique_ptr<Piplines::PathPipeline> pathPipeline;
        wgpu::CommandEncoder commandEncoder;
        std::unordered_map<Core::Renderer::TextureHandle, TextureResource>
            textures;
        std::shared_ptr<WgpuTexture> defaultTexture;

        PrimitiveBatch opaquePrimitiveBatch;
        PrimitiveBatch transparentPrimitiveBatch;
        PathStrokeBatch opaquePathStrokeBatch;
        PathStrokeBatch transparentPathStrokeBatch;
        PathBatch opaquePathBatch;
        PathBatch transparentPathBatch;
        std::vector<PathCommand> activePathCommands;
        PathProps activePathProps;
        bool pathStarted = false;
        Core::Renderer::Renderer2DStats stats;
        Color clearColor{0.f, 0.f, 0.f, 1.f};
        bool shouldClear = true;
        bool frameStarted = false;
        wgpu::TextureFormat pickingFormat = wgpu::TextureFormat::Undefined;
        Core::Renderer::TextureHandle pickingTextureHandle = 0;

        void createDevice();
        void createOffscreenTarget();
        void createDepthTarget();
        void createWindowSurface();
        void configureWindowSurface(uint32_t width, uint32_t height);
        void createDefaultTexture();
        void recreateTextureBindGroups();
        [[nodiscard]] const TextureResource &
        getTexture(Core::Renderer::TextureHandle texture) const;
        [[nodiscard]] uint32_t primitiveStatsCount() const noexcept {
            return opaquePrimitiveBatch.count() +
                   transparentPrimitiveBatch.count();
        }
    };

    void WgpuRenderer2D::Impl::createDevice() {
        struct RequestResult {
            wgpu::Adapter adapter;
            wgpu::Device device;
            std::string error;
        };

        wgpu::InstanceDescriptor instanceDescriptor{};
        instanceDescriptor.capabilities.timedWaitAnyEnable = true;
        instance = wgpu::CreateInstance(&instanceDescriptor);
        if (instance == nullptr) {
            throw std::runtime_error("Failed to create WebGPU instance");
        }

        wgpu::RequestAdapterOptions adapterOptions{};
        RequestResult adapterResult;
        auto adapterCallback =
            [&adapterResult](wgpu::RequestAdapterStatus status,
                             wgpu::Adapter adapter, wgpu::StringView message) {
                if (status != wgpu::RequestAdapterStatus::Success) {
                    adapterResult.error =
                        message.data != nullptr
                            ? std::string(message.data, message.length)
                            : "unknown adapter error";
                    return;
                }
                adapterResult.adapter = std::move(adapter);
            };

        instance.WaitAny(instance.RequestAdapter(
                             &adapterOptions, wgpu::CallbackMode::WaitAnyOnly,
                             adapterCallback),
                         UINT64_MAX);
        adapter = adapterResult.adapter;
        if (adapter == nullptr) {
            throw std::runtime_error("Failed to request WebGPU adapter: " +
                                     adapterResult.error);
        }

        wgpu::DeviceDescriptor deviceDescriptor{};
        deviceDescriptor.SetUncapturedErrorCallback(
            [](const wgpu::Device &, wgpu::ErrorType type,
               wgpu::StringView message) {
                BESS_ERROR("Dawn Validation Error [{}]: {}",
                           static_cast<int>(type),
                           std::string_view(message.data, message.length));
            });
        deviceDescriptor.SetDeviceLostCallback(
            wgpu::CallbackMode::AllowSpontaneous,
            [](const wgpu::Device &, wgpu::DeviceLostReason reason,
               wgpu::StringView message) {
                BESS_ERROR("Dawn Device Lost [{}]: {}",
                           static_cast<int>(reason),
                           std::string_view(message.data, message.length));
            });
        RequestResult deviceResult;
        auto deviceCallback = [&deviceResult](wgpu::RequestDeviceStatus status,
                                              wgpu::Device device,
                                              wgpu::StringView message) {
            if (status != wgpu::RequestDeviceStatus::Success) {
                deviceResult.error =
                    message.data != nullptr
                        ? std::string(message.data, message.length)
                        : "unknown device error";
                return;
            }
            deviceResult.device = std::move(device);
        };

        instance.WaitAny(adapter.RequestDevice(&deviceDescriptor,
                                               wgpu::CallbackMode::WaitAnyOnly,
                                               deviceCallback),
                         UINT64_MAX);
        device = deviceResult.device;
        if (device == nullptr) {
            throw std::runtime_error("Failed to request WebGPU device: " +
                                     deviceResult.error);
        }

        queue = device.GetQueue();
    }

    void WgpuRenderer2D::Impl::createOffscreenTarget() {
        wgpu::TextureDescriptor descriptor{};
        descriptor.dimension = wgpu::TextureDimension::e2D;
        descriptor.size = {std::max(1u, extent.width),
                           std::max(1u, extent.height), 1};
        descriptor.format = targetFormat;
        descriptor.mipLevelCount = 1;
        descriptor.sampleCount = 1;
        descriptor.usage = wgpu::TextureUsage::RenderAttachment |
                           wgpu::TextureUsage::TextureBinding |
                           wgpu::TextureUsage::CopySrc;
        descriptor.label = "OffscreenRenderTarget";

        offscreenTarget = device.CreateTexture(&descriptor);
        offscreenTargetView = offscreenTarget.CreateView();
    }

    void WgpuRenderer2D::Impl::createDepthTarget() {
        wgpu::TextureDescriptor descriptor{};
        descriptor.dimension = wgpu::TextureDimension::e2D;
        descriptor.size = {std::max(1u, extent.width),
                           std::max(1u, extent.height), 1};
        descriptor.format = kDepthStencilFormat;
        descriptor.mipLevelCount = 1;
        descriptor.sampleCount = 1;
        descriptor.usage = wgpu::TextureUsage::RenderAttachment;
        descriptor.label = "DepthRenderTarget";

        depthTarget = device.CreateTexture(&descriptor);
        depthTargetView = depthTarget.CreateView();
    }

    void WgpuRenderer2D::Impl::createDefaultTexture() {
        const std::array<uint8_t, 4> whitePixel{255, 255, 255, 255};
        defaultTexture = WgpuTexture::fromPixels(whitePixel.data(), 1, 1);
    }

    void WgpuRenderer2D::Impl::recreateTextureBindGroups() {
        if (!primitivePipeline) {
            return;
        }

        for (auto &[handle, texture] : textures) {
            texture.bindGroup = primitivePipeline->createTextureBindGroup(
                texture.view, "TextureBindGroup_" + std::to_string(handle));
        }
    }

    const TextureResource &WgpuRenderer2D::Impl::getTexture(
        Core::Renderer::TextureHandle texture) const {
        if (texture == 0) {
            return textures.at(defaultTexture->getHandle());
        }

        BESS_ASSERT(!textures.empty(), "No textures available in renderer");
        const auto it = textures.find(texture);
        if (it != textures.end()) {
            return it->second;
        }

        BESS_ASSERT(false, "Requested texture handle {} not found in renderer",
                    texture);
        return textures.at(defaultTexture->getHandle());
    }

    WgpuRenderer2D::WgpuRenderer2D() : m_impl(std::make_unique<Impl>()) {}

    WgpuRenderer2D::~WgpuRenderer2D() { destroy(); }

    void WgpuRenderer2D::init(
        const Core::Renderer::Renderer2DCreateInfo &createInfo) {
        destroy();
        m_impl = std::make_unique<Impl>();
        m_impl->createInfo = createInfo;
        m_impl->extent = createInfo.extent;
        m_impl->targetFormatType = createInfo.targetFormat;
        m_impl->targetFormat = toWgpuFormat(createInfo.targetFormat);
        if (createInfo.surface.type ==
            Core::Renderer::Renderer2DNativeSurfaceType::PlatformHandle) {
            m_impl->windowHandle =
                static_cast<GLFWwindow *>(createInfo.surface.handle);
        }
        m_impl->opaquePrimitiveBatch.configure(
            createInfo.batching.initialQuadCapacity,
            createInfo.batching.maxQuadCapacity);
        m_impl->transparentPrimitiveBatch.configure(
            createInfo.batching.initialQuadCapacity,
            createInfo.batching.maxQuadCapacity);
        m_impl->createDevice();
        m_impl->createWindowSurface();
        m_impl->createOffscreenTarget();
        m_impl->createDepthTarget();
        m_impl->sharedFrameBuffer.init(m_impl->device);
        m_impl->pickingFormat = toWgpuFormat(createInfo.pickingFormat);
        m_impl->primitivePipeline =
            std::make_unique<Piplines::PrimitivePipeline>();
        m_impl->primitivePipeline->init(m_impl->device, m_impl->targetFormat,
                                        m_impl->sharedFrameBuffer.getBuffer(),
                                        m_impl->sharedFrameBuffer.getSize(),
                                        m_impl->pickingFormat);
        m_impl->pathPipeline = std::make_unique<Piplines::PathPipeline>();
        m_impl->pathPipeline->init(m_impl->device, m_impl->targetFormat,
                                   m_impl->sharedFrameBuffer.getBuffer(),
                                   m_impl->sharedFrameBuffer.getSize(),
                                   m_impl->pickingFormat);
        if (m_impl->primitivePipeline->ensureInstanceBufferSize(
                std::max(1u, createInfo.batching.initialQuadCapacity))) {
            m_impl->recreateTextureBindGroups();
        }
        m_impl->createDefaultTexture();
    }

    void WgpuRenderer2D::destroy() {
        if (m_impl == nullptr) {
            return;
        }
        m_impl->commandEncoder = nullptr;
        if (m_impl->primitivePipeline) {
            m_impl->primitivePipeline->destroy();
            m_impl->primitivePipeline = nullptr;
        }
        if (m_impl->pathPipeline) {
            m_impl->pathPipeline->destroy();
            m_impl->pathPipeline = nullptr;
        }
        m_impl->sharedFrameBuffer.destroy();
        m_impl->textures.clear();
        m_impl->surface = nullptr;
        m_impl->surfaceConfigured = false;
        m_impl->frameUsesSurface = false;
        m_impl->frameTargetTexture = 0;
        m_impl->lastCompletedTargetTexture = 0;
        m_impl->depthTargetView = nullptr;
        m_impl->depthTarget = nullptr;
        m_impl->offscreenTargetView = nullptr;
        m_impl->offscreenTarget = nullptr;
        m_impl->queue = nullptr;
        m_impl->device = nullptr;
        m_impl->adapter = nullptr;
        m_impl->instance = nullptr;
        m_impl->opaquePrimitiveBatch.clear();
        m_impl->transparentPrimitiveBatch.clear();
        m_impl->opaquePathStrokeBatch.clear();
        m_impl->transparentPathStrokeBatch.clear();
        m_impl->opaquePathBatch.clear();
        m_impl->transparentPathBatch.clear();
        m_impl->activePathCommands.clear();
        m_impl->pathStarted = false;
        m_impl->stats = {};
        m_impl->frameStarted = false;
    }

    void WgpuRenderer2D::Impl::createWindowSurface() {
        if (windowHandle == nullptr) {
            return;
        }

        surface = wgpu::Surface(
            glfwCreateWindowWGPUSurface(instance.Get(), windowHandle));
        if (surface == nullptr) {
            throw std::runtime_error("Failed to create WebGPU surface");
        }

        wgpu::SurfaceCapabilities capabilities;
        surface.GetCapabilities(adapter, &capabilities);
        if (capabilities.formatCount == 0 ||
            capabilities.presentModeCount == 0 ||
            capabilities.alphaModeCount == 0) {
            throw std::runtime_error(
                "WebGPU surface reports no supported configuration");
        }

        surfaceFormat = capabilities.formats[0];
        surfacePresentMode = capabilities.presentModes[0];
        surfaceAlphaMode = capabilities.alphaModes[0];
    }

    void WgpuRenderer2D::Impl::configureWindowSurface(uint32_t width,
                                                      uint32_t height) {
        if (surface == nullptr || device == nullptr) {
            return;
        }

        surfaceConfiguration.device = device;
        surfaceConfiguration.usage = wgpu::TextureUsage::RenderAttachment;
        surfaceConfiguration.format = surfaceFormat;
        surfaceConfiguration.presentMode = surfacePresentMode;
        surfaceConfiguration.alphaMode = surfaceAlphaMode;
        surfaceConfiguration.width = std::max(1u, width);
        surfaceConfiguration.height = std::max(1u, height);
        surfaceConfiguration.viewFormatCount = 0;
        surfaceConfiguration.viewFormats = nullptr;

        surface.Configure(&surfaceConfiguration);
        surfaceConfigured = true;
    }

    void WgpuRenderer2D::resize(const Renderer2DExtent &extent) {
        m_impl->extent = extent;
        if (m_impl->device != nullptr) {
            m_impl->createOffscreenTarget();
            m_impl->createDepthTarget();
        }
    }

    void WgpuRenderer2D::beginFrame(
        const Core::Renderer::Renderer2DFrameInfo &frameInfo) {
        if (m_impl->device == nullptr) {
            throw std::runtime_error("WgpuRenderer2D is not initialized");
        }

        if (frameInfo.extent.width != 0 && frameInfo.extent.height != 0 &&
            (frameInfo.extent.width != m_impl->extent.width ||
             frameInfo.extent.height != m_impl->extent.height)) {
            resize(frameInfo.extent);
        }

        m_impl->clearColor = frameInfo.clearColor;
        m_impl->shouldClear = frameInfo.shouldClear;
        m_impl->opaquePrimitiveBatch.clear();
        m_impl->transparentPrimitiveBatch.clear();
        m_impl->opaquePathStrokeBatch.clear();
        m_impl->transparentPathStrokeBatch.clear();
        m_impl->opaquePathBatch.clear();
        m_impl->transparentPathBatch.clear();
        m_impl->activePathCommands.clear();
        m_impl->pathStarted = false;
        m_impl->stats = {};
        m_impl->cameraTransform = nullptr;

        m_impl->frameTargetTexture = frameInfo.targetTexture;
        m_impl->pickingTextureHandle = frameInfo.pickingTexture;
        m_impl->frameUsesSurface = frameInfo.targetTexture == 0;
        m_impl->frameStarted = true;
        m_impl->cameraTransform = frameInfo.cameraTransform;
    }

    void WgpuRenderer2D::endFrame() {
        if (!m_impl->frameStarted) {
            return;
        }

        if (m_impl->pathStarted) {
            endPath();
        }

        m_impl->commandEncoder = m_impl->device.CreateCommandEncoder();

        wgpu::SurfaceTexture surfaceTexture{};
        wgpu::TextureView targetView;

        if (m_impl->frameUsesSurface) {
            m_impl->surface.GetCurrentTexture(&surfaceTexture);
            if (surfaceTexture.status !=
                wgpu::SurfaceGetCurrentTextureStatus::SuccessOptimal) {
                m_impl->commandEncoder = nullptr;
                m_impl->frameStarted = false;
                return;
            }
            targetView = surfaceTexture.texture.CreateView();
        } else if (m_impl->frameTargetTexture != 0) {
            targetView = m_impl->getTexture(m_impl->frameTargetTexture).view;
        } else {
            targetView = m_impl->offscreenTargetView;
        }

        wgpu::RenderPassColorAttachment colorAttachments[2]{};
        uint32_t colorAttachmentCount = 1;

        colorAttachments[0].view = targetView;
        colorAttachments[0].loadOp =
            m_impl->shouldClear ? wgpu::LoadOp::Clear : wgpu::LoadOp::Load;
        colorAttachments[0].storeOp = wgpu::StoreOp::Store;
        colorAttachments[0].clearValue = toWgpuColor(m_impl->clearColor);

        // Attach picking target if available
        if (m_impl->pickingFormat != wgpu::TextureFormat::Undefined &&
            m_impl->pickingTextureHandle != 0) {
            const auto &pickingRes =
                m_impl->getTexture(m_impl->pickingTextureHandle);
            colorAttachments[1].view = pickingRes.view;
            colorAttachments[1].loadOp = wgpu::LoadOp::Clear;
            colorAttachments[1].storeOp = wgpu::StoreOp::Store;
            colorAttachments[1].clearValue = {0.0, 0.0, 0.0, 0.0};
            colorAttachmentCount = 2;
        }

        wgpu::RenderPassDepthStencilAttachment depthAttachment{};
        depthAttachment.view = m_impl->depthTargetView;
        depthAttachment.depthLoadOp =
            m_impl->shouldClear ? wgpu::LoadOp::Clear : wgpu::LoadOp::Load;
        depthAttachment.depthStoreOp = wgpu::StoreOp::Store;
        depthAttachment.depthClearValue = 1.0f;
        depthAttachment.stencilLoadOp = wgpu::LoadOp::Clear;
        depthAttachment.stencilStoreOp = wgpu::StoreOp::Store;
        depthAttachment.stencilClearValue = 0;

        wgpu::RenderPassDescriptor renderPassDescriptor{};
        renderPassDescriptor.colorAttachmentCount = colorAttachmentCount;
        renderPassDescriptor.colorAttachments = colorAttachments;
        renderPassDescriptor.depthStencilAttachment = &depthAttachment;

        m_impl->opaquePrimitiveBatch.prepareForRendering(false);
        m_impl->transparentPrimitiveBatch.prepareForRendering(true);
        m_impl->opaquePathStrokeBatch.prepareForRendering(false);
        m_impl->transparentPathStrokeBatch.prepareForRendering(true);
        m_impl->opaquePathBatch.prepareForRendering(false);
        m_impl->transparentPathBatch.prepareForRendering(true);

        const uint32_t opaqueInstanceOffset = 0;
        const uint32_t transparentInstanceOffset =
            m_impl->opaquePrimitiveBatch.count();
        const uint32_t totalInstanceCount =
            transparentInstanceOffset +
            m_impl->transparentPrimitiveBatch.count();

        const uint32_t opaqueStencilVertexOffset = 0;
        const uint32_t transparentStencilVertexOffset =
            m_impl->opaquePathBatch.stencilVertexCount();
        const uint32_t totalStencilVertexCount =
            transparentStencilVertexOffset +
            m_impl->transparentPathBatch.stencilVertexCount();

        const uint32_t opaqueCoverVertexOffset = 0;
        const uint32_t transparentCoverVertexOffset =
            m_impl->opaquePathBatch.coverVertexCount();
        const uint32_t totalCoverVertexCount =
            transparentCoverVertexOffset +
            m_impl->transparentPathBatch.coverVertexCount();

        const uint32_t opaqueStrokeVertexOffset = 0;
        const uint32_t transparentStrokeVertexOffset =
            m_impl->opaquePathStrokeBatch.vertexCount();
        const uint32_t totalStrokeVertexCount =
            transparentStrokeVertexOffset +
            m_impl->transparentPathStrokeBatch.vertexCount();

        if (totalInstanceCount > 0 &&
            m_impl->primitivePipeline->ensureInstanceBufferSize(
                totalInstanceCount)) {
            m_impl->recreateTextureBindGroups();
        }

        if (!m_impl->opaquePrimitiveBatch.empty()) {
            m_impl->primitivePipeline->uploadInstances(
                m_impl->queue, m_impl->opaquePrimitiveBatch.data(),
                m_impl->opaquePrimitiveBatch.byteSize(),
                opaqueInstanceOffset * sizeof(Piplines::PrimitiveInstance));
            m_impl->stats.uploadedBytes +=
                m_impl->opaquePrimitiveBatch.byteSize();
        }

        if (!m_impl->transparentPrimitiveBatch.empty()) {
            m_impl->primitivePipeline->uploadInstances(
                m_impl->queue, m_impl->transparentPrimitiveBatch.data(),
                m_impl->transparentPrimitiveBatch.byteSize(),
                transparentInstanceOffset *
                    sizeof(Piplines::PrimitiveInstance));
            m_impl->stats.uploadedBytes +=
                m_impl->transparentPrimitiveBatch.byteSize();
        }

        m_impl->stats.quadCount = totalInstanceCount;

        if (totalStencilVertexCount > 0) {
            static_cast<void>(
                m_impl->pathPipeline->ensureStencilVertexBufferSize(
                    totalStencilVertexCount));
        }

        if (totalCoverVertexCount > 0) {
            static_cast<void>(m_impl->pathPipeline->ensureCoverVertexBufferSize(
                totalCoverVertexCount));
        }

        if (totalStrokeVertexCount > 0) {
            static_cast<void>(
                m_impl->pathPipeline->ensureStrokeVertexBufferSize(
                    totalStrokeVertexCount));
        }

        if (!m_impl->opaquePathBatch.empty()) {
            m_impl->pathPipeline->uploadStencilVertices(
                m_impl->queue, m_impl->opaquePathBatch.stencilData(),
                m_impl->opaquePathBatch.stencilByteSize(),
                opaqueStencilVertexOffset * sizeof(PathStencilVertex));
            m_impl->pathPipeline->uploadCoverVertices(
                m_impl->queue, m_impl->opaquePathBatch.coverData(),
                m_impl->opaquePathBatch.coverByteSize(),
                opaqueCoverVertexOffset * sizeof(PathCoverVertex));
            m_impl->stats.uploadedBytes +=
                m_impl->opaquePathBatch.stencilByteSize() +
                m_impl->opaquePathBatch.coverByteSize();
        }

        if (!m_impl->transparentPathBatch.empty()) {
            m_impl->pathPipeline->uploadStencilVertices(
                m_impl->queue, m_impl->transparentPathBatch.stencilData(),
                m_impl->transparentPathBatch.stencilByteSize(),
                transparentStencilVertexOffset * sizeof(PathStencilVertex));
            m_impl->pathPipeline->uploadCoverVertices(
                m_impl->queue, m_impl->transparentPathBatch.coverData(),
                m_impl->transparentPathBatch.coverByteSize(),
                transparentCoverVertexOffset * sizeof(PathCoverVertex));
            m_impl->stats.uploadedBytes +=
                m_impl->transparentPathBatch.stencilByteSize() +
                m_impl->transparentPathBatch.coverByteSize();
        }

        if (!m_impl->opaquePathStrokeBatch.empty()) {
            m_impl->pathPipeline->uploadStrokeVertices(
                m_impl->queue, m_impl->opaquePathStrokeBatch.data(),
                m_impl->opaquePathStrokeBatch.byteSize(),
                opaqueStrokeVertexOffset * sizeof(PathCoverVertex));
            m_impl->stats.uploadedBytes +=
                m_impl->opaquePathStrokeBatch.byteSize();
        }

        if (!m_impl->transparentPathStrokeBatch.empty()) {
            m_impl->pathPipeline->uploadStrokeVertices(
                m_impl->queue, m_impl->transparentPathStrokeBatch.data(),
                m_impl->transparentPathStrokeBatch.byteSize(),
                transparentStrokeVertexOffset * sizeof(PathCoverVertex));
            m_impl->stats.uploadedBytes +=
                m_impl->transparentPathStrokeBatch.byteSize();
        }

        m_impl->sharedFrameBuffer.update(m_impl->queue, m_impl->extent.width,
                                         m_impl->extent.height);
        m_impl->sharedFrameBuffer.setCameraTransform(m_impl->cameraTransform);

        wgpu::RenderPassEncoder renderPass =
            m_impl->commandEncoder.BeginRenderPass(&renderPassDescriptor);

        auto renderBatch = [&](PrimitiveBatch &batch, uint32_t instanceOffset,
                               const wgpu::RenderPipeline &pipeline) {
            if (batch.empty()) {
                return;
            }

            renderPass.SetPipeline(pipeline);

            const uint32_t runCount = batch.drawRunsCount();
            const DrawRun *runs = batch.drawRunsData();
            for (uint32_t i = 0; i < runCount; ++i) {
                const auto &run = runs[i];
                const auto &texture = m_impl->getTexture(run.texture);
                renderPass.SetBindGroup(0, texture.bindGroup);
                renderPass.Draw(6, run.instanceCount, 0,
                                instanceOffset + run.firstInstance);
                m_impl->stats.drawCallCount++;
            }
        };

        auto renderPathBatch = [&](const PathBatch &batch,
                                   uint32_t stencilVertexOffset,
                                   uint32_t coverVertexOffset,
                                   bool transparent) {
            if (batch.empty()) {
                return;
            }

            const PathDrawRange *ranges = batch.drawRanges();
            const uint32_t rangeCount = batch.drawCount();
            for (uint32_t i = 0; i < rangeCount; ++i) {
                const auto &range = ranges[i];
                m_impl->pathPipeline->drawPath(
                    renderPass, stencilVertexOffset + range.firstStencilVertex,
                    range.stencilVertexCount,
                    coverVertexOffset + range.firstCoverVertex,
                    range.coverVertexCount, transparent);
                m_impl->stats.drawCallCount += 2;
            }
        };

        auto renderPathStrokeBatch = [&](const PathStrokeBatch &batch,
                                         uint32_t vertexOffset,
                                         bool transparent) {
            if (batch.empty()) {
                return;
            }

            const PathStrokeDrawRange *ranges = batch.drawRanges();
            const uint32_t rangeCount = batch.drawCount();
            for (uint32_t i = 0; i < rangeCount; ++i) {
                const auto &range = ranges[i];
                m_impl->pathPipeline->drawStroke(
                    renderPass, vertexOffset + range.firstVertex,
                    range.vertexCount, transparent);
                m_impl->stats.drawCallCount++;
            }
        };

        renderBatch(m_impl->opaquePrimitiveBatch, opaqueInstanceOffset,
                    m_impl->primitivePipeline->getOpaquePipeline());
        renderPathBatch(m_impl->opaquePathBatch, opaqueStencilVertexOffset,
                        opaqueCoverVertexOffset, false);
        renderPathStrokeBatch(m_impl->opaquePathStrokeBatch,
                              opaqueStrokeVertexOffset, false);
        renderBatch(m_impl->transparentPrimitiveBatch,
                    transparentInstanceOffset,
                    m_impl->primitivePipeline->getTransparentPipeline());
        renderPathBatch(m_impl->transparentPathBatch,
                        transparentStencilVertexOffset,
                        transparentCoverVertexOffset, true);
        renderPathStrokeBatch(m_impl->transparentPathStrokeBatch,
                              transparentStrokeVertexOffset, true);

        renderPass.End();
        wgpu::CommandBuffer commandBuffer = m_impl->commandEncoder.Finish();
        m_impl->queue.Submit(1, &commandBuffer);

        if (m_impl->frameUsesSurface) {
            m_impl->surface.Present();
        }

        m_impl->commandEncoder = nullptr;
        m_impl->frameStarted = false;
        m_impl->lastCompletedTargetTexture = m_impl->frameTargetTexture;
        m_impl->frameTargetTexture = 0;
        m_impl->frameUsesSurface = false;
    }

    void WgpuRenderer2D::clear(const Color &color) {
        m_impl->clearColor = color;
        m_impl->shouldClear = true;
    }

    void WgpuRenderer2D::saveTargetToFile(const std::string &path) {
        if (m_impl->device == nullptr) {
            throw std::runtime_error("WgpuRenderer2D is not initialized");
        }

        if (m_impl->frameStarted) {
            endFrame();
        }

        if (!isPngWritableFormat(m_impl->targetFormat)) {
            throw std::runtime_error(
                "saveTargetToFile currently supports only 8-bit RGBA/BGRA "
                "render targets");
        }

        const auto targetTexture =
            m_impl->lastCompletedTargetTexture != 0
                ? m_impl->getTexture(m_impl->lastCompletedTargetTexture).texture
                : m_impl->offscreenTarget;

        const uint32_t width = std::max(1u, m_impl->extent.width);
        const uint32_t height = std::max(1u, m_impl->extent.height);
        const uint32_t bytesPerPixel = 4;
        const uint32_t unpaddedBytesPerRow = width * bytesPerPixel;
        const uint32_t paddedBytesPerRow = alignTo(unpaddedBytesPerRow, 256);
        const auto readbackSize =
            static_cast<uint64_t>(paddedBytesPerRow) * height;

        wgpu::BufferDescriptor bufferDescriptor{};
        bufferDescriptor.size = readbackSize;
        bufferDescriptor.usage =
            wgpu::BufferUsage::MapRead | wgpu::BufferUsage::CopyDst;
        wgpu::Buffer readbackBuffer =
            m_impl->device.CreateBuffer(&bufferDescriptor);

        wgpu::CommandEncoder encoder = m_impl->device.CreateCommandEncoder();

        wgpu::TexelCopyTextureInfo source{};
        source.texture = targetTexture;
        source.mipLevel = 0;
        source.origin = {0, 0, 0};
        source.aspect = wgpu::TextureAspect::All;

        wgpu::TexelCopyBufferInfo destination{};
        destination.buffer = readbackBuffer;
        destination.layout.offset = 0;
        destination.layout.bytesPerRow = paddedBytesPerRow;
        destination.layout.rowsPerImage = height;

        wgpu::Extent3D copySize{width, height, 1};
        encoder.CopyTextureToBuffer(&source, &destination, &copySize);

        wgpu::CommandBuffer commandBuffer = encoder.Finish();
        m_impl->queue.Submit(1, &commandBuffer);

        wgpu::MapAsyncStatus mapStatus = wgpu::MapAsyncStatus::Error;
        std::string mapError;
        auto mapCallback = [&mapStatus, &mapError](wgpu::MapAsyncStatus status,
                                                   wgpu::StringView message) {
            mapStatus = status;
            if (status != wgpu::MapAsyncStatus::Success &&
                message.data != nullptr) {
                mapError.assign(message.data, message.length);
            }
        };

        wgpu::Future mapFuture = readbackBuffer.MapAsync(
            wgpu::MapMode::Read, 0, readbackSize,
            wgpu::CallbackMode::WaitAnyOnly, mapCallback);
        if (m_impl->instance.WaitAny(mapFuture, UINT64_MAX) !=
            wgpu::WaitStatus::Success) {
            throw std::runtime_error("Timed out waiting for WGPU readback");
        }
        if (mapStatus != wgpu::MapAsyncStatus::Success) {
            throw std::runtime_error("Failed to map WGPU readback buffer: " +
                                     mapError);
        }

        const auto *mappedData = static_cast<const uint8_t *>(
            readbackBuffer.GetConstMappedRange(0, readbackSize));
        if (mappedData == nullptr) {
            readbackBuffer.Unmap();
            throw std::runtime_error("Failed to access WGPU readback data");
        }

        std::vector<uint8_t> rgba(static_cast<size_t>(width) * height *
                                  bytesPerPixel);
        for (uint32_t row = 0; row < height; ++row) {
            const uint8_t *src =
                mappedData + (static_cast<size_t>(row) * paddedBytesPerRow);
            uint8_t *dst =
                rgba.data() + (static_cast<size_t>(row) * unpaddedBytesPerRow);

            if (m_impl->targetFormat == wgpu::TextureFormat::BGRA8Unorm) {
                for (uint32_t col = 0; col < width; ++col) {
                    const uint32_t offset = col * bytesPerPixel;
                    dst[offset + 0] = src[offset + 2];
                    dst[offset + 1] = src[offset + 1];
                    dst[offset + 2] = src[offset + 0];
                    dst[offset + 3] = src[offset + 3];
                }
            } else {
                std::copy(src, src + unpaddedBytesPerRow, dst);
            }
        }

        readbackBuffer.Unmap();
        writePng(path, rgba.data(), width, height);
    }

    Core::Renderer::Renderer2DStats WgpuRenderer2D::getStats() const noexcept {
        return m_impl->stats;
    }

    void
    WgpuRenderer2D::unregisterTexture(Core::Renderer::TextureHandle texture) {
        m_impl->textures.erase(texture);
        m_impl->recreateTextureBindGroups();
    }

    void WgpuRenderer2D::registerTexture(const TextureResource &texture) {
        m_impl->textures[texture.handle] = texture;
        m_impl->recreateTextureBindGroups();
    }

    void WgpuRenderer2D::drawQuad(const Core::Renderer::QuadProps &props) {
        if (!m_impl->frameStarted) {
            return;
        }

        if (isTransparent(props, nullptr)) {
            makePrimitiveInstanceInPlace(
                m_impl->transparentPrimitiveBatch.push(props.texture), props,
                nullptr);
        } else {
            makePrimitiveInstanceInPlace(
                m_impl->opaquePrimitiveBatch.push(props.texture), props,
                nullptr);
        }
        m_impl->stats.quadCount = m_impl->primitiveStatsCount();
    }

    void WgpuRenderer2D::drawRoundedQuad(
        const Core::Renderer::QuadProps &props,
        const Core::Renderer::RoundedBorderProps &roundedProps) {
        if (!m_impl->frameStarted) {
            return;
        }

        if (isTransparent(props, &roundedProps)) {
            makePrimitiveInstanceInPlace(
                m_impl->transparentPrimitiveBatch.push(props.texture), props,
                &roundedProps);
        } else {
            makePrimitiveInstanceInPlace(
                m_impl->opaquePrimitiveBatch.push(props.texture), props,
                &roundedProps);
        }
        m_impl->stats.quadCount = m_impl->primitiveStatsCount();
    }

    void WgpuRenderer2D::drawCircle(const Core::Renderer::CircleProps &props) {
        if (!m_impl->frameStarted) {
            return;
        }

        if (props.color.a < 1.0f) {
            makeCircleInstanceInPlace(m_impl->transparentPrimitiveBatch.push(0),
                                      props);
        } else {
            makeCircleInstanceInPlace(m_impl->opaquePrimitiveBatch.push(0),
                                      props);
        }
        m_impl->stats.quadCount = m_impl->primitiveStatsCount();
    }

    void WgpuRenderer2D::drawLine(const Core::Renderer::LineProps &props) {
        if (!m_impl->frameStarted) {
            return;
        }

        if (props.color.a < 1.0f) {
            makeLineInstanceInPlace(m_impl->transparentPrimitiveBatch.push(0),
                                    props);
        } else {
            makeLineInstanceInPlace(m_impl->opaquePrimitiveBatch.push(0),
                                    props);
        }
        m_impl->stats.quadCount = m_impl->primitiveStatsCount();
    }

    void WgpuRenderer2D::beginPath(const PathProps &props) {
        if (!m_impl->frameStarted) {
            return;
        }

        if (m_impl->pathStarted) {
            endPath();
        }

        m_impl->activePathCommands.clear();
        m_impl->activePathProps = props;
        m_impl->pathStarted = true;
    }

    void WgpuRenderer2D::pathMoveTo(const glm::vec2 &pos) {
        if (!m_impl->frameStarted) {
            return;
        }
        if (!m_impl->pathStarted) {
            beginPath();
        }

        m_impl->activePathCommands.push_back(
            {.kind = PathCommandKind::Move, .p = pos});
    }

    void WgpuRenderer2D::pathLineTo(const glm::vec2 &pos) {
        if (!m_impl->frameStarted) {
            return;
        }
        if (!m_impl->pathStarted) {
            beginPath();
        }

        m_impl->activePathCommands.push_back(
            {.kind = PathCommandKind::Line, .p = pos});
    }

    void WgpuRenderer2D::pathQuadTo(const glm::vec2 &control,
                                    const glm::vec2 &pos) {
        if (!m_impl->frameStarted) {
            return;
        }
        if (!m_impl->pathStarted) {
            beginPath();
        }

        m_impl->activePathCommands.push_back(
            {.kind = PathCommandKind::Quad, .p = pos, .control = control});
    }

    void WgpuRenderer2D::pathQuadraticTo(const glm::vec2 &control,
                                         const glm::vec2 &pos) {
        pathQuadTo(control, pos);
    }

    void WgpuRenderer2D::pathCubicTo(const glm::vec2 &control1,
                                     const glm::vec2 &control2,
                                     const glm::vec2 &pos) {
        if (!m_impl->frameStarted) {
            return;
        }
        if (!m_impl->pathStarted) {
            beginPath();
        }

        m_impl->activePathCommands.push_back({.kind = PathCommandKind::Cubic,
                                              .p = pos,
                                              .control = control1,
                                              .control2 = control2});
    }

    void WgpuRenderer2D::pathCubicBezierTo(const glm::vec2 &control1,
                                           const glm::vec2 &control2,
                                           const glm::vec2 &pos) {
        pathCubicTo(control1, control2, pos);
    }

    void WgpuRenderer2D::pathBezierCurveTo(const glm::vec2 &control1,
                                           const glm::vec2 &control2,
                                           const glm::vec2 &pos) {
        pathCubicTo(control1, control2, pos);
    }

    void WgpuRenderer2D::endPath() {
        if (!m_impl->pathStarted) {
            return;
        }

        if (hasPathFill(m_impl->activePathProps)) {
            BakedPath baked =
                bakePath(m_impl->activePathCommands, m_impl->activePathProps);
            if (baked.valid) {
                if (isFillTransparent(m_impl->activePathProps)) {
                    m_impl->transparentPathBatch.push(
                        std::move(baked), m_impl->activePathProps.zIndex);
                } else {
                    m_impl->opaquePathBatch.push(
                        std::move(baked), m_impl->activePathProps.zIndex);
                }
            }
        }

        if (hasPathStroke(m_impl->activePathProps)) {
            PathStrokeBatch &strokeBatch =
                isStrokeTransparent(m_impl->activePathProps)
                    ? m_impl->transparentPathStrokeBatch
                    : m_impl->opaquePathStrokeBatch;
            strokeBatch.push(bakePathStroke(m_impl->activePathCommands,
                                            m_impl->activePathProps),
                             m_impl->activePathProps.zIndex);
        }

        m_impl->activePathCommands.clear();
        m_impl->pathStarted = false;
    }

    void WgpuRenderer2D::drawImGui(
        const std::function<void(void *)> &imguiRenderFn) {

        // if someframe is already started skip ui
        if (m_impl->frameStarted) {
            return;
        }

        m_impl->commandEncoder = m_impl->device.CreateCommandEncoder();

        wgpu::RenderPassColorAttachment colorAttachment{};
        colorAttachment.view = m_impl->offscreenTargetView;
        colorAttachment.loadOp = wgpu::LoadOp::Load;
        colorAttachment.storeOp = wgpu::StoreOp::Store;

        wgpu::RenderPassDescriptor renderPassDescriptor{};
        renderPassDescriptor.colorAttachmentCount = 1;
        renderPassDescriptor.colorAttachments = &colorAttachment;

        wgpu::RenderPassEncoder renderPass =
            m_impl->commandEncoder.BeginRenderPass(&renderPassDescriptor);

        imguiRenderFn(renderPass.Get());

        renderPass.End();
        wgpu::CommandBuffer commandBuffer = m_impl->commandEncoder.Finish();
        m_impl->queue.Submit(1, &commandBuffer);

        m_impl->commandEncoder = nullptr;
    }

    void
    WgpuRenderer2D::drawToWindow(const std::shared_ptr<Window> &window,
                                 const std::function<void(void *)> &renderFn) {
        if (m_impl->surface == nullptr || m_impl->windowHandle == nullptr ||
            m_impl->device == nullptr) {
            return;
        }

        int width = 0;
        int height = 0;
        glfwGetFramebufferSize(m_impl->windowHandle, &width, &height);
        if (width <= 0 || height <= 0) {
            return;
        }

        if (!m_impl->surfaceConfigured ||
            m_impl->surfaceConfiguration.width !=
                static_cast<uint32_t>(width) ||
            m_impl->surfaceConfiguration.height !=
                static_cast<uint32_t>(height)) {
            m_impl->configureWindowSurface(static_cast<uint32_t>(width),
                                           static_cast<uint32_t>(height));
        }

        wgpu::SurfaceTexture surfaceTexture;
        m_impl->surface.GetCurrentTexture(&surfaceTexture);
        if (surfaceTexture.status !=
            wgpu::SurfaceGetCurrentTextureStatus::SuccessOptimal) {
            return;
        }

        wgpu::TextureView targetView = surfaceTexture.texture.CreateView();

        m_impl->commandEncoder = m_impl->device.CreateCommandEncoder();

        wgpu::RenderPassColorAttachment colorAttachment{};
        colorAttachment.view = targetView;
        colorAttachment.loadOp = wgpu::LoadOp::Load;
        colorAttachment.storeOp = wgpu::StoreOp::Store;

        wgpu::RenderPassDescriptor renderPassDescriptor{};
        renderPassDescriptor.colorAttachmentCount = 1;
        renderPassDescriptor.colorAttachments = &colorAttachment;

        wgpu::RenderPassEncoder renderPass =
            m_impl->commandEncoder.BeginRenderPass(&renderPassDescriptor);

        renderFn(renderPass.Get());

        renderPass.End();
        wgpu::CommandBuffer commandBuffer = m_impl->commandEncoder.Finish();
        m_impl->queue.Submit(1, &commandBuffer);
        m_impl->surface.Present();

        m_impl->commandEncoder = nullptr;
    }

    wgpu::Device WgpuRenderer2D::getDevice() const { return m_impl->device; }
    wgpu::Queue WgpuRenderer2D::getQueue() const { return m_impl->queue; }

    wgpu::TextureView WgpuRenderer2D::getCurrentTargetView() const {
        return m_impl->offscreenTargetView;
    }

    [[nodiscard]] wgpu::TextureFormat WgpuRenderer2D::getTargetFormat() const {
        return m_impl->targetFormat;
    }

    [[nodiscard]] Core::Renderer::Renderer2DTargetFormat
    WgpuRenderer2D::getTargetFormatType() const {
        return m_impl->targetFormatType;
    }

    [[nodiscard]] wgpu::TextureFormat WgpuRenderer2D::getSurfaceFormat() const {
        return m_impl->surfaceFormat;
    }

} // namespace Bess::Wgpu
