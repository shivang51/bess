#pragma once

#include "bess_wgpu/piplines/pipeline.h"
#include "bess_wgpu/wgpu_shader.h"
#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <webgpu/webgpu_cpp.h>

namespace Bess::Wgpu::Piplines {

    struct PrimitiveInstance {
        float position[3] = {0.f, 0.f, 0.f};
        float padding0 = 0.f;
        float color[4] = {1.f, 1.f, 1.f, 1.f};
        float borderRadius[4] = {0.f, 0.f, 0.f, 0.f};
        float borderColor[4] = {0.f, 0.f, 0.f, 1.f};
        float borderSize[4] = {0.f, 0.f, 0.f, 0.f};
        float texData[4] = {0.f, 0.f, 1.f, 1.f};
        float primitiveData[4] = {0.f, 0.f, 0.f, 0.f};
        float size[2] = {1.f, 1.f};
        uint32_t id[2] = {0, 0};
        int32_t primitiveType = 0;
        int32_t isMica = 0;
        int32_t texSlotIdx = 0;
        float angle = 0.f;
    };

    class PrimitivePipeline final : public Pipeline {
      public:
        void init(const wgpu::Device &device,
                  wgpu::TextureFormat targetFormat,
                  const wgpu::Buffer &frameBuffer,
                  uint64_t frameBufferSize,
                  wgpu::TextureFormat pickingFormat = wgpu::TextureFormat::Undefined) override;
        void destroy() override;

        [[nodiscard]] bool ensureInstanceBufferSize(std::size_t quadCount);
        void uploadInstances(const wgpu::Queue &queue,
                             const PrimitiveInstance *instances,
                             uint64_t byteSize,
                             uint64_t bufferOffset = 0) const;

        [[nodiscard]] wgpu::BindGroup
        createTextureBindGroup(const wgpu::TextureView &textureView,
                               const std::string &label = "") const;

        [[nodiscard]] const wgpu::RenderPipeline &getOpaquePipeline() const;
        [[nodiscard]] const wgpu::RenderPipeline &
        getTransparentPipeline() const;

      private:
        void createShader();
        void createBindGroupLayout();
        void createPipelineState();
        void createTextureSampler();

        wgpu::Device m_device;
        wgpu::TextureFormat m_targetFormat = wgpu::TextureFormat::BGRA8Unorm;
        wgpu::TextureFormat m_pickingFormat = wgpu::TextureFormat::Undefined;
        std::unique_ptr<Bess::Wgpu::WgpuShader> m_shader;
        wgpu::RenderPipeline m_opaquePipeline;
        wgpu::RenderPipeline m_transparentPipeline;
        wgpu::BindGroupLayout m_bindGroupLayout;
        wgpu::Sampler m_textureSampler;
        wgpu::Buffer m_instanceBuffer;
        wgpu::Buffer m_frameBuffer;
        uint64_t m_frameBufferSize = 0;
        uint64_t m_instanceBufferSize = 0;
    };

} // namespace Bess::Wgpu::Piplines
