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

fn path_depth(z_index: f32) -> f32 {
    return clamp(0.5 - 0.5 * tanh(z_index * 0.01), 0.0, 1.0);
}

@vertex
fn vs_stencil(in: StencilVertexIn) -> StencilVertexOut {
    var out: StencilVertexOut;
    out.position = frame.camera_transform * vec4f(in.position.xy, 0.0, 1.0);
    out.position.z = path_depth(in.position.z);
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
    out.position = frame.camera_transform * vec4f(in.position.xy, 0.0, 1.0);
    out.position.z = path_depth(in.position.z);
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
