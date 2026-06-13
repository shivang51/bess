#include "bess_wgpu/piplines/primitive_pipeline.h"
#include "bess_wgpu/shaders/primitive_shader.h"
#include "bess_wgpu/wgpu_shader.h"
#include <algorithm>
#include <array>

namespace Bess::Wgpu::Piplines {
    namespace {
        constexpr wgpu::TextureFormat kDepthStencilFormat =
            wgpu::TextureFormat::Depth24PlusStencil8;
    }

    void PrimitivePipeline::init(const wgpu::Device &device,
                                 wgpu::TextureFormat targetFormat,
                                 const wgpu::Buffer &frameBuffer,
                                 uint64_t frameBufferSize,
                                 wgpu::TextureFormat pickingFormat) {
        m_device = device;
        m_targetFormat = targetFormat;
        m_pickingFormat = pickingFormat;
        m_frameBuffer = frameBuffer;
        m_frameBufferSize = frameBufferSize;

        createShader();
        createBindGroupLayout();
        createTextureSampler();
        createPipelineState();
    }

    void PrimitivePipeline::destroy() {
        m_instanceBuffer = nullptr;
        m_frameBuffer = nullptr;
        m_frameBufferSize = 0;
        m_opaquePipeline = nullptr;
        m_transparentPipeline = nullptr;
        m_textureSampler = nullptr;
        m_bindGroupLayout = nullptr;
        m_shader = nullptr;
        m_device = nullptr;
        m_instanceBufferSize = 0;
    }

    bool PrimitivePipeline::ensureInstanceBufferSize(std::size_t quadCount) {
        const auto requiredSize = std::max<std::size_t>(
            sizeof(PrimitiveInstance), quadCount * sizeof(PrimitiveInstance));
        if (m_instanceBuffer != nullptr &&
            m_instanceBufferSize >= requiredSize) {
            return false;
        }

        wgpu::BufferDescriptor descriptor{};
        descriptor.size = requiredSize;
        descriptor.usage =
            wgpu::BufferUsage::Storage | wgpu::BufferUsage::CopyDst;
        m_instanceBuffer = m_device.CreateBuffer(&descriptor);
        m_instanceBufferSize = requiredSize;
        return true;
    }

    void PrimitivePipeline::uploadInstances(const wgpu::Queue &queue,
                                            const PrimitiveInstance *instances,
                                            uint64_t byteSize,
                                            uint64_t bufferOffset) const {
        if (m_instanceBuffer == nullptr || instances == nullptr ||
            byteSize == 0) {
            return;
        }
        queue.WriteBuffer(m_instanceBuffer, bufferOffset, instances, byteSize);
    }

    wgpu::BindGroup PrimitivePipeline::createTextureBindGroup(
        const wgpu::TextureView &textureView, const std::string &label) const {
        if (m_instanceBuffer == nullptr || m_frameBuffer == nullptr ||
            m_bindGroupLayout == nullptr || m_textureSampler == nullptr) {
            return nullptr;
        }

        std::array<wgpu::BindGroupEntry, 4> entries{};
        entries[0].binding = 0;
        entries[0].buffer = m_instanceBuffer;
        entries[0].offset = 0;
        entries[0].size = m_instanceBufferSize;

        entries[1].binding = 1;
        entries[1].buffer = m_frameBuffer;
        entries[1].offset = 0;
        entries[1].size = m_frameBufferSize;

        entries[2].binding = 2;
        entries[2].sampler = m_textureSampler;

        entries[3].binding = 3;
        entries[3].textureView = textureView;

        wgpu::BindGroupDescriptor descriptor{};
        descriptor.layout = m_bindGroupLayout;
        descriptor.entryCount = entries.size();
        descriptor.entries = entries.data();
        descriptor.label = label.empty() ? nullptr : label.c_str();
        return m_device.CreateBindGroup(&descriptor);
    }

    const wgpu::RenderPipeline &PrimitivePipeline::getOpaquePipeline() const {
        return m_opaquePipeline;
    }

    const wgpu::RenderPipeline &
    PrimitivePipeline::getTransparentPipeline() const {
        return m_transparentPipeline;
    }

    void PrimitivePipeline::createShader() {
        m_shader = std::make_unique<Bess::Wgpu::WgpuShader>(
            "renderer_2d_primitive",
            Shaders::getPrimitiveShaderModules(),
            m_device);
    }

    void PrimitivePipeline::createBindGroupLayout() {
        std::array<wgpu::BindGroupLayoutEntry, 4> bindings{};
        bindings[0].binding = 0;
        bindings[0].visibility = wgpu::ShaderStage::Vertex;
        bindings[0].buffer.type = wgpu::BufferBindingType::ReadOnlyStorage;

        bindings[1].binding = 1;
        bindings[1].visibility = wgpu::ShaderStage::Vertex;
        bindings[1].buffer.type = wgpu::BufferBindingType::Uniform;

        bindings[2].binding = 2;
        bindings[2].visibility = wgpu::ShaderStage::Fragment;
        bindings[2].sampler.type = wgpu::SamplerBindingType::Filtering;

        bindings[3].binding = 3;
        bindings[3].visibility = wgpu::ShaderStage::Fragment;
        bindings[3].texture.sampleType = wgpu::TextureSampleType::Float;
        bindings[3].texture.viewDimension = wgpu::TextureViewDimension::e2D;

        wgpu::BindGroupLayoutDescriptor bindGroupLayoutDescriptor{};
        bindGroupLayoutDescriptor.entryCount = bindings.size();
        bindGroupLayoutDescriptor.entries = bindings.data();
        m_bindGroupLayout =
            m_device.CreateBindGroupLayout(&bindGroupLayoutDescriptor);
    }

    void PrimitivePipeline::createPipelineState() {
        wgpu::PipelineLayoutDescriptor pipelineLayoutDescriptor{};
        pipelineLayoutDescriptor.bindGroupLayoutCount = 1;
        pipelineLayoutDescriptor.bindGroupLayouts = &m_bindGroupLayout;
        wgpu::PipelineLayout pipelineLayout =
            m_device.CreatePipelineLayout(&pipelineLayoutDescriptor);

        wgpu::ColorTargetState colorTargets[2]{};
        uint32_t targetCount = 1;

        colorTargets[0].format = m_targetFormat;

        wgpu::BlendState blendState{};
        blendState.color.operation = wgpu::BlendOperation::Add;
        blendState.color.srcFactor = wgpu::BlendFactor::SrcAlpha;
        blendState.color.dstFactor = wgpu::BlendFactor::OneMinusSrcAlpha;
        blendState.alpha.operation = wgpu::BlendOperation::Add;
        blendState.alpha.srcFactor = wgpu::BlendFactor::One;
        blendState.alpha.dstFactor = wgpu::BlendFactor::OneMinusSrcAlpha;
        colorTargets[0].blend = &blendState;

        if (m_pickingFormat != wgpu::TextureFormat::Undefined) {
            colorTargets[1].format = m_pickingFormat;
            targetCount = 2;
        }

        wgpu::FragmentState fragment{};
        fragment.module =
            m_shader->getModule(Core::Renderer::ShaderStage::Fragment);
        const char *fragEntryPoint =
            (m_pickingFormat != wgpu::TextureFormat::Undefined)
                ? "fs_main_picking"
                : "fs_main";
        fragment.entryPoint = fragEntryPoint;
        fragment.targetCount = targetCount;
        fragment.targets = colorTargets;

        wgpu::DepthStencilState depthStencil{};
        depthStencil.format = kDepthStencilFormat;
        depthStencil.depthCompare = wgpu::CompareFunction::LessEqual;
        depthStencil.depthWriteEnabled = true;

        wgpu::RenderPipelineDescriptor opaqueDescriptor{};
        opaqueDescriptor.layout = pipelineLayout;
        opaqueDescriptor.vertex.module =
            m_shader->getModule(Core::Renderer::ShaderStage::Vertex);
        opaqueDescriptor.vertex.entryPoint =
            m_shader->getEntryPoint(Core::Renderer::ShaderStage::Vertex)
                .c_str();
        opaqueDescriptor.primitive.topology =
            wgpu::PrimitiveTopology::TriangleList;
        opaqueDescriptor.primitive.cullMode = wgpu::CullMode::None;
        opaqueDescriptor.fragment = &fragment;
        opaqueDescriptor.depthStencil = &depthStencil;

        m_opaquePipeline = m_device.CreateRenderPipeline(&opaqueDescriptor);

        depthStencil.depthWriteEnabled = false;
        wgpu::RenderPipelineDescriptor transparentDescriptor = opaqueDescriptor;
        transparentDescriptor.depthStencil = &depthStencil;
        m_transparentPipeline =
            m_device.CreateRenderPipeline(&transparentDescriptor);
    }

    void PrimitivePipeline::createTextureSampler() {
        wgpu::SamplerDescriptor descriptor{};
        descriptor.addressModeU = wgpu::AddressMode::ClampToEdge;
        descriptor.addressModeV = wgpu::AddressMode::ClampToEdge;
        descriptor.addressModeW = wgpu::AddressMode::ClampToEdge;
        descriptor.magFilter = wgpu::FilterMode::Linear;
        descriptor.minFilter = wgpu::FilterMode::Linear;
        descriptor.mipmapFilter = wgpu::MipmapFilterMode::Nearest;
        m_textureSampler = m_device.CreateSampler(&descriptor);
    }

} // namespace Bess::Wgpu::Piplines
