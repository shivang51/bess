#pragma once
#include "asset_manager/asset_id.h"
#include "scene/renderer/font.h"
#include "scene/renderer/msdf_font.h"
#include "vulkan_texture.h"

namespace Bess::Assets {
    namespace Fonts {
        constexpr auto roboto = AssetID<Renderer::Font::FontFile, 1>("assets/fonts/Roboto/Roboto-Regular.ttf");
        constexpr auto robotoMsdf = AssetID<Renderer::MsdfFont, 2>("assets/fonts/Roboto/msdf-Roboto-Regular-32/", "Roboto-Regular");
        namespace Paths {
            constexpr auto roboto = AssetID<std::string, 1>("assets/fonts/Roboto/Roboto-Regular.ttf");
            constexpr auto alexBrush = AssetID<std::string, 1>("assets/fonts/AlexBrush/AlexBrush-Regular.ttf");
            constexpr auto componentIcons = AssetID<std::string, 1>("assets/fonts/icons/ComponentIcons.ttf");
            constexpr auto codeIcons = AssetID<std::string, 1>("assets/fonts/icons/codicon.ttf");
            constexpr auto materialIcons = AssetID<std::string, 1>("assets/fonts/icons/MaterialIcons-Regular.ttf");
            constexpr auto fontAwesomeIcons = AssetID<std::string, 1>("assets/fonts/icons/Font-Awesome-7-Free-Regular-400.otf");
        } // namespace Paths
    } // namespace Fonts

    namespace TileMaps {
        constexpr auto sevenSegDisplay = AssetID<Bess::Vulkan::VulkanTexture, 1>("assets/images/7-seg-display-tilemap.png");
    }

    namespace Textures {
        constexpr auto shadowTexture = AssetID<Bess::Vulkan::VulkanTexture, 1>("assets/images/shadow_texture.png");
    }
} // namespace Bess::Assets
