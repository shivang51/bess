#pragma once

#include "bess_wgpu/piplines/pipeline.h"
#include "bess_wgpu/wgpu_shader.h"
#include <cstddef>
#include <cstdint>
#include <memory>
#include <webgpu/webgpu_cpp.h>

namespace Bess::Wgpu::Piplines {

    struct PathStencilVertex {
        float position[3] = {0.f, 0.f, 0.f};
        float curveCoord[2] = {0.f, 0.f};
        uint32_t curveType = 0;
    };

    struct PathCoverVertex {
        float position[3] = {0.f, 0.f, 0.f};
        float color[4] = {1.f, 1.f, 1.f, 1.f};
        uint32_t id[2] = {0, 0};
    };

    class PathPipeline final : public Pipeline {
      public:
        void init(const wgpu::Device &device, wgpu::TextureFormat targetFormat,
                  const wgpu::Buffer &frameBuffer, uint64_t frameBufferSize,
                  wgpu::TextureFormat pickingFormat =
                      wgpu::TextureFormat::Undefined) override;
        void destroy() override;

        [[nodiscard]] bool
        ensureStencilVertexBufferSize(std::size_t vertexCount);
        [[nodiscard]] bool ensureCoverVertexBufferSize(std::size_t vertexCount);

        void uploadStencilVertices(const wgpu::Queue &queue,
                                   const PathStencilVertex *vertices,
                                   uint64_t byteSize,
                                   uint64_t bufferOffset = 0) const;
        void uploadCoverVertices(const wgpu::Queue &queue,
                                 const PathCoverVertex *vertices,
                                 uint64_t byteSize,
                                 uint64_t bufferOffset = 0) const;

        void drawPath(wgpu::RenderPassEncoder &renderPass,
                      uint32_t firstStencilVertex, uint32_t stencilVertexCount,
                      uint32_t firstCoverVertex, uint32_t coverVertexCount,
                      bool transparent) const;

      private:
        void createShader();
        void createBindGroupLayout();
        void createBindGroup();
        void createPipelineState();

        wgpu::Device m_device;
        wgpu::TextureFormat m_targetFormat = wgpu::TextureFormat::BGRA8Unorm;
        wgpu::TextureFormat m_pickingFormat = wgpu::TextureFormat::Undefined;
        std::unique_ptr<Bess::Wgpu::WgpuShader> m_shader;
        wgpu::RenderPipeline m_stencilPipeline;
        wgpu::RenderPipeline m_opaqueCoverPipeline;
        wgpu::RenderPipeline m_transparentCoverPipeline;
        wgpu::BindGroupLayout m_bindGroupLayout;
        wgpu::BindGroup m_bindGroup;
        wgpu::Buffer m_frameBuffer;
        uint64_t m_frameBufferSize = 0;
        wgpu::Buffer m_stencilVertexBuffer;
        uint64_t m_stencilVertexBufferSize = 0;
        wgpu::Buffer m_coverVertexBuffer;
        uint64_t m_coverVertexBufferSize = 0;
    };

} // namespace Bess::Wgpu::Piplines
