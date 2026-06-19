#pragma once

#include "bess_core/renderer/renderer_2d.h"
#include "bess_core/renderer/renderer_path.h"
#include "bess_core/renderer/renderer_types.h"
#include "bess_wgpu/piplines/path_pipeline.h"
#include "common/bess_api.h"
#include <array>
#include <span>
#include <vector>

namespace Bess::Wgpu {

    using Core::Renderer::PathCommand;
    using Core::Renderer::PathProps;
    using Core::Renderer::Renderer2DExtent;

    struct PathDrawRange {
        uint32_t firstStencilVertex = 0;
        uint32_t stencilVertexCount = 0;
        uint32_t firstCoverVertex = 0;
        uint32_t coverVertexCount = 0;
        uint32_t firstInstance = 0;
        bool evenOddFill = true;
        float zIndex = 0.f;
        uint64_t submitOrder = 0;
    };

    struct BakedPath {
        std::vector<Piplines::PathStencilVertex> stencilVertices;
        std::array<Piplines::PathCoverVertex, 6> coverVertices{};
        bool evenOddFill = true;
        bool valid = false;
    };

    struct PathBakeMetrics {
        float screenScale = 1.f;
        float pixelWorldSize = 1.f;
    };

    struct StrokeMeshParams {
        PathBakeMetrics metrics;
        float halfWidth = 0.5f;
        float fringe = 1.f;
        float overlap = 1.f;
    };

    struct StyledStrokeSegment {
        glm::vec2 from{0.f};
        glm::vec2 to{0.f};
        float fromHalfWidth = 0.5f;
        float toHalfWidth = 0.5f;
        PickingId id = PickingId::invalid();
    };

    struct BakedPathSubmission {
        BakedPath fill;
        std::vector<Piplines::PathCoverVertex> strokeVertices;
        bool fillTransparent = false;
        bool strokeTransparent = false;
    };

    class PathBatch {
      public:
        void clear();

        void push(const BakedPath &path,
                  const PathProps &props,
                  uint64_t submitOrder);
        void
        push(BakedPath &&path, const PathProps &props, uint64_t submitOrder);

        void prepareForRendering(bool sortBackToFront);

        [[nodiscard]] bool empty() const noexcept;
        [[nodiscard]] uint32_t drawCount() const noexcept;
        [[nodiscard]] uint32_t stencilVertexCount() const noexcept;
        [[nodiscard]] uint32_t coverVertexCount() const noexcept;
        [[nodiscard]] uint32_t instanceCount() const noexcept;
        [[nodiscard]] uint64_t stencilByteSize() const noexcept;
        [[nodiscard]] uint64_t coverByteSize() const noexcept;
        [[nodiscard]] uint64_t instanceByteSize() const noexcept;

        [[nodiscard]] const Piplines::PathStencilVertex *
        stencilData() const noexcept;
        [[nodiscard]] const Piplines::PathCoverVertex *
        coverData() const noexcept;
        [[nodiscard]] const Piplines::PathInstance *
        instanceData() const noexcept;
        [[nodiscard]] const PathDrawRange *drawRanges() const noexcept;

      private:
        std::vector<Piplines::PathStencilVertex> m_stencilVertices;
        std::vector<Piplines::PathCoverVertex> m_coverVertices;
        std::vector<Piplines::PathInstance> m_instances;
        std::vector<PathDrawRange> m_drawRanges;
    };

    struct PathStrokeDrawRange {
        uint32_t firstVertex = 0;
        uint32_t vertexCount = 0;
        uint32_t firstInstance = 0;
        float zIndex = 0.f;
        uint64_t submitOrder = 0;
    };

    class PathStrokeBatch {
      public:
        void clear();

        void push(const std::vector<Piplines::PathCoverVertex> &vertices,
                  const PathProps &props,
                  uint64_t submitOrder);
        void push(std::vector<Piplines::PathCoverVertex> &&vertices,
                  const PathProps &props,
                  uint64_t submitOrder);

        void prepareForRendering(bool sortBackToFront);

        [[nodiscard]] bool empty() const noexcept;
        [[nodiscard]] uint32_t drawCount() const noexcept;
        [[nodiscard]] uint32_t vertexCount() const noexcept;
        [[nodiscard]] uint32_t instanceCount() const noexcept;
        [[nodiscard]] uint64_t byteSize() const noexcept;
        [[nodiscard]] uint64_t instanceByteSize() const noexcept;

        [[nodiscard]] const Piplines::PathCoverVertex *data() const noexcept;
        [[nodiscard]] const Piplines::PathInstance *
        instanceData() const noexcept;
        [[nodiscard]] const PathStrokeDrawRange *drawRanges() const noexcept;

      private:
        std::vector<Piplines::PathCoverVertex> m_vertices;
        std::vector<Piplines::PathInstance> m_instances;
        std::vector<PathStrokeDrawRange> m_drawRanges;
    };

    [[nodiscard]] PathBakeMetrics
    makePathBakeMetrics(const float *cameraTransform,
                        const Renderer2DExtent &extent);

    [[nodiscard]] BakedPathSubmission BESS_API
    bakePathSubmission(std::span<const PathCommand> commands,
                       const PathProps &props,
                       const PathBakeMetrics &metrics);

    void BESS_API
    submitBakedPathSubmission(const BakedPathSubmission &submission,
                              const PathProps &props,
                              uint64_t submitOrder,
                              PathBatch &opaquePathBatch,
                              PathBatch &transparentPathBatch,
                              PathStrokeBatch &opaquePathStrokeBatch,
                              PathStrokeBatch &transparentPathStrokeBatch);

    void BESS_API
    submitPathCommands(std::span<const PathCommand> commands,
                       const PathProps &props,
                       const PathBakeMetrics &metrics,
                       uint64_t submitOrder,
                       PathBatch &opaquePathBatch,
                       PathBatch &transparentPathBatch,
                       PathStrokeBatch &opaquePathStrokeBatch,
                       PathStrokeBatch &transparentPathStrokeBatch);

    void BESS_API
    submitPathCommands(std::span<const PathCommand> commands,
                       const PathProps &props,
                       const PathBakeMetrics &metrics,
                       PathBatch &opaquePathBatch,
                       PathBatch &transparentPathBatch,
                       PathStrokeBatch &opaquePathStrokeBatch,
                       PathStrokeBatch &transparentPathStrokeBatch);

    std::vector<Piplines::PathCoverVertex>
        BESS_API bakePathFillAntiAlias(std::span<const PathCommand> commands,
                                       const PathProps &props,
                                       const PathBakeMetrics &metrics,
                                       float fringeScale);

    StrokeMeshParams BESS_API makeStrokeMeshParams(
        const PathProps &props, const PathBakeMetrics &metrics);

} // namespace Bess::Wgpu
