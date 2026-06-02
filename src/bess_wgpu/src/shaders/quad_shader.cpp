#include "bess_wgpu/shaders/quad_shader.h"

namespace Bess::Wgpu::Shaders {
    namespace {
        constexpr const char *kQuadShader = R"(
struct Frame {
    viewport: vec2f,
    padding: vec2f,
    camera_transform: mat4x4f,
};

struct Quad {
    position: vec2f,
    size: vec2f,
    color: vec4f,
    radius: vec4f,
    border_size: vec4f,
    border_color: vec4f,
    uv_rect: vec4f,
    rotation: f32,
    z_index: f32,
    use_texture: f32,
    padding: f32,
};

struct VertexOut {
    @builtin(position) position: vec4f,
    @location(0) local_pos: vec2f,
    @location(1) size: vec2f,
    @location(2) color: vec4f,
    @location(3) radius: vec4f,
    @location(4) border_size: vec4f,
    @location(5) border_color: vec4f,
    @location(6) tex_coord: vec2f,
    @location(7) use_texture: f32,
};

@group(0) @binding(0) var<storage, read> quads: array<Quad>;
@group(0) @binding(1) var<uniform> frame: Frame;
@group(0) @binding(2) var quad_sampler: sampler;
@group(0) @binding(3) var quad_texture: texture_2d<f32>;

@vertex
fn vs_main(@builtin(vertex_index) vertex_index: u32,
           @builtin(instance_index) instance_index: u32) -> VertexOut {
    let corners = array<vec2f, 6>(
        vec2f(0.0, 0.0), vec2f(1.0, 0.0), vec2f(0.0, 1.0),
        vec2f(0.0, 1.0), vec2f(1.0, 0.0), vec2f(1.0, 1.0));
    let local = corners[vertex_index];
    let q = quads[instance_index];
    let centered = (local - vec2f(0.5, 0.5)) * q.size;
    let s = sin(q.rotation);
    let c = cos(q.rotation);
    let rotated = vec2f(
        centered.x * c - centered.y * s,
        centered.x * s + centered.y * c);
    let world = q.position + rotated;
    let ndc = vec2f(
        (world.x / frame.viewport.x) * 2.0 - 1.0,
        1.0 - (world.y / frame.viewport.y) * 2.0);

    var out: VertexOut;
    let depth = 0.5 - 0.5 * tanh(q.z_index * 0.01);
    out.position = vec4f(ndc, depth, 1.0);
    out.local_pos = local * q.size;
    out.size = q.size;
    out.color = q.color;
    out.radius = q.radius;
    out.border_size = q.border_size;
    out.border_color = q.border_color;
    out.tex_coord = q.uv_rect.xy + local * (q.uv_rect.zw - q.uv_rect.xy);
    out.use_texture = q.use_texture;
    return out;
}

@fragment
fn fs_main(in: VertexOut) -> @location(0) vec4f {
    let border = max(max(in.border_size.x, in.border_size.y),
                     max(in.border_size.z, in.border_size.w));
    if (border > 0.0) {
        let near_edge = in.local_pos.x < border ||
                        in.local_pos.y < border ||
                        in.local_pos.x > in.size.x - border ||
                        in.local_pos.y > in.size.y - border;
        if (near_edge) {
            return in.border_color;
        }
    }
    if (in.use_texture > 0.5) {
        return textureSampleLevel(quad_texture, quad_sampler, in.tex_coord, 0.0) * in.color;
    }
    return in.color;
}
)";
    } // namespace

    std::vector<Core::Renderer::ShaderModuleDesc> getQuadShaderModules() {
        using Core::Renderer::ShaderLanguage;
        using Core::Renderer::ShaderStage;

        return {
            {.language = ShaderLanguage::WGSL,
             .stage = ShaderStage::Vertex,
             .entryPoint = "vs_main",
             .source = kQuadShader},
            {.language = ShaderLanguage::WGSL,
             .stage = ShaderStage::Fragment,
             .entryPoint = "fs_main",
             .source = kQuadShader},
        };
    }

} // namespace Bess::Wgpu::Shaders
