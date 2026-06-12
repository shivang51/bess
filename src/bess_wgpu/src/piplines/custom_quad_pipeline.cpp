#include "bess_wgpu/piplines/custom_quad_pipeline.h"
#include "bess_core/renderer/shader.h"

namespace Bess {
    void CustomQuadPipeline::init(const wgpu::Device &device,
                                  wgpu::TextureFormat targetFormat,
                                  const wgpu::Buffer &frameBuffer,
                                  uint64_t frameBufferSize,
                                  wgpu::TextureFormat pickingFormat) {
        m_device = device;
        m_targetFormat = targetFormat;
        m_pickingFormat = pickingFormat;
        m_frameBuffer = frameBuffer;
        m_frameBufferSize = frameBufferSize;
        createBindGroupLayout();
        createPipelineLayout();
    }

    void CustomQuadPipeline::destroy() {
        m_shaders.clear();
        m_bindGroup = nullptr;
        m_instanceBuffer = nullptr;
        m_instanceBufferSize = 0;
        m_pipelineLayout = nullptr;
        m_bindGroupLayout = nullptr;
        m_frameBuffer = nullptr;
        m_frameBufferSize = 0;
        m_device = nullptr;
        m_nextShaderHandle = 1;
    }

    [[nodiscard]] bool
    CustomQuadPipeline::ensureInstanceBufferSize(std::size_t quadCount) {
        const auto requiredSize = std::max<std::size_t>(
            sizeof(CustomQuadInstance), quadCount * sizeof(CustomQuadInstance));
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
        createBindGroup();
        return true;
    }

    void CustomQuadPipeline::uploadInstances(
        const wgpu::Queue &queue, const CustomQuadInstance *instances,
        uint64_t byteSize, uint64_t bufferOffset) const {
        if (m_instanceBuffer == nullptr || instances == nullptr ||
            byteSize == 0) {
            return;
        }
        queue.WriteBuffer(m_instanceBuffer, bufferOffset, instances, byteSize);
    }

    [[nodiscard]] CustomQuadShaderHandle
    CustomQuadPipeline::createShader(const CustomQuadShaderDesc &desc) {
        if (m_device == nullptr) {
            throw std::runtime_error(
                "Custom quad shaders require an initialized WGPU "
                "device");
        }

        const CustomQuadShaderHandle handle = m_nextShaderHandle++;
        if (m_nextShaderHandle == 0) {
            m_nextShaderHandle = 1;
        }

        ShaderResource resource;
        resource.label = desc.label.empty()
                             ? "custom_quad_shader_" + std::to_string(handle)
                             : desc.label;

        const std::string source = buildCustomQuadShaderSource(desc);
        using Core::Renderer::ShaderLanguage;
        using Core::Renderer::ShaderModuleDesc;
        using Core::Renderer::ShaderStage;
        resource.shader = std::make_unique<Wgpu::WgpuShader>(
            resource.label,
            std::vector<ShaderModuleDesc>{
                {.language = ShaderLanguage::WGSL,
                 .stage = ShaderStage::Vertex,
                 .entryPoint = "vs_main",
                 .source = source},
                {.language = ShaderLanguage::WGSL,
                 .stage = ShaderStage::Fragment,
                 .entryPoint = "fs_main",
                 .source = source},
            },
            m_device);
        createPipelineState(resource);
        m_shaders.emplace(handle, std::move(resource));
        return handle;
    }

    void CustomQuadPipeline::destroyShader(CustomQuadShaderHandle shader) {
        m_shaders.erase(shader);
    }

    [[nodiscard]] bool
    CustomQuadPipeline::hasShader(CustomQuadShaderHandle shader) const {
        return shader != 0 && m_shaders.contains(shader);
    }

    const wgpu::RenderPipeline &
    CustomQuadPipeline::getPipeline(CustomQuadShaderHandle shader,
                                    bool transparent) const {
        const auto it = m_shaders.find(shader);
        if (it == m_shaders.end()) {
            throw std::runtime_error(
                "Custom quad shader handle is not registered");
        }
        return transparent ? it->second.transparentPipeline
                           : it->second.opaquePipeline;
    }

    const wgpu::BindGroup &CustomQuadPipeline::getBindGroup() const {
        if (m_bindGroup == nullptr) {
            throw std::runtime_error("Custom quad pipeline has no bind group");
        }
        return m_bindGroup;
    }

    void CustomQuadPipeline::drawInstances(
        wgpu::RenderPassEncoder &renderPass, uint32_t firstInstance,
        uint32_t instanceCount) const {
        if (instanceCount == 0) {
            return;
        }
        renderPass.Draw(6, instanceCount, 0, firstInstance);
    }

    void CustomQuadPipeline::draw(wgpu::RenderPassEncoder &renderPass,
                                  CustomQuadShaderHandle shader,
                                  uint32_t firstInstance,
                                  uint32_t instanceCount,
                                  bool transparent) const {
        if (instanceCount == 0) {
            return;
        }

        renderPass.SetPipeline(getPipeline(shader, transparent));
        renderPass.SetBindGroup(0, getBindGroup());
        drawInstances(renderPass, firstInstance, instanceCount);
    }

    bool CustomQuadPipeline::isWGSLIdentifier(std::string_view value) {
        if (value.empty()) {
            return false;
        }

        const auto isAlphaOrUnderscore = [](char c) {
            return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_';
        };
        const auto isAlphaNumericOrUnderscore = [&](char c) {
            return isAlphaOrUnderscore(c) || (c >= '0' && c <= '9');
        };

        if (!isAlphaOrUnderscore(value.front())) {
            return false;
        }
        return std::all_of(value.begin() + 1, value.end(),
                           isAlphaNumericOrUnderscore);
    }

    std::string CustomQuadPipeline::buildCustomQuadShaderSource(
        const CustomQuadShaderDesc &desc) {
        if (desc.fragmentSource.empty()) {
            throw std::runtime_error(
                "Custom quad shader fragment source is empty");
        }
        if (!isWGSLIdentifier(desc.fragmentEntryPoint)) {
            throw std::runtime_error(
                "Custom quad shader fragment entry point is not a valid "
                "WGSL identifier");
        }

        constexpr const char *kCustomQuadShaderPrelude = R"(
struct Frame {
    viewport: vec2f,
    padding: vec2f,
    camera_transform: mat4x4f,
};

struct CustomQuad {
    position: vec3f,
    rotation: f32,
    size: vec2f,
    padding0: vec2f,
    color: vec4f,
    uv_rect: vec4f,
    data0: vec4f,
    data1: vec4f,
    data2: vec4f,
    data3: vec4f,
    id: vec2u,
    flags: vec2u,
};

struct VertexOut {
    @builtin(position) position: vec4f,
    @location(0) uv: vec2f,
    @location(1) local_uv: vec2f,
    @location(2) local_pos: vec2f,
    @location(3) size: vec2f,
    @location(4) color: vec4f,
    @location(5) data0: vec4f,
    @location(6) data1: vec4f,
    @location(7) data2: vec4f,
    @location(8) data3: vec4f,
    @location(9) @interpolate(flat) id: vec2u,
};

struct CustomQuadFragmentInput {
    frag_coord: vec4f,
    uv: vec2f,
    local_uv: vec2f,
    local_pos: vec2f,
    size: vec2f,
    color: vec4f,
    data0: vec4f,
    data1: vec4f,
    data2: vec4f,
    data3: vec4f,
    viewport: vec2f,
    camera_transform: mat4x4f,
    camera_zoom: f32,
    camera_zoom_xy: vec2f,
};

struct FragmentOut {
    @location(0) color: vec4f,
};

struct FragmentOutPicking {
    @location(0) color: vec4f,
    @location(1) id: vec2u,
};

@group(0) @binding(0) var<storage, read> custom_quads: array<CustomQuad>;
@group(0) @binding(1) var<uniform> frame: Frame;

const CUSTOM_QUAD_FLAG_APPLY_CAMERA_TRANSFORM: u32 = 1u;

fn custom_quad_resolution() -> vec3f {
    return vec3f(frame.viewport, 1.0);
}

fn custom_quad_camera_zoom_xy() -> vec2f {
    return vec2f(
        abs(frame.camera_transform[0][0]) * frame.viewport.x * 0.5,
        abs(frame.camera_transform[1][1]) * frame.viewport.y * 0.5);
}

fn custom_quad_camera_zoom() -> f32 {
    let zoom_xy = custom_quad_camera_zoom_xy();
    return (zoom_xy.x + zoom_xy.y) * 0.5;
}

fn custom_quad_depth(z_index: f32) -> f32 {
    return clamp(0.5 - 0.5 * tanh(z_index * 0.01), 0.0, 1.0);
}

fn custom_quad_screen_clip_position(world: vec2f, z_index: f32) -> vec4f {
    let safe_viewport = max(frame.viewport, vec2f(1.0, 1.0));
    let clip_xy = vec2f(
        (world.x / safe_viewport.x) * 2.0,
        -(world.y / safe_viewport.y) * 2.0);
    return vec4f(clip_xy, custom_quad_depth(z_index), 1.0);
}

fn custom_quad_camera_clip_position(world: vec2f, z_index: f32) -> vec4f {
    var clip = frame.camera_transform * vec4f(world, 0.0, 1.0);
    clip.z = custom_quad_depth(z_index);
    return clip;
}

@vertex
fn vs_main(@builtin(vertex_index) vertex_index: u32,
           @builtin(instance_index) instance_index: u32) -> VertexOut {
    let corners = array<vec2f, 6>(
        vec2f(0.0, 0.0), vec2f(1.0, 0.0), vec2f(0.0, 1.0),
        vec2f(0.0, 1.0), vec2f(1.0, 0.0), vec2f(1.0, 1.0));
    let local = corners[vertex_index];
    let q = custom_quads[instance_index];
    let local_coord = local - vec2f(0.5, 0.5);
    let centered = local_coord * q.size;

    let s = sin(q.rotation);
    let c = cos(q.rotation);
    let rotated = vec2f(
        centered.x * c - centered.y * s,
        centered.x * s + centered.y * c);
    let world = q.position.xy + rotated;

    var out: VertexOut;
    if ((q.flags.x & CUSTOM_QUAD_FLAG_APPLY_CAMERA_TRANSFORM) != 0u) {
        out.position = custom_quad_camera_clip_position(world, q.position.z);
    } else {
        out.position = custom_quad_screen_clip_position(world, q.position.z);
    }
    out.uv = q.uv_rect.xy + local * (q.uv_rect.zw - q.uv_rect.xy);
    out.local_uv = local;
    out.local_pos = centered;
    out.size = q.size;
    out.color = q.color;
    out.data0 = q.data0;
    out.data1 = q.data1;
    out.data2 = q.data2;
    out.data3 = q.data3;
    out.id = q.id;
    return out;
}

fn make_custom_quad_fragment_input(in: VertexOut) -> CustomQuadFragmentInput {
    var out: CustomQuadFragmentInput;
    out.frag_coord = in.position;
    out.uv = in.uv;
    out.local_uv = in.local_uv;
    out.local_pos = in.local_pos;
    out.size = in.size;
    out.color = in.color;
    out.data0 = in.data0;
    out.data1 = in.data1;
    out.data2 = in.data2;
    out.data3 = in.data3;
    out.viewport = frame.viewport;
    out.camera_transform = frame.camera_transform;
    out.camera_zoom = custom_quad_camera_zoom();
    out.camera_zoom_xy = custom_quad_camera_zoom_xy();
    return out;
}
)";

        std::string source = kCustomQuadShaderPrelude;
        source += "\n";
        source += desc.fragmentSource;
        source += R"(

@fragment
fn fs_main(in: VertexOut) -> FragmentOut {
    var out: FragmentOut;
    out.color = )";
        source += desc.fragmentEntryPoint;
        source += R"((make_custom_quad_fragment_input(in));
    return out;
}

@fragment
fn fs_main_picking(in: VertexOut) -> FragmentOutPicking {
    var out: FragmentOutPicking;
    out.color = )";
        source += desc.fragmentEntryPoint;
        source += R"((make_custom_quad_fragment_input(in));
    out.id = in.id;
    return out;
}
)";
        return source;
    }

    void CustomQuadPipeline::createBindGroupLayout() {
        std::array<wgpu::BindGroupLayoutEntry, 2> bindings{};
        bindings[0].binding = 0;
        bindings[0].visibility = wgpu::ShaderStage::Vertex;
        bindings[0].buffer.type = wgpu::BufferBindingType::ReadOnlyStorage;

        bindings[1].binding = 1;
        bindings[1].visibility =
            wgpu::ShaderStage::Vertex | wgpu::ShaderStage::Fragment;
        bindings[1].buffer.type = wgpu::BufferBindingType::Uniform;

        wgpu::BindGroupLayoutDescriptor descriptor{};
        descriptor.entryCount = bindings.size();
        descriptor.entries = bindings.data();
        m_bindGroupLayout = m_device.CreateBindGroupLayout(&descriptor);
    }

    void CustomQuadPipeline::createPipelineLayout() {
        wgpu::PipelineLayoutDescriptor descriptor{};
        descriptor.bindGroupLayoutCount = 1;
        descriptor.bindGroupLayouts = &m_bindGroupLayout;
        m_pipelineLayout = m_device.CreatePipelineLayout(&descriptor);
    }

    void CustomQuadPipeline::createBindGroup() {
        if (m_instanceBuffer == nullptr || m_frameBuffer == nullptr ||
            m_bindGroupLayout == nullptr) {
            m_bindGroup = nullptr;
            return;
        }

        std::array<wgpu::BindGroupEntry, 2> entries{};
        entries[0].binding = 0;
        entries[0].buffer = m_instanceBuffer;
        entries[0].offset = 0;
        entries[0].size = m_instanceBufferSize;

        entries[1].binding = 1;
        entries[1].buffer = m_frameBuffer;
        entries[1].offset = 0;
        entries[1].size = m_frameBufferSize;

        wgpu::BindGroupDescriptor descriptor{};
        descriptor.layout = m_bindGroupLayout;
        descriptor.entryCount = entries.size();
        descriptor.entries = entries.data();
        m_bindGroup = m_device.CreateBindGroup(&descriptor);
    }

    void CustomQuadPipeline::createPipelineState(ShaderResource &resource) {
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
            resource.shader->getModule(Core::Renderer::ShaderStage::Fragment);
        fragment.entryPoint = m_pickingFormat != wgpu::TextureFormat::Undefined
                                  ? "fs_main_picking"
                                  : "fs_main";
        fragment.targetCount = targetCount;
        fragment.targets = colorTargets;

        wgpu::DepthStencilState depthStencil{};
        depthStencil.format = wgpu::TextureFormat::Depth24PlusStencil8;
        depthStencil.depthCompare = wgpu::CompareFunction::LessEqual;
        depthStencil.depthWriteEnabled = true;

        wgpu::RenderPipelineDescriptor opaqueDescriptor{};
        opaqueDescriptor.layout = m_pipelineLayout;
        opaqueDescriptor.vertex.module =
            resource.shader->getModule(Core::Renderer::ShaderStage::Vertex);
        opaqueDescriptor.vertex.entryPoint = "vs_main";
        opaqueDescriptor.primitive.topology =
            wgpu::PrimitiveTopology::TriangleList;
        opaqueDescriptor.primitive.cullMode = wgpu::CullMode::None;
        opaqueDescriptor.fragment = &fragment;
        opaqueDescriptor.depthStencil = &depthStencil;

        resource.opaquePipeline =
            m_device.CreateRenderPipeline(&opaqueDescriptor);
        if (resource.opaquePipeline == nullptr) {
            throw std::runtime_error(
                "Failed to create custom quad opaque pipeline");
        }

        depthStencil.depthWriteEnabled = false;
        wgpu::RenderPipelineDescriptor transparentDescriptor = opaqueDescriptor;
        transparentDescriptor.depthStencil = &depthStencil;
        resource.transparentPipeline =
            m_device.CreateRenderPipeline(&transparentDescriptor);
        if (resource.transparentPipeline == nullptr) {
            throw std::runtime_error(
                "Failed to create custom quad transparent pipeline");
        }
    }
} // namespace Bess
