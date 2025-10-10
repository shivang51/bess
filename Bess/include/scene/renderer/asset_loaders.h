#pragma once
#include "asset_manager/asset_loader.h"
#include "font.h"
#include "msdf_font.h"
#include "vulkan_core.h"
#include "vulkan_subtexture.h"
#include "vulkan_texture.h"

#include <memory>
#include <stdexcept>

using namespace Bess::Vulkan;
namespace Bess::Assets {
    template <>
    struct AssetLoader<VulkanTexture> {
        static std::shared_ptr<VulkanTexture> load(const std::string &path) {
            return std::make_shared<VulkanTexture>(Bess::Vulkan::VulkanCore::instance().getDevice(), path);
        }
    };

    template <>
    struct AssetLoader<Bess::Renderer::Font::FontFile> {
        static std::shared_ptr<Bess::Renderer::Font::FontFile> load(const std::string &path) {
            return std::make_shared<Bess::Renderer::Font::FontFile>(path);
        }
    };

    template <>
    struct AssetLoader<Bess::Renderer2D::MsdfFont> {
        static std::shared_ptr<Bess::Renderer2D::MsdfFont> load(const std::string &path, const std::string &json) {
            return std::make_shared<Bess::Renderer2D::MsdfFont>(path, json, Bess::Vulkan::VulkanCore::instance().getDevice());
        }
    };

    template <>
    struct AssetLoader<std::string> {
        static std::shared_ptr<std::string> load(const std::string &path) {
            return std::make_shared<std::string>(path);
        }
    };

} // namespace Bess::Assets
