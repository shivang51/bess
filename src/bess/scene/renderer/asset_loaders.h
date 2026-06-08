#pragma once
#include "bess_core/asset_manager/asset_loader.h"
#include "bess_core/g_app_context.h"
#include "font.h"
#include "msdf_font.h"
#include "vulkan_core.h"
#include "vulkan_subtexture.h"
#include "vulkan_texture.h"

#include <memory>

using namespace Bess::Vulkan;
namespace Bess::Assets {
    template <> struct AssetLoader<Bess::Renderer::Font::FontFile> {
        static std::shared_ptr<Bess::Renderer::Font::FontFile>
        load(const std::string &path) {
            return std::make_shared<Bess::Renderer::Font::FontFile>(path);
        }
    };

    template <> struct AssetLoader<std::string> {
        static std::shared_ptr<std::string> load(const std::string &path) {
            return std::make_shared<std::string>(path);
        }
    };

} // namespace Bess::Assets
