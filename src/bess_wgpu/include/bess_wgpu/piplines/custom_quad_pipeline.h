#pragma once

#include "common/bess_api.h"

#include "bess_core/renderer/renderer_2d.h"
#include "bess_wgpu/wgpu_shader.h"
#include "webgpu/webgpu_cpp.h"

namespace Bess {
    using CustomQuadShaderHandle = Core::Renderer::CustomQuadShaderHandle;
    using CustomQuadShaderDesc = Core::Renderer::CustomQuadShaderDesc;
    using CustomQuadProps = Core::Renderer::CustomQuadProps;

    struct BESS_API CustomQuadInstance {
        float position[3] = {0.f, 0.f, 0.f};
        float rotation = 0.f;
        float size[2] = {1.f, 1.f};
        float padding0[2] = {0.f, 0.f};
        float color[4] = {1.f, 1.f, 1.f, 1.f};
        float uvRect[4] = {0.f, 0.f, 1.f, 1.f};
        float data0[4] = {0.f, 0.f, 0.f, 0.f};
        float data1[4] = {0.f, 0.f, 0.f, 0.f};
        float data2[4] = {0.f, 0.f, 0.f, 0.f};
        float data3[4] = {0.f, 0.f, 0.f, 0.f};
        uint32_t id[2] = {0, 0};
        uint32_t flags[2] = {0, 0};
    };

    class BESS_API CustomQuadPipeline {
      public:
        CustomQuadPipeline() = default;
        CustomQuadPipeline(const CustomQuadPipeline &) = delete;
        CustomQuadPipeline &operator=(const CustomQuadPipeline &) = delete;

        void init(const wgpu::Device &device,
                  wgpu::TextureFormat targetFormat,
                  const wgpu::Buffer &frameBuffer,
                  uint64_t frameBufferSize,
                  wgpu::TextureFormat pickingFormat);

        void destroy();

        [[nodiscard]] bool ensureInstanceBufferSize(std::size_t quadCount);

        void uploadInstances(const wgpu::Queue &queue,
                             const CustomQuadInstance *instances,
                             uint64_t byteSize,
                             uint64_t bufferOffset = 0) const;

        [[nodiscard]] CustomQuadShaderHandle
        createShader(const CustomQuadShaderDesc &desc);

        void destroyShader(CustomQuadShaderHandle shader);

        [[nodiscard]] bool hasShader(CustomQuadShaderHandle shader) const;

        [[nodiscard]] const wgpu::RenderPipeline &
        getPipeline(CustomQuadShaderHandle shader, bool transparent) const;

        [[nodiscard]] const wgpu::BindGroup &getBindGroup() const;

        void drawInstances(wgpu::RenderPassEncoder &renderPass,
                           uint32_t firstInstance,
                           uint32_t instanceCount) const;

        void draw(wgpu::RenderPassEncoder &renderPass,
                  CustomQuadShaderHandle shader,
                  uint32_t firstInstance,
                  uint32_t instanceCount,
                  bool transparent) const;

      private:
        bool isWGSLIdentifier(std::string_view value);

        std::string
        buildCustomQuadShaderSource(const CustomQuadShaderDesc &desc);

        struct ShaderResource {
            std::string label;
            std::unique_ptr<Wgpu::WgpuShader> shader;
            wgpu::RenderPipeline opaquePipeline;
            wgpu::RenderPipeline transparentPipeline;
        };

        void createBindGroupLayout();

        void createPipelineLayout();

        void createBindGroup();

        void createPipelineState(ShaderResource &resource);

        wgpu::Device m_device;
        wgpu::TextureFormat m_targetFormat = wgpu::TextureFormat::BGRA8Unorm;
        wgpu::TextureFormat m_pickingFormat = wgpu::TextureFormat::Undefined;
        wgpu::Buffer m_frameBuffer;
        uint64_t m_frameBufferSize = 0;
        wgpu::Buffer m_instanceBuffer;
        uint64_t m_instanceBufferSize = 0;
        wgpu::BindGroupLayout m_bindGroupLayout;
        wgpu::PipelineLayout m_pipelineLayout;
        wgpu::BindGroup m_bindGroup;
        std::unordered_map<CustomQuadShaderHandle, ShaderResource> m_shaders;
        CustomQuadShaderHandle m_nextShaderHandle = 1;
    };

} // namespace Bess
