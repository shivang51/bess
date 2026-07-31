#include "bess_wgpu/piplines/path_pipeline.h"
#include "bess_wgpu/shaders/path_shader.h"
#include "bess_wgpu/wgpu_shader.h"
#include <algorithm>
#include <array>
#include <stdexcept>

namespace Bess::Wgpu::Piplines {
    namespace {
        constexpr wgpu::TextureFormat kDepthStencilFormat =
            wgpu::TextureFormat::Depth24PlusStencil8;
    }

    void PathPipeline::init(const wgpu::Device &device,
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
        createBindGroup();
        createPipelineState();
    }

    void PathPipeline::destroy() {
        m_instanceBuffer = nullptr;
        m_instanceBufferSize = 0;
        m_strokeVertexBuffer = nullptr;
        m_strokeVertexBufferSize = 0;
        m_coverVertexBuffer = nullptr;
        m_coverVertexBufferSize = 0;
        m_stencilVertexBuffer = nullptr;
        m_stencilVertexBufferSize = 0;
        m_frameBuffer = nullptr;
        m_frameBufferSize = 0;
        m_bindGroup = nullptr;
        m_bindGroupLayout = nullptr;
        m_transparentStrokePipeline = nullptr;
        m_opaqueStrokePipeline = nullptr;
        m_transparentCoverPipeline = nullptr;
        m_opaqueCoverPipeline = nullptr;
        m_evenOddStencilPipeline = nullptr;
        m_stencilPipeline = nullptr;
        m_shader = nullptr;
        m_device = nullptr;
    }

    bool PathPipeline::ensureStencilVertexBufferSize(std::size_t vertexCount) {
        const auto requiredSize = std::max<std::size_t>(
            sizeof(PathStencilVertex), vertexCount * sizeof(PathStencilVertex));
        if (m_stencilVertexBuffer != nullptr &&
            m_stencilVertexBufferSize >= requiredSize) {
            return false;
        }

        wgpu::BufferDescriptor descriptor{};
        descriptor.size = requiredSize;
        descriptor.usage =
            wgpu::BufferUsage::Vertex | wgpu::BufferUsage::CopyDst;
        m_stencilVertexBuffer = m_device.CreateBuffer(&descriptor);
        m_stencilVertexBufferSize = requiredSize;
        return true;
    }

    bool PathPipeline::ensureStrokeVertexBufferSize(std::size_t vertexCount) {
        const auto requiredSize = std::max<std::size_t>(
            sizeof(PathCoverVertex), vertexCount * sizeof(PathCoverVertex));
        if (m_strokeVertexBuffer != nullptr &&
            m_strokeVertexBufferSize >= requiredSize) {
            return false;
        }

        wgpu::BufferDescriptor descriptor{};
        descriptor.size = requiredSize;
        descriptor.usage =
            wgpu::BufferUsage::Vertex | wgpu::BufferUsage::CopyDst;
        m_strokeVertexBuffer = m_device.CreateBuffer(&descriptor);
        m_strokeVertexBufferSize = requiredSize;
        return true;
    }

    bool PathPipeline::ensureCoverVertexBufferSize(std::size_t vertexCount) {
        const auto requiredSize = std::max<std::size_t>(
            sizeof(PathCoverVertex), vertexCount * sizeof(PathCoverVertex));
        if (m_coverVertexBuffer != nullptr &&
            m_coverVertexBufferSize >= requiredSize) {
            return false;
        }

        wgpu::BufferDescriptor descriptor{};
        descriptor.size = requiredSize;
        descriptor.usage =
            wgpu::BufferUsage::Vertex | wgpu::BufferUsage::CopyDst;
        m_coverVertexBuffer = m_device.CreateBuffer(&descriptor);
        m_coverVertexBufferSize = requiredSize;
        return true;
    }

    bool PathPipeline::ensureInstanceBufferSize(std::size_t instanceCount) {
        const auto requiredSize = std::max<std::size_t>(
            sizeof(PathInstance), instanceCount * sizeof(PathInstance));
        if (m_instanceBuffer != nullptr &&
            m_instanceBufferSize >= requiredSize) {
            return false;
        }

        wgpu::BufferDescriptor descriptor{};
        descriptor.size = requiredSize;
        descriptor.usage =
            wgpu::BufferUsage::Vertex | wgpu::BufferUsage::CopyDst;
        m_instanceBuffer = m_device.CreateBuffer(&descriptor);
        m_instanceBufferSize = requiredSize;
        return true;
    }

    void PathPipeline::uploadStencilVertices(const wgpu::Queue &queue,
                                             const PathStencilVertex *vertices,
                                             uint64_t byteSize,
                                             uint64_t bufferOffset) const {
        if (m_stencilVertexBuffer == nullptr || vertices == nullptr ||
            byteSize == 0) {
            return;
        }
        queue.WriteBuffer(
            m_stencilVertexBuffer, bufferOffset, vertices, byteSize);
    }

    void PathPipeline::uploadCoverVertices(const wgpu::Queue &queue,
                                           const PathCoverVertex *vertices,
                                           uint64_t byteSize,
                                           uint64_t bufferOffset) const {
        if (m_coverVertexBuffer == nullptr || vertices == nullptr ||
            byteSize == 0) {
            return;
        }
        queue.WriteBuffer(
            m_coverVertexBuffer, bufferOffset, vertices, byteSize);
    }

    void PathPipeline::uploadStrokeVertices(const wgpu::Queue &queue,
                                            const PathCoverVertex *vertices,
                                            uint64_t byteSize,
                                            uint64_t bufferOffset) const {
        if (m_strokeVertexBuffer == nullptr || vertices == nullptr ||
            byteSize == 0) {
            return;
        }
        queue.WriteBuffer(
            m_strokeVertexBuffer, bufferOffset, vertices, byteSize);
    }

    void PathPipeline::uploadInstances(const wgpu::Queue &queue,
                                       const PathInstance *instances,
                                       uint64_t byteSize,
                                       uint64_t bufferOffset) const {
        if (m_instanceBuffer == nullptr || instances == nullptr ||
            byteSize == 0) {
            return;
        }
        queue.WriteBuffer(m_instanceBuffer, bufferOffset, instances, byteSize);
    }

    const wgpu::BindGroup &PathPipeline::getBindGroup() const {
        if (m_bindGroup == nullptr) {
            throw std::runtime_error("Path pipeline has no bind group");
        }
        return m_bindGroup;
    }

    const wgpu::RenderPipeline &
    PathPipeline::getStencilPipeline(bool evenOddFill) const {
        const auto &pipeline =
            evenOddFill ? m_evenOddStencilPipeline : m_stencilPipeline;
        if (pipeline == nullptr) {
            throw std::runtime_error("Path stencil pipeline is not ready");
        }
        return pipeline;
    }

    const wgpu::RenderPipeline &
    PathPipeline::getCoverPipeline(bool transparent) const {
        const auto &pipeline =
            transparent ? m_transparentCoverPipeline : m_opaqueCoverPipeline;
        if (pipeline == nullptr) {
            throw std::runtime_error("Path cover pipeline is not ready");
        }
        return pipeline;
    }

    const wgpu::RenderPipeline &
    PathPipeline::getStrokePipeline(bool transparent) const {
        const auto &pipeline =
            transparent ? m_transparentStrokePipeline : m_opaqueStrokePipeline;
        if (pipeline == nullptr) {
            throw std::runtime_error("Path stroke pipeline is not ready");
        }
        return pipeline;
    }

    const wgpu::Buffer &PathPipeline::getStencilVertexBuffer() const {
        if (m_stencilVertexBuffer == nullptr) {
            throw std::runtime_error("Path stencil vertex buffer is not ready");
        }
        return m_stencilVertexBuffer;
    }

    const wgpu::Buffer &PathPipeline::getCoverVertexBuffer() const {
        if (m_coverVertexBuffer == nullptr) {
            throw std::runtime_error("Path cover vertex buffer is not ready");
        }
        return m_coverVertexBuffer;
    }

    const wgpu::Buffer &PathPipeline::getStrokeVertexBuffer() const {
        if (m_strokeVertexBuffer == nullptr) {
            throw std::runtime_error("Path stroke vertex buffer is not ready");
        }
        return m_strokeVertexBuffer;
    }

    const wgpu::Buffer &PathPipeline::getInstanceBuffer() const {
        if (m_instanceBuffer == nullptr) {
            throw std::runtime_error("Path instance buffer is not ready");
        }
        return m_instanceBuffer;
    }

    void PathPipeline::drawPath(wgpu::RenderPassEncoder &renderPass,
                                uint32_t firstStencilVertex,
                                uint32_t stencilVertexCount,
                                uint32_t firstCoverVertex,
                                uint32_t coverVertexCount,
                                uint32_t firstInstance,
                                bool transparent,
                                bool evenOddFill) const {
        if (stencilVertexCount == 0 || coverVertexCount == 0) {
            return;
        }

        renderPass.SetBindGroup(0, m_bindGroup);

        renderPass.SetPipeline(evenOddFill ? m_evenOddStencilPipeline
                                           : m_stencilPipeline);
        renderPass.SetVertexBuffer(0,
                                   m_stencilVertexBuffer,
                                   static_cast<uint64_t>(firstStencilVertex) *
                                       sizeof(PathStencilVertex),
                                   static_cast<uint64_t>(stencilVertexCount) *
                                       sizeof(PathStencilVertex));
        renderPass.SetVertexBuffer(1,
                                   m_instanceBuffer,
                                   static_cast<uint64_t>(firstInstance) *
                                       sizeof(PathInstance),
                                   sizeof(PathInstance));
        renderPass.Draw(stencilVertexCount, 1, 0, 0);

        renderPass.SetPipeline(transparent ? m_transparentCoverPipeline
                                           : m_opaqueCoverPipeline);
        renderPass.SetVertexBuffer(
            0,
            m_coverVertexBuffer,
            static_cast<uint64_t>(firstCoverVertex) * sizeof(PathCoverVertex),
            static_cast<uint64_t>(coverVertexCount) * sizeof(PathCoverVertex));
        renderPass.SetVertexBuffer(1,
                                   m_instanceBuffer,
                                   static_cast<uint64_t>(firstInstance) *
                                       sizeof(PathInstance),
                                   sizeof(PathInstance));
        renderPass.Draw(coverVertexCount, 1, 0, 0);
    }

    void PathPipeline::drawStroke(wgpu::RenderPassEncoder &renderPass,
                                  uint32_t firstVertex,
                                  uint32_t vertexCount,
                                  uint32_t firstInstance,
                                  bool transparent) const {
        if (vertexCount == 0) {
            return;
        }

        renderPass.SetBindGroup(0, m_bindGroup);
        renderPass.SetPipeline(transparent ? m_transparentStrokePipeline
                                           : m_opaqueStrokePipeline);
        renderPass.SetVertexBuffer(
            0,
            m_strokeVertexBuffer,
            static_cast<uint64_t>(firstVertex) * sizeof(PathCoverVertex),
            static_cast<uint64_t>(vertexCount) * sizeof(PathCoverVertex));
        renderPass.SetVertexBuffer(1,
                                   m_instanceBuffer,
                                   static_cast<uint64_t>(firstInstance) *
                                       sizeof(PathInstance),
                                   sizeof(PathInstance));
        renderPass.Draw(vertexCount, 1, 0, 0);
    }

    void PathPipeline::createShader() {
        m_shader = std::make_unique<Bess::Wgpu::WgpuShader>(
            "renderer_2d_path", Shaders::getPathShaderModules(), m_device);
    }

    void PathPipeline::createBindGroupLayout() {
        wgpu::BindGroupLayoutEntry binding{};
        binding.binding = 0;
        binding.visibility = wgpu::ShaderStage::Vertex;
        binding.buffer.type = wgpu::BufferBindingType::Uniform;

        wgpu::BindGroupLayoutDescriptor descriptor{};
        descriptor.entryCount = 1;
        descriptor.entries = &binding;
        m_bindGroupLayout = m_device.CreateBindGroupLayout(&descriptor);
    }

    void PathPipeline::createBindGroup() {
        wgpu::BindGroupEntry entry{};
        entry.binding = 0;
        entry.buffer = m_frameBuffer;
        entry.offset = 0;
        entry.size = m_frameBufferSize;

        wgpu::BindGroupDescriptor descriptor{};
        descriptor.layout = m_bindGroupLayout;
        descriptor.entryCount = 1;
        descriptor.entries = &entry;
        m_bindGroup = m_device.CreateBindGroup(&descriptor);
    }

    void PathPipeline::createPipelineState() {
        wgpu::PipelineLayoutDescriptor pipelineLayoutDescriptor{};
        pipelineLayoutDescriptor.bindGroupLayoutCount = 1;
        pipelineLayoutDescriptor.bindGroupLayouts = &m_bindGroupLayout;
        wgpu::PipelineLayout pipelineLayout =
            m_device.CreatePipelineLayout(&pipelineLayoutDescriptor);

        std::array<wgpu::VertexAttribute, 4> stencilAttributes{};
        stencilAttributes[0].shaderLocation = 0;
        stencilAttributes[0].format = wgpu::VertexFormat::Float32x3;
        stencilAttributes[0].offset = offsetof(PathStencilVertex, position);
        stencilAttributes[1].shaderLocation = 1;
        stencilAttributes[1].format = wgpu::VertexFormat::Float32x2;
        stencilAttributes[1].offset = offsetof(PathStencilVertex, curveCoord);
        stencilAttributes[2].shaderLocation = 2;
        stencilAttributes[2].format = wgpu::VertexFormat::Uint32;
        stencilAttributes[2].offset = offsetof(PathStencilVertex, curveType);
        stencilAttributes[3].shaderLocation = 3;
        stencilAttributes[3].format = wgpu::VertexFormat::Uint32;
        stencilAttributes[3].offset = offsetof(PathStencilVertex, flags);

        wgpu::VertexBufferLayout stencilVertexBufferLayout{};
        stencilVertexBufferLayout.arrayStride = sizeof(PathStencilVertex);
        stencilVertexBufferLayout.stepMode = wgpu::VertexStepMode::Vertex;
        stencilVertexBufferLayout.attributeCount = stencilAttributes.size();
        stencilVertexBufferLayout.attributes = stencilAttributes.data();

        std::array<wgpu::VertexAttribute, 4> instanceAttributes{};
        instanceAttributes[0].shaderLocation = 4;
        instanceAttributes[0].format = wgpu::VertexFormat::Float32x3;
        instanceAttributes[0].offset = offsetof(PathInstance, position);
        instanceAttributes[1].shaderLocation = 5;
        instanceAttributes[1].format = wgpu::VertexFormat::Float32x2;
        instanceAttributes[1].offset = offsetof(PathInstance, scale);
        instanceAttributes[2].shaderLocation = 6;
        instanceAttributes[2].format = wgpu::VertexFormat::Float32;
        instanceAttributes[2].offset = offsetof(PathInstance, rotation);
        instanceAttributes[3].shaderLocation = 7;
        instanceAttributes[3].format = wgpu::VertexFormat::Uint32;
        instanceAttributes[3].offset = offsetof(PathInstance, flags);

        wgpu::VertexBufferLayout instanceVertexBufferLayout{};
        instanceVertexBufferLayout.arrayStride = sizeof(PathInstance);
        instanceVertexBufferLayout.stepMode = wgpu::VertexStepMode::Instance;
        instanceVertexBufferLayout.attributeCount = instanceAttributes.size();
        instanceVertexBufferLayout.attributes = instanceAttributes.data();

        std::array<wgpu::VertexBufferLayout, 2> stencilVertexBuffers{
            stencilVertexBufferLayout,
            instanceVertexBufferLayout,
        };

        wgpu::FragmentState stencilFragment{};
        stencilFragment.module =
            m_shader->getModule(Core::Renderer::ShaderStage::Fragment);
        stencilFragment.entryPoint =
            (m_pickingFormat != wgpu::TextureFormat::Undefined)
                ? "fs_stencil_picking"
                : "fs_stencil";
        wgpu::ColorTargetState stencilColorTargets[2]{};
        uint32_t stencilTargetCount = 1;
        stencilColorTargets[0].format = m_targetFormat;
        stencilColorTargets[0].writeMask = wgpu::ColorWriteMask::None;
        if (m_pickingFormat != wgpu::TextureFormat::Undefined) {
            stencilColorTargets[1].format = m_pickingFormat;
            stencilColorTargets[1].writeMask = wgpu::ColorWriteMask::None;
            stencilTargetCount = 2;
        }
        stencilFragment.targetCount = stencilTargetCount;
        stencilFragment.targets = stencilColorTargets;

        wgpu::DepthStencilState stencilDepthStencil{};
        stencilDepthStencil.format = kDepthStencilFormat;
        stencilDepthStencil.depthCompare = wgpu::CompareFunction::LessEqual;
        stencilDepthStencil.depthWriteEnabled = false;
        stencilDepthStencil.stencilReadMask = 0xFF;
        stencilDepthStencil.stencilWriteMask = 0xFF;
        stencilDepthStencil.stencilFront.compare =
            wgpu::CompareFunction::Always;
        stencilDepthStencil.stencilFront.failOp = wgpu::StencilOperation::Keep;
        stencilDepthStencil.stencilFront.depthFailOp =
            wgpu::StencilOperation::Keep;
        stencilDepthStencil.stencilFront.passOp =
            wgpu::StencilOperation::IncrementWrap;
        stencilDepthStencil.stencilBack.compare = wgpu::CompareFunction::Always;
        stencilDepthStencil.stencilBack.failOp = wgpu::StencilOperation::Keep;
        stencilDepthStencil.stencilBack.depthFailOp =
            wgpu::StencilOperation::Keep;
        stencilDepthStencil.stencilBack.passOp =
            wgpu::StencilOperation::DecrementWrap;

        wgpu::RenderPipelineDescriptor stencilDescriptor{};
        stencilDescriptor.layout = pipelineLayout;
        stencilDescriptor.vertex.module =
            m_shader->getModule(Core::Renderer::ShaderStage::Vertex);
        stencilDescriptor.vertex.entryPoint = "vs_stencil";
        stencilDescriptor.vertex.bufferCount = stencilVertexBuffers.size();
        stencilDescriptor.vertex.buffers = stencilVertexBuffers.data();
        stencilDescriptor.primitive.topology =
            wgpu::PrimitiveTopology::TriangleList;
        stencilDescriptor.primitive.cullMode = wgpu::CullMode::None;
        stencilDescriptor.fragment = &stencilFragment;
        stencilDescriptor.depthStencil = &stencilDepthStencil;
        m_stencilPipeline = m_device.CreateRenderPipeline(&stencilDescriptor);

        stencilDepthStencil.stencilFront.passOp =
            wgpu::StencilOperation::Invert;
        stencilDepthStencil.stencilBack.passOp = wgpu::StencilOperation::Invert;
        m_evenOddStencilPipeline =
            m_device.CreateRenderPipeline(&stencilDescriptor);

        std::array<wgpu::VertexAttribute, 4> coverAttributes{};
        coverAttributes[0].shaderLocation = 0;
        coverAttributes[0].format = wgpu::VertexFormat::Float32x3;
        coverAttributes[0].offset = offsetof(PathCoverVertex, position);
        coverAttributes[1].shaderLocation = 1;
        coverAttributes[1].format = wgpu::VertexFormat::Float32x4;
        coverAttributes[1].offset = offsetof(PathCoverVertex, color);
        coverAttributes[2].shaderLocation = 2;
        coverAttributes[2].format = wgpu::VertexFormat::Uint32x2;
        coverAttributes[2].offset = offsetof(PathCoverVertex, id);
        coverAttributes[3].shaderLocation = 3;
        coverAttributes[3].format = wgpu::VertexFormat::Uint32;
        coverAttributes[3].offset = offsetof(PathCoverVertex, flags);

        wgpu::VertexBufferLayout coverVertexBufferLayout{};
        coverVertexBufferLayout.arrayStride = sizeof(PathCoverVertex);
        coverVertexBufferLayout.stepMode = wgpu::VertexStepMode::Vertex;
        coverVertexBufferLayout.attributeCount = coverAttributes.size();
        coverVertexBufferLayout.attributes = coverAttributes.data();

        std::array<wgpu::VertexBufferLayout, 2> coverVertexBuffers{
            coverVertexBufferLayout,
            instanceVertexBufferLayout,
        };

        wgpu::BlendState blendState{};
        blendState.color.operation = wgpu::BlendOperation::Add;
        blendState.color.srcFactor = wgpu::BlendFactor::SrcAlpha;
        blendState.color.dstFactor = wgpu::BlendFactor::OneMinusSrcAlpha;
        blendState.alpha.operation = wgpu::BlendOperation::Add;
        blendState.alpha.srcFactor = wgpu::BlendFactor::One;
        blendState.alpha.dstFactor = wgpu::BlendFactor::OneMinusSrcAlpha;

        wgpu::ColorTargetState colorTargets[2]{};
        uint32_t targetCount = 1;
        colorTargets[0].format = m_targetFormat;
        colorTargets[0].blend = &blendState;
        if (m_pickingFormat != wgpu::TextureFormat::Undefined) {
            colorTargets[1].format = m_pickingFormat;
            targetCount = 2;
        }

        wgpu::FragmentState coverFragment{};
        coverFragment.module =
            m_shader->getModule(Core::Renderer::ShaderStage::Fragment);
        coverFragment.entryPoint =
            (m_pickingFormat != wgpu::TextureFormat::Undefined)
                ? "fs_cover_picking"
                : "fs_cover";
        coverFragment.targetCount = targetCount;
        coverFragment.targets = colorTargets;

        wgpu::DepthStencilState coverDepthStencil{};
        coverDepthStencil.format = kDepthStencilFormat;
        coverDepthStencil.depthCompare = wgpu::CompareFunction::LessEqual;
        coverDepthStencil.depthWriteEnabled = true;
        coverDepthStencil.stencilReadMask = 0xFF;
        coverDepthStencil.stencilWriteMask = 0xFF;
        coverDepthStencil.stencilFront.compare =
            wgpu::CompareFunction::NotEqual;
        coverDepthStencil.stencilFront.failOp = wgpu::StencilOperation::Keep;
        coverDepthStencil.stencilFront.depthFailOp =
            wgpu::StencilOperation::Zero;
        coverDepthStencil.stencilFront.passOp = wgpu::StencilOperation::Zero;
        coverDepthStencil.stencilBack = coverDepthStencil.stencilFront;

        wgpu::RenderPipelineDescriptor coverDescriptor{};
        coverDescriptor.layout = pipelineLayout;
        coverDescriptor.vertex.module =
            m_shader->getModule(Core::Renderer::ShaderStage::Vertex);
        coverDescriptor.vertex.entryPoint = "vs_cover";
        coverDescriptor.vertex.bufferCount = coverVertexBuffers.size();
        coverDescriptor.vertex.buffers = coverVertexBuffers.data();
        coverDescriptor.primitive.topology =
            wgpu::PrimitiveTopology::TriangleList;
        coverDescriptor.primitive.cullMode = wgpu::CullMode::None;
        coverDescriptor.fragment = &coverFragment;
        coverDescriptor.depthStencil = &coverDepthStencil;
        m_opaqueCoverPipeline = m_device.CreateRenderPipeline(&coverDescriptor);

        coverDepthStencil.depthWriteEnabled = false;
        wgpu::RenderPipelineDescriptor transparentCoverDescriptor =
            coverDescriptor;
        transparentCoverDescriptor.depthStencil = &coverDepthStencil;
        m_transparentCoverPipeline =
            m_device.CreateRenderPipeline(&transparentCoverDescriptor);

        wgpu::DepthStencilState strokeDepthStencil{};
        strokeDepthStencil.format = kDepthStencilFormat;
        strokeDepthStencil.depthCompare = wgpu::CompareFunction::LessEqual;
        strokeDepthStencil.depthWriteEnabled = true;
        strokeDepthStencil.stencilReadMask = 0x00;
        strokeDepthStencil.stencilWriteMask = 0x00;
        strokeDepthStencil.stencilFront.compare = wgpu::CompareFunction::Always;
        strokeDepthStencil.stencilFront.failOp = wgpu::StencilOperation::Keep;
        strokeDepthStencil.stencilFront.depthFailOp =
            wgpu::StencilOperation::Keep;
        strokeDepthStencil.stencilFront.passOp = wgpu::StencilOperation::Keep;
        strokeDepthStencil.stencilBack = strokeDepthStencil.stencilFront;

        wgpu::RenderPipelineDescriptor strokeDescriptor = coverDescriptor;
        strokeDescriptor.depthStencil = &strokeDepthStencil;
        m_opaqueStrokePipeline =
            m_device.CreateRenderPipeline(&strokeDescriptor);

        strokeDepthStencil.depthWriteEnabled = false;
        wgpu::RenderPipelineDescriptor transparentStrokeDescriptor =
            strokeDescriptor;
        transparentStrokeDescriptor.depthStencil = &strokeDepthStencil;
        m_transparentStrokePipeline =
            m_device.CreateRenderPipeline(&transparentStrokeDescriptor);
    }

} // namespace Bess::Wgpu::Piplines
