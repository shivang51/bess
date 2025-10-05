#pragma once
#include "asset_manager/asset_loader.h"
#include "font.h"
#include "msdf_font.h"
#include "scene/renderer/vulkan/vulkan_core.h"
#include "vulkan/vulkan_shader.h"
#include "vulkan/vulkan_subtexture.h"
#include "vulkan/vulkan_texture.h"

#include <memory>
#include <stdexcept>

namespace Bess::Assets {
    template <>
    struct AssetLoader<Bess::Renderer2D::Vulkan::VulkanTexture> {
        static std::shared_ptr<Bess::Renderer2D::Vulkan::VulkanTexture> load(const std::string &path) {
            return std::make_shared<Renderer2D::Vulkan::VulkanTexture>(Renderer2D::VulkanCore::instance().getDevice(), path);
        }
    };

    template <>
    struct AssetLoader<Bess::Renderer2D::Font> {
        static std::shared_ptr<Bess::Renderer2D::Font> load(const std::string &path) {
            return std::make_shared<Bess::Renderer2D::Font>(path);
        }
    };

    template <>
    struct AssetLoader<Bess::Renderer2D::MsdfFont> {
        static std::shared_ptr<Bess::Renderer2D::MsdfFont> load(const std::string &path, const std::string &json) {
            return std::make_shared<Bess::Renderer2D::MsdfFont>(path, json, Renderer2D::VulkanCore::instance().getDevice());
        }
    };

    template <>
    struct AssetLoader<std::string> {
        static std::shared_ptr<std::string> load(const std::string &path) {
            return std::make_shared<std::string>(path);
        }
    };

} // namespace Bess::Assets
