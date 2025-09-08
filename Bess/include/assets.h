#pragma once
#include "asset_manager/asset_id.h"
#include "scene/renderer/font.h"
#include "scene/renderer/gl/shader.h"
#include "scene/renderer/msdf_font.h"

namespace Bess::Assets {
    namespace Fonts {
        constexpr auto roboto = AssetID<Renderer2D::Font, 1>("assets/fonts/Roboto/Roboto-Regular.ttf");
        constexpr auto robotoMsdf = AssetID<Renderer2D::MsdfFont, 2>("assets/fonts/Roboto/msdf/", "Roboto-Regular");
        namespace Paths {
            constexpr auto roboto = AssetID<std::string, 1>("assets/fonts/Roboto/Roboto-Regular.ttf");
            constexpr auto componentIcons = AssetID<std::string, 1>("assets/icons/ComponentIcons.ttf");
            constexpr auto codeIcons = AssetID<std::string, 1>("assets/icons/codicon.ttf");
            constexpr auto materialIcons = AssetID<std::string, 1>("assets/icons/MaterialIcons-Regular.ttf");
            constexpr auto fontAwesomeIcons = AssetID<std::string, 1>("assets/icons/fa-solid-900.ttf");
        } // namespace Paths
    } // namespace Fonts

    namespace Shaders {
        constexpr auto quad = AssetID<Gl::Shader, 2>("assets/shaders/quad_vert.glsl", "assets/shaders/quad_frag.glsl");
        constexpr auto path = AssetID<Gl::Shader, 2>("assets/shaders/vert.glsl", "assets/shaders/curve_frag.glsl");
        constexpr auto circle = AssetID<Gl::Shader, 2>("assets/shaders/circle_vert.glsl", "assets/shaders/circle_frag.glsl");
        constexpr auto triangle = AssetID<Gl::Shader, 2>("assets/shaders/vert.glsl", "assets/shaders/triangle_frag.glsl");
        constexpr auto line = AssetID<Gl::Shader, 2>("assets/shaders/instance_vert.glsl", "assets/shaders/line_frag.glsl");
        constexpr auto text = AssetID<Gl::Shader, 2>("assets/shaders/instance_vert.glsl", "assets/shaders/text_frag.glsl");
        constexpr auto grid = AssetID<Gl::Shader, 2>("assets/shaders/grid_vert.glsl", "assets/shaders/grid_frag.glsl");
    } // namespace Shaders

    namespace TileMaps {
        constexpr auto sevenSegDisplay = AssetID<Gl::Texture, 1>("assets/images/7-seg-display-tilemap.png");
    }
} // namespace Bess::Assets
