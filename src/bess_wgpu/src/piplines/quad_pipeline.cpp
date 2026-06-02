#include "bess_wgpu/piplines/quad_pipeline.h"
#include "bess_wgpu/shaders/quad_shader.h"
#include "bess_wgpu/wgpu_shader.h"
#include <algorithm>
#include <array>
#include <stdexcept>

namespace Bess::Wgpu::Piplines {

    void QuadPipeline::init(const wgpu::Device &device,
                            wgpu::TextureFormat targetFormat,
                            const wgpu::Buffer &frameBuffer,
                            uint64_t frameBufferSize) {
        m_device = device;
        m_targetFormat = targetFormat;
        m_frameBuffer = frameBuffer;
        m_frameBufferSize = frameBufferSize;

        createShader();
        createBindGroupLayout();
        createTextureSampler();
        createPipelineState();
    }

    void QuadPipeline::destroy() {
        m_instanceBuffer = nullptr;
        m_frameBuffer = nullptr;
        m_frameBufferSize = 0;
        m_pipeline = nullptr;
        m_textureSampler = nullptr;
        m_bindGroupLayout = nullptr;
        m_shader = nullptr;
        m_device = nullptr;
        m_instanceBufferSize = 0;
    }

    bool QuadPipeline::ensureInstanceBufferSize(std::size_t quadCount) {
        const auto requiredSize = std::max<std::size_t>(
            sizeof(QuadInstance), quadCount * sizeof(QuadInstance));
        if (m_instanceBuffer != nullptr && m_instanceBufferSize >= requiredSize) {
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

    void QuadPipeline::uploadInstances(const wgpu::Queue &queue,
                                       const QuadInstance *instances,
                                       uint64_t byteSize) const {
        if (m_instanceBuffer == nullptr || instances == nullptr || byteSize == 0) {
            return;
        }
        queue.WriteBuffer(m_instanceBuffer, 0, instances, byteSize);
    }

    wgpu::BindGroup
    QuadPipeline::createTextureBindGroup(const wgpu::TextureView &textureView,
                                         const std::string &label) const {
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

    const wgpu::RenderPipeline &QuadPipeline::getPipeline() const {
        return m_pipeline;
    }

    void QuadPipeline::createShader() {
        m_shader = std::make_unique<Bess::Wgpu::WgpuShader>(
            "renderer_2d_quad", Shaders::getQuadShaderModules(), m_device);
    }

    void QuadPipeline::createBindGroupLayout() {
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
        m_bindGroupLayout = m_device.CreateBindGroupLayout(&bindGroupLayoutDescriptor);
    }

    void QuadPipeline::createPipelineState() {
        wgpu::PipelineLayoutDescriptor pipelineLayoutDescriptor{};
        pipelineLayoutDescriptor.bindGroupLayoutCount = 1;
        pipelineLayoutDescriptor.bindGroupLayouts = &m_bindGroupLayout;
        wgpu::PipelineLayout pipelineLayout =
            m_device.CreatePipelineLayout(&pipelineLayoutDescriptor);

        wgpu::ColorTargetState colorTarget{};
        colorTarget.format = m_targetFormat;

        wgpu::BlendState blendState{};
        blendState.color.operation = wgpu::BlendOperation::Add;
        blendState.color.srcFactor = wgpu::BlendFactor::SrcAlpha;
        blendState.color.dstFactor = wgpu::BlendFactor::OneMinusSrcAlpha;
        blendState.alpha.operation = wgpu::BlendOperation::Add;
        blendState.alpha.srcFactor = wgpu::BlendFactor::One;
        blendState.alpha.dstFactor = wgpu::BlendFactor::OneMinusSrcAlpha;
        colorTarget.blend = &blendState;

        wgpu::FragmentState fragment{};
        fragment.module = m_shader->getModule(Core::Renderer::ShaderStage::Fragment);
        fragment.entryPoint =
            m_shader->getEntryPoint(Core::Renderer::ShaderStage::Fragment).c_str();
        fragment.targetCount = 1;
        fragment.targets = &colorTarget;

        wgpu::RenderPipelineDescriptor pipelineDescriptor{};
        pipelineDescriptor.layout = pipelineLayout;
        pipelineDescriptor.vertex.module =
            m_shader->getModule(Core::Renderer::ShaderStage::Vertex);
        pipelineDescriptor.vertex.entryPoint =
            m_shader->getEntryPoint(Core::Renderer::ShaderStage::Vertex).c_str();
        pipelineDescriptor.primitive.topology =
            wgpu::PrimitiveTopology::TriangleList;
        pipelineDescriptor.primitive.cullMode = wgpu::CullMode::None;
        pipelineDescriptor.fragment = &fragment;

        m_pipeline = m_device.CreateRenderPipeline(&pipelineDescriptor);
    }

    void QuadPipeline::createTextureSampler() {
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
