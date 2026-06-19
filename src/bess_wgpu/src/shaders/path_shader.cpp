#include "bess_wgpu/shaders/path_shader.h"

namespace Bess::Wgpu::Shaders {
    namespace {
        constexpr const char *kPathShader = R"(
struct Frame {
    viewport: vec2f,
    padding: vec2f,
    camera_transform: mat4x4f,
};

struct StencilVertexIn {
    @location(0) position: vec3f,
    @location(1) curve_coord: vec2f,
    @location(2) curve_type: u32,
    @location(3) flags: u32,
    @location(4) instance_position: vec3f,
    @location(5) instance_scale: vec2f,
    @location(6) instance_rotation: f32,
    @location(7) instance_flags: u32,
};

struct StencilVertexOut {
    @builtin(position) position: vec4f,
    @location(0) curve_coord: vec2f,
    @location(1) @interpolate(flat) curve_type: u32,
};

struct CoverVertexIn {
    @location(0) position: vec3f,
    @location(1) color: vec4f,
    @location(2) id: vec2u,
    @location(3) flags: u32,
    @location(4) instance_position: vec3f,
    @location(5) instance_scale: vec2f,
    @location(6) instance_rotation: f32,
    @location(7) instance_flags: u32,
};

struct CoverVertexOut {
    @builtin(position) position: vec4f,
    @location(0) color: vec4f,
    @location(1) @interpolate(flat) id: vec2u,
};

struct FragmentOut {
    @location(0) color: vec4f,
};

struct FragmentOutPicking {
    @location(0) color: vec4f,
    @location(1) id: vec2u,
};

struct StencilFragmentOut {
    @location(0) color: vec4f,
};

struct StencilFragmentOutPicking {
    @location(0) color: vec4f,
    @location(1) id: vec2u,
};

@group(0) @binding(0) var<uniform> frame: Frame;

const CURVE_TYPE_LINE = 0u;
const CURVE_TYPE_QUADRATIC = 1u;
const PATH_FLAG_APPLY_CAMERA_TRANSFORM: u32 = 1u;

fn path_depth(z_index: f32) -> f32 {
    return clamp(0.5 - 0.5 * tanh(z_index * 0.01), 0.0, 1.0);
}

fn path_screen_clip_position(world: vec2f, z_index: f32) -> vec4f {
    let safe_viewport = max(frame.viewport, vec2f(1.0, 1.0));
    let clip_xy = vec2f(
        (world.x / safe_viewport.x) * 2.0,
        -(world.y / safe_viewport.y) * 2.0);
    return vec4f(clip_xy, path_depth(z_index), 1.0);
}

fn path_camera_clip_position(world: vec2f, z_index: f32) -> vec4f {
    var clip = frame.camera_transform * vec4f(world, 0.0, 1.0);
    clip.z = path_depth(z_index);
    return clip;
}

fn path_instance_world_position(local: vec2f, instance_position: vec2f, instance_scale: vec2f, rotation: f32) -> vec2f {
    let scaled = local * instance_scale;
    let s = sin(rotation);
    let c = cos(rotation);
    let rotated = vec2f(
        scaled.x * c - scaled.y * s,
        scaled.x * s + scaled.y * c);
    return instance_position + rotated;
}

@vertex
fn vs_stencil(in: StencilVertexIn) -> StencilVertexOut {
    var out: StencilVertexOut;
    let world = path_instance_world_position(
        in.position.xy,
        in.instance_position.xy,
        in.instance_scale,
        in.instance_rotation);
    let z_index = in.position.z + in.instance_position.z;
    if ((in.instance_flags & PATH_FLAG_APPLY_CAMERA_TRANSFORM) != 0u) {
        out.position = path_camera_clip_position(world, z_index);
    } else {
        out.position = path_screen_clip_position(world, z_index);
    }
    out.curve_coord = in.curve_coord;
    out.curve_type = in.curve_type;
    return out;
}

@fragment
fn fs_stencil(in: StencilVertexOut) -> StencilFragmentOut {
    if (in.curve_type == CURVE_TYPE_QUADRATIC) {
        let implicit = (in.curve_coord.x * in.curve_coord.x) - in.curve_coord.y;
        if (implicit > 0.0) {
            discard;
        }
    }
    var out: StencilFragmentOut;
    out.color = vec4f(0.0);
    return out;
}

@fragment
fn fs_stencil_picking(in: StencilVertexOut) -> StencilFragmentOutPicking {
    if (in.curve_type == CURVE_TYPE_QUADRATIC) {
        let implicit = (in.curve_coord.x * in.curve_coord.x) - in.curve_coord.y;
        if (implicit > 0.0) {
            discard;
        }
    }
    var out: StencilFragmentOutPicking;
    out.color = vec4f(0.0);
    out.id = vec2u(0u);
    return out;
}

@vertex
fn vs_cover(in: CoverVertexIn) -> CoverVertexOut {
    var out: CoverVertexOut;
    let world = path_instance_world_position(
        in.position.xy,
        in.instance_position.xy,
        in.instance_scale,
        in.instance_rotation);
    let z_index = in.position.z + in.instance_position.z;
    if ((in.instance_flags & PATH_FLAG_APPLY_CAMERA_TRANSFORM) != 0u) {
        out.position = path_camera_clip_position(world, z_index);
    } else {
        out.position = path_screen_clip_position(world, z_index);
    }
    out.color = in.color;
    out.id = in.id;
    return out;
}

@fragment
fn fs_cover(in: CoverVertexOut) -> FragmentOut {
    var out: FragmentOut;
    out.color = in.color;
    return out;
}

@fragment
fn fs_cover_picking(in: CoverVertexOut) -> FragmentOutPicking {
    var out: FragmentOutPicking;
    out.color = in.color;
    out.id = in.id;
    return out;
}
)";
    } // namespace

    std::vector<Core::Renderer::ShaderModuleDesc> getPathShaderModules() {
        using Core::Renderer::ShaderLanguage;
        using Core::Renderer::ShaderStage;

        return {
            {.language = ShaderLanguage::WGSL,
             .stage = ShaderStage::Vertex,
             .entryPoint = "vs_stencil",
             .source = kPathShader},
            {.language = ShaderLanguage::WGSL,
             .stage = ShaderStage::Fragment,
             .entryPoint = "fs_stencil",
             .source = kPathShader},
        };
    }

} // namespace Bess::Wgpu::Shaders
