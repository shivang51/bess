#pragma once
#include "asset_manager/asset_id.h"
#include "scene/renderer/font.h"
#include "scene/renderer/msdf_font.h"
#include "scene/renderer/vulkan/vulkan_shader.h"
#include "scene/renderer/vulkan/vulkan_texture.h"

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
        constexpr const char *quadVert = "assets/shaders/quad_vert.spv";
        constexpr const char *quadFrag = "assets/shaders/quad_frag.spv";
        constexpr const char *circleVert = "assets/shaders/circle_vert.spv";
        constexpr const char *circleFrag = "assets/shaders/circle_frag.spv";
        constexpr const char *textVert = "assets/shaders/text_vert.spv";
        constexpr const char *textFrag = "assets/shaders/text_frag.spv";
        constexpr const char *lineVert = "assets/shaders/line_vert.spv";
        constexpr const char *lineFrag = "assets/shaders/line_frag.spv";
        constexpr const char *gridVert = "assets/shaders/grid_vert.spv";
        constexpr const char *gridFrag = "assets/shaders/grid_frag.spv";
    } // namespace Shaders

    namespace TileMaps {
        constexpr auto sevenSegDisplay = AssetID<Bess::Renderer2D::Vulkan::VulkanTexture, 1>("assets/images/7-seg-display-tilemap.png");
    }

    namespace Textures {
        constexpr auto shadowTexture = AssetID<Bess::Renderer2D::Vulkan::VulkanTexture, 1>("assets/images/shadow_texture.png");
    }
} // namespace Bess::Assets
