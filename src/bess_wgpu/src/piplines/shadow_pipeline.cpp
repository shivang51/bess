#include "bess_wgpu/piplines/shadow_pipeline.h"
#include "bess_core/renderer/shader.h"
#include <algorithm>
#include <array>
#include <stdexcept>

namespace Bess::Wgpu::Piplines {
    namespace {
        constexpr wgpu::TextureFormat kDepthStencilFormat =
            wgpu::TextureFormat::Depth24PlusStencil8;

        constexpr const char *kShadowShader = R"(
struct Frame {
    viewport: vec2f,
    padding: vec2f,
    camera_transform: mat4x4f,
};

struct Shadow {
    position: vec3f,
    rotation: f32,
    size: vec2f,
    blur: f32,
    spread: f32,
    color: vec4f,
    radii: vec4f,
    shape_data: vec4f,
    shape_type: u32,
    flags: u32,
    padding0: vec2u,
};

struct VertexOut {
    @builtin(position) position: vec4f,
    @location(0) local_pos: vec2f,
    @location(1) size: vec2f,
    @location(2) blur: f32,
    @location(3) spread: f32,
    @location(4) color: vec4f,
    @location(5) radii: vec4f,
    @location(6) shape_data: vec4f,
    @location(7) @interpolate(flat) shape_type: u32,
};

struct FragmentOut {
    @location(0) color: vec4f,
};

@group(0) @binding(0) var<storage, read> shadows: array<Shadow>;
@group(0) @binding(1) var<uniform> frame: Frame;

const SHADOW_SHAPE_ROUNDED_RECT: u32 = 0u;
const SHADOW_SHAPE_CIRCLE: u32 = 1u;
const SHADOW_SHAPE_LINE: u32 = 2u;
const SHADOW_FLAG_APPLY_CAMERA_TRANSFORM: u32 = 1u;

fn shadow_depth(z_index: f32) -> f32 {
    return clamp(0.5 - 0.5 * tanh(z_index * 0.01), 0.0, 1.0);
}

fn screen_clip_position(world: vec2f, z_index: f32) -> vec4f {
    let safe_viewport = max(frame.viewport, vec2f(1.0, 1.0));
    let clip_xy = vec2f(
        (world.x / safe_viewport.x) * 2.0,
        -(world.y / safe_viewport.y) * 2.0);
    return vec4f(clip_xy, shadow_depth(z_index), 1.0);
}

fn camera_clip_position(world: vec2f, z_index: f32) -> vec4f {
    var clip = frame.camera_transform * vec4f(world, 0.0, 1.0);
    clip.z = shadow_depth(z_index);
    return clip;
}

@vertex
fn vs_main(@builtin(vertex_index) vertex_index: u32,
           @builtin(instance_index) instance_index: u32) -> VertexOut {
    let corners = array<vec2f, 6>(
        vec2f(0.0, 0.0), vec2f(1.0, 0.0), vec2f(0.0, 1.0),
        vec2f(0.0, 1.0), vec2f(1.0, 0.0), vec2f(1.0, 1.0));

    let local = corners[vertex_index];
    let shadow = shadows[instance_index];
    let local_coord = local - vec2f(0.5, 0.5);
    let centered = local_coord * shadow.size;

    let s = sin(shadow.rotation);
    let c = cos(shadow.rotation);
    let rotated = vec2f(
        centered.x * c - centered.y * s,
        centered.x * s + centered.y * c);
    let world = shadow.position.xy + rotated;

    var out: VertexOut;
    if ((shadow.flags & SHADOW_FLAG_APPLY_CAMERA_TRANSFORM) != 0u) {
        out.position = camera_clip_position(world, shadow.position.z);
    } else {
        out.position = screen_clip_position(world, shadow.position.z);
    }
    out.local_pos = centered;
    out.size = shadow.size;
    out.blur = shadow.blur;
    out.spread = shadow.spread;
    out.color = shadow.color;
    out.radii = shadow.radii;
    out.shape_data = shadow.shape_data;
    out.shape_type = shadow.shape_type;
    return out;
}

fn corner_radius_for_point(p: vec2f, radii: vec4f) -> f32 {
    var radius = radii.w;
    if (p.x < 0.0 && p.y < 0.0) {
        radius = radii.x;
    } else if (p.x >= 0.0 && p.y < 0.0) {
        radius = radii.y;
    } else if (p.x >= 0.0 && p.y >= 0.0) {
        radius = radii.z;
    }
    return radius;
}

fn sd_rounded_rect(p: vec2f, half_size: vec2f, radii: vec4f) -> f32 {
    let max_radius = max(min(half_size.x, half_size.y), 0.0);
    let clamped_radii = clamp(radii, vec4f(0.0), vec4f(max_radius));
    let radius = corner_radius_for_point(p, clamped_radii);
    let inner_half_size = max(half_size - vec2f(radius), vec2f(0.0));
    let d = abs(p) - inner_half_size;
    return length(max(d, vec2f(0.0))) + min(max(d.x, d.y), 0.0) - radius;
}

fn sd_capsule(p: vec2f, half_segment_length: f32, radius: f32) -> f32 {
    let a = vec2f(-half_segment_length, 0.0);
    let b = vec2f(half_segment_length, 0.0);
    let pa = p - a;
    let ba = b - a;
    let h = clamp(dot(pa, ba) / max(dot(ba, ba), 0.000001), 0.0, 1.0);
    return length(pa - ba * h) - radius;
}

fn shadow_distance(in: VertexOut) -> f32 {
    let spread = in.spread;
    if (in.shape_type == SHADOW_SHAPE_CIRCLE) {
        return length(in.local_pos) - max(in.shape_data.x + spread, 0.0);
    }

    if (in.shape_type == SHADOW_SHAPE_LINE) {
        let segment_length = max(in.shape_data.x, 0.0);
        let thickness = max(in.shape_data.y, 1.0);
        let radius = max((thickness * 0.5) + spread, 0.0);
        let half_segment_length = max((segment_length - thickness) * 0.5, 0.0);
        return sd_capsule(in.local_pos, half_segment_length, radius);
    }

    let half_size = max((in.shape_data.xy * 0.5) + vec2f(spread), vec2f(0.0001));
    let radii = max(in.radii + vec4f(spread), vec4f(0.0));
    return sd_rounded_rect(in.local_pos, half_size, radii);
}

fn shadow_mask(distance: f32, blur: f32) -> f32 {
    if (blur <= 0.001) {
        return select(0.0, 1.0, distance <= 0.0);
    }
    return 1.0 - smoothstep(-blur, blur, distance);
}

fn compute_color(in: VertexOut) -> vec4f {
    let distance = shadow_distance(in);
    let mask = clamp(shadow_mask(distance, max(in.blur, 0.0)), 0.0, 1.0);
    if (mask < 0.001 || in.color.a <= 0.0) {
        discard;
    }

    return vec4f(in.color.rgb, in.color.a * mask);
}

@fragment
fn fs_main(in: VertexOut) -> FragmentOut {
    var out: FragmentOut;
    out.color = compute_color(in);
    return out;
}
)";
    } // namespace

    void ShadowPipeline::init(const wgpu::Device &device,
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
        createPipelineLayout();
        createPipelineState();
    }

    void ShadowPipeline::destroy() {
        m_pipeline = nullptr;
        m_bindGroup = nullptr;
        m_pipelineLayout = nullptr;
        m_bindGroupLayout = nullptr;
        m_instanceBuffer = nullptr;
        m_instanceBufferSize = 0;
        m_frameBuffer = nullptr;
        m_frameBufferSize = 0;
        m_shader = nullptr;
        m_device = nullptr;
    }

    bool ShadowPipeline::ensureInstanceBufferSize(std::size_t shadowCount) {
        const auto requiredSize = std::max<std::size_t>(
            sizeof(ShadowInstance), shadowCount * sizeof(ShadowInstance));
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

    void ShadowPipeline::uploadInstances(const wgpu::Queue &queue,
                                         const ShadowInstance *instances,
                                         uint64_t byteSize) const {
        if (m_instanceBuffer == nullptr || instances == nullptr ||
            byteSize == 0) {
            return;
        }

        queue.WriteBuffer(m_instanceBuffer, 0, instances, byteSize);
    }

    const wgpu::RenderPipeline &ShadowPipeline::getPipeline() const {
        if (m_pipeline == nullptr || m_bindGroup == nullptr) {
            throw std::runtime_error("Shadow pipeline is not ready");
        }
        return m_pipeline;
    }

    const wgpu::BindGroup &ShadowPipeline::getBindGroup() const {
        if (m_pipeline == nullptr || m_bindGroup == nullptr) {
            throw std::runtime_error("Shadow pipeline is not ready");
        }
        return m_bindGroup;
    }

    void ShadowPipeline::drawInstances(wgpu::RenderPassEncoder &renderPass,
                                       uint32_t firstInstance,
                                       uint32_t instanceCount) const {
        if (instanceCount == 0) {
            return;
        }
        renderPass.Draw(6, instanceCount, 0, firstInstance);
    }

    void ShadowPipeline::draw(wgpu::RenderPassEncoder &renderPass,
                              uint32_t firstInstance,
                              uint32_t instanceCount) const {
        if (instanceCount == 0) {
            return;
        }
        renderPass.SetPipeline(getPipeline());
        renderPass.SetBindGroup(0, getBindGroup());
        drawInstances(renderPass, firstInstance, instanceCount);
    }

    void ShadowPipeline::createShader() {
        using Core::Renderer::ShaderLanguage;
        using Core::Renderer::ShaderModuleDesc;
        using Core::Renderer::ShaderStage;

        m_shader = std::make_unique<Bess::Wgpu::WgpuShader>(
            "renderer_2d_shadow",
            std::vector<ShaderModuleDesc>{
                {.language = ShaderLanguage::WGSL,
                 .stage = ShaderStage::Vertex,
                 .entryPoint = "vs_main",
                 .source = kShadowShader},
                {.language = ShaderLanguage::WGSL,
                 .stage = ShaderStage::Fragment,
                 .entryPoint = "fs_main",
                 .source = kShadowShader},
            },
            m_device);
    }

    void ShadowPipeline::createBindGroupLayout() {
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

    void ShadowPipeline::createPipelineLayout() {
        wgpu::PipelineLayoutDescriptor descriptor{};
        descriptor.bindGroupLayoutCount = 1;
        descriptor.bindGroupLayouts = &m_bindGroupLayout;
        m_pipelineLayout = m_device.CreatePipelineLayout(&descriptor);
    }

    void ShadowPipeline::createBindGroup() {
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

    void ShadowPipeline::createPipelineState() {
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
            colorTargets[1].writeMask = wgpu::ColorWriteMask::None;
            targetCount = 2;
        }

        wgpu::FragmentState fragment{};
        fragment.module =
            m_shader->getModule(Core::Renderer::ShaderStage::Fragment);
        fragment.entryPoint = "fs_main";
        fragment.targetCount = targetCount;
        fragment.targets = colorTargets;

        wgpu::DepthStencilState depthStencil{};
        depthStencil.format = kDepthStencilFormat;
        depthStencil.depthCompare = wgpu::CompareFunction::LessEqual;
        depthStencil.depthWriteEnabled = false;

        wgpu::RenderPipelineDescriptor descriptor{};
        descriptor.layout = m_pipelineLayout;
        descriptor.vertex.module =
            m_shader->getModule(Core::Renderer::ShaderStage::Vertex);
        descriptor.vertex.entryPoint = "vs_main";
        descriptor.primitive.topology = wgpu::PrimitiveTopology::TriangleList;
        descriptor.primitive.cullMode = wgpu::CullMode::None;
        descriptor.fragment = &fragment;
        descriptor.depthStencil = &depthStencil;

        m_pipeline = m_device.CreateRenderPipeline(&descriptor);
        if (m_pipeline == nullptr) {
            throw std::runtime_error("Failed to create shadow pipeline");
        }
    }

} // namespace Bess::Wgpu::Piplines
