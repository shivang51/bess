#pragma once
#include "asset_manager/asset_id.h"
#include "scene/renderer/font.h"
#include "scene/renderer/msdf_font.h"
#include "scene/renderer/gl/shader.h"

namespace Bess::Assets {
    namespace Fonts {
        constexpr auto roboto = AssetID<Renderer2D::Font, 1>("assets/fonts/Roboto/Roboto-Regular.ttf");
        constexpr auto robotoMsdf = AssetID<Renderer2D::MsdfFont, 2>("assets/fonts/Roboto/msdf/", "Roboto-Regular.json");
    } // namespace Fonts

    namespace Shaders {
        constexpr auto quad = AssetID<Gl::Shader, 2>("assets/shaders/quad_vert.glsl", "assets/shaders/quad_frag.glsl");
        constexpr auto path = AssetID<Gl::Shader, 2>("assets/shaders/vert.glsl", "assets/shaders/curve_frag.glsl");
        constexpr auto circle = AssetID<Gl::Shader, 2>("assets/shaders/circle_vert.glsl", "assets/shaders/circle_frag.glsl");
        constexpr auto triangle = AssetID<Gl::Shader, 2>("assets/shaders/vert.glsl", "assets/shaders/triangle_frag.glsl");
        constexpr auto line = AssetID<Gl::Shader, 2>("assets/shaders/vert.glsl", "assets/shaders/line_frag.glsl");
        constexpr auto text = AssetID<Gl::Shader, 2>("assets/shaders/vert.glsl", "assets/shaders/text_frag.glsl");
        constexpr auto grid = AssetID<Gl::Shader, 2>("assets/shaders/grid_vert.glsl", "assets/shaders/grid_frag.glsl");
    } // namespace Shaders
} // namespace Bess::Assets