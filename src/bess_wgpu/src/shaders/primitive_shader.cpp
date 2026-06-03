#include "bess_wgpu/shaders/primitive_shader.h"

namespace Bess::Wgpu::Shaders {
    namespace {
        constexpr const char *kPrimitiveShader = R"(
struct Frame {
    viewport: vec2f,
    padding: vec2f,
    camera_transform: mat4x4f,
};

struct Primitive {
    position: vec3f,
    padding0: f32,
    color: vec4f,
    border_radius: vec4f,
    border_color: vec4f,
    border_size: vec4f,
    tex_data: vec4f,
    primitive_data: vec4f,
    size: vec2f,
    id: vec2u,
    primitive_type: i32,
    is_mica: i32,
    tex_slot_idx: i32,
    angle: f32,
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
    @location(8) @interpolate(flat) id: vec2u,
    @location(9) local_coord: vec2f,
    @location(10) primitive_data: vec4f,
    @location(11) @interpolate(flat) primitive_type: i32,
};

struct FragmentOut {
    @location(0) color: vec4f,
};

struct FragmentOutPicking {
    @location(0) color: vec4f,
    @location(1) id: vec2u,
};

@group(0) @binding(0) var<storage, read> primitives: array<Primitive>;
@group(0) @binding(1) var<uniform> frame: Frame;
@group(0) @binding(2) var prim_sampler: sampler;
@group(0) @binding(3) var prim_texture: texture_2d<f32>;

const PRIMITIVE_TYPE_QUAD = 0;
const PRIMITIVE_TYPE_CIRCLE = 1;
const PRIMITIVE_TYPE_LINE = 2;

@vertex
fn vs_main(@builtin(vertex_index) vertex_index: u32,
           @builtin(instance_index) instance_index: u32) -> VertexOut {
    let corners = array<vec2f, 6>(
        vec2f(0.0, 0.0), vec2f(1.0, 0.0), vec2f(0.0, 1.0),
        vec2f(0.0, 1.0), vec2f(1.0, 0.0), vec2f(1.0, 1.0));
    let local = corners[vertex_index];
    let q = primitives[instance_index];
    let local_coord = local - vec2f(0.5, 0.5);
    let centered = local_coord * q.size;
    
    let s = sin(q.angle);
    let c = cos(q.angle);
    let rotated = vec2f(
        centered.x * c - centered.y * s,
        centered.x * s + centered.y * c);
    let world = q.position.xy + rotated;
    let ndc = vec2f(
        (world.x / frame.viewport.x) * 2.0 - 1.0,
        1.0 - (world.y / frame.viewport.y) * 2.0);

    var out: VertexOut;
    let depth = 0.5 - 0.5 * tanh(q.position.z * 0.01);
    out.position = vec4f(ndc, depth, 1.0);
    out.local_pos = local * q.size;
    out.local_coord = local_coord;
    out.size = q.size;
    out.color = q.color;
    out.radius = q.border_radius;
    out.border_size = q.border_size;
    out.border_color = q.border_color;
    out.tex_coord = q.tex_data.xy + local * (q.tex_data.zw - q.tex_data.xy);
    out.use_texture = f32(q.tex_slot_idx);
    out.id = q.id;
    out.primitive_data = q.primitive_data;
    out.primitive_type = q.primitive_type;
    return out;
}

fn shadeQuad(in: VertexOut) -> vec4f {
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
        return textureSampleLevel(prim_texture, prim_sampler, in.tex_coord, 0.0) * in.color;
    }
    return in.color;
}

fn shadeCircle(in: VertexOut, fw: vec2f) -> vec4f {
    let outerRadius = max(in.primitive_data.x, 0.0);
    let innerRadius = clamp(in.primitive_data.y, 0.0, outerRadius);
    var innerRatio = 0.0;
    if (outerRadius > 0.0) {
        innerRatio = innerRadius / outerRadius;
    }

    let dist = length(in.local_coord);
    let aa = length(fw) * 0.5;
    let outerMask = 1.0 - smoothstep(0.5 - aa, 0.5 + aa, dist);
    let innerMask = smoothstep((innerRatio * 0.5) - aa, (innerRatio * 0.5) + aa, dist);
    let mask = outerMask * innerMask;

    if (mask < 0.001) {
        discard;
    }

    var color = in.color;
    color.a *= mask;
    return color;
}

fn sdCapsule(p: vec2f, halfSegmentLength: f32, radius: f32) -> f32 {
    let a = vec2f(-halfSegmentLength, 0.0);
    let b = vec2f(halfSegmentLength, 0.0);
    let pa = p - a;
    let ba = b - a;
    let h = clamp(dot(pa, ba) / max(dot(ba, ba), 0.000001), 0.0, 1.0);
    return length(pa - ba * h) - radius;
}

fn shadeLine(in: VertexOut, fw: vec2f) -> vec4f {
    let thickness = max(in.primitive_data.y, 1.0);
    let segmentLength = max(in.primitive_data.x, 0.0);
    let radius = thickness * 0.5;
    let halfSegmentLength = max((segmentLength - thickness) * 0.5, 0.0);
    let p = in.local_coord * in.size;

    let dist = sdCapsule(p, halfSegmentLength, radius);
    let aa = max(length(fw) * 0.5, 0.75);
    let mask = 1.0 - smoothstep(0.0, aa, dist);

    if (mask < 0.001) {
        discard;
    }

    var color = in.color;
    color.a *= mask;
    return color;
}

fn compute_color(in: VertexOut) -> vec4f {

		let fw_line = fwidth(in.local_coord * in.size);
		let fw_circle = fwidth(in.local_coord);
    if (in.primitive_type == PRIMITIVE_TYPE_QUAD) {
        return shadeQuad(in);
    } else if (in.primitive_type == PRIMITIVE_TYPE_CIRCLE) {
        return shadeCircle(in, fw_circle);
    } else if (in.primitive_type == PRIMITIVE_TYPE_LINE) {
        return shadeLine(in, fw_line);
    }
    discard;
    return vec4f(0.0, 0.0, 0.0, 0.0);
}

@fragment
fn fs_main(in: VertexOut) -> FragmentOut {
    var out: FragmentOut;
    out.color = compute_color(in);
    return out;
}

@fragment
fn fs_main_picking(in: VertexOut) -> FragmentOutPicking {
    var out: FragmentOutPicking;
    out.color = compute_color(in);
    out.id = in.id;
    return out;
}
)";
    } // namespace

    std::vector<Core::Renderer::ShaderModuleDesc> getPrimitiveShaderModules() {
        using Core::Renderer::ShaderLanguage;
        using Core::Renderer::ShaderStage;

        return {
            {.language = ShaderLanguage::WGSL,
             .stage = ShaderStage::Vertex,
             .entryPoint = "vs_main",
             .source = kPrimitiveShader},
            {.language = ShaderLanguage::WGSL,
             .stage = ShaderStage::Fragment,
             .entryPoint = "fs_main",
             .source = kPrimitiveShader},
        };
    }

} // namespace Bess::Wgpu::Shaders
