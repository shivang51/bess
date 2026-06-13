#pragma once

#include "bess_wgpu/piplines/pipeline.h"
#include "bess_wgpu/wgpu_shader.h"
#include <cstddef>
#include <cstdint>
#include <memory>
#include <webgpu/webgpu_cpp.h>

namespace Bess::Wgpu::Piplines {

    struct ShadowInstance {
        float position[3] = {0.f, 0.f, 0.f};
        float rotation = 0.f;
        float size[2] = {1.f, 1.f};
        float blur = 8.f;
        float spread = 0.f;
        float color[4] = {0.f, 0.f, 0.f, 0.f};
        float radii[4] = {0.f, 0.f, 0.f, 0.f};
        float shapeData[4] = {1.f, 1.f, 0.f, 0.f};
        uint32_t shapeType = 0;
        uint32_t flags = 0;
        uint32_t padding[2] = {0, 0};
    };

    static_assert(sizeof(ShadowInstance) == 96,
                  "ShadowInstance must match WGSL layout");

    class ShadowPipeline final : public Pipeline {
      public:
        void init(const wgpu::Device &device,
                  wgpu::TextureFormat targetFormat,
                  const wgpu::Buffer &frameBuffer,
                  uint64_t frameBufferSize,
                  wgpu::TextureFormat pickingFormat =
                      wgpu::TextureFormat::Undefined) override;
        void destroy() override;

        [[nodiscard]] bool ensureInstanceBufferSize(std::size_t shadowCount);

        void uploadInstances(const wgpu::Queue &queue,
                             const ShadowInstance *instances,
                             uint64_t byteSize) const;

        [[nodiscard]] const wgpu::RenderPipeline &getPipeline() const;

        [[nodiscard]] const wgpu::BindGroup &getBindGroup() const;

        void drawInstances(wgpu::RenderPassEncoder &renderPass,
                           uint32_t firstInstance,
                           uint32_t instanceCount) const;

        void draw(wgpu::RenderPassEncoder &renderPass,
                  uint32_t firstInstance,
                  uint32_t instanceCount) const;

      private:
        void createShader();
        void createBindGroupLayout();
        void createPipelineLayout();
        void createBindGroup();
        void createPipelineState();

        wgpu::Device m_device;
        wgpu::TextureFormat m_targetFormat = wgpu::TextureFormat::BGRA8Unorm;
        wgpu::TextureFormat m_pickingFormat = wgpu::TextureFormat::Undefined;
        std::unique_ptr<Bess::Wgpu::WgpuShader> m_shader;
        wgpu::Buffer m_instanceBuffer;
        uint64_t m_instanceBufferSize = 0;
        wgpu::Buffer m_frameBuffer;
        uint64_t m_frameBufferSize = 0;
        wgpu::BindGroupLayout m_bindGroupLayout;
        wgpu::PipelineLayout m_pipelineLayout;
        wgpu::BindGroup m_bindGroup;
        wgpu::RenderPipeline m_pipeline;
    };

} // namespace Bess::Wgpu::Piplines
