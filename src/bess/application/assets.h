#pragma once
#include "bess_core/asset_manager/asset_id.h"

namespace Bess::Assets {

    namespace Fonts::Paths {
        constexpr auto roboto =
            AssetID<std::string, 1>("assets/fonts/Roboto/Roboto-Regular.ttf");
        constexpr auto alexBrush = AssetID<std::string, 1>(
            "assets/fonts/AlexBrush/AlexBrush-Regular.ttf");
        constexpr auto componentIcons = AssetID<std::string, 1>(
            "assets/bess_fonts/ComponentIcons_remapped.ttf");
        constexpr auto codeIcons =
            AssetID<std::string, 1>("assets/bess_fonts/CodIcons_remapped.ttf");
        constexpr auto materialIcons = AssetID<std::string, 1>(
            "assets/bess_fonts/MaterialIcons_remapped.ttf");
        constexpr auto fontAwesomeIcons = AssetID<std::string, 1>(
            "assets/bess_fonts/FontAwesomeIcons_remapped.ttf");
    } // namespace Fonts::Paths

    namespace Textures {
        // constexpr auto shadowTexture = AssetID<Bess::Vulkan::VulkanTexture,
        // 1>(
        //     "assets/images/shadow_texture.png");
    }
} // namespace Bess::Assets
