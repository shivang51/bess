#pragma once
#include "asset_manager/asset_loader.h"
#include "font.h"
#include "msdf_font.h"
#include "vulkan/vulkan_shader.h"
#include "vulkan/vulkan_texture.h"
#include "vulkan/vulkan_subtexture.h"

#include <memory>

namespace Bess::Assets {
    // Vulkan shader loader
    template <>
    struct AssetLoader<Bess::Renderer2D::Vulkan::VulkanShader> {
        static std::shared_ptr<Bess::Renderer2D::Vulkan::VulkanShader> load(const std::string& vertPath, const std::string& fragPath) {
            // TODO: Get device from renderer context
            // For now, return nullptr as placeholder
            return nullptr;
        }
    };

    // Vulkan texture loader
    template <>
    struct AssetLoader<Bess::Renderer2D::Vulkan::VulkanTexture> {
        static std::shared_ptr<Bess::Renderer2D::Vulkan::VulkanTexture> load(const std::string& path) {
            // TODO: Get device from renderer context
            // For now, return nullptr as placeholder
            return nullptr;
        }
    };

    // Vulkan subtexture loader - temporarily disabled due to constructor issues
    // template <>
    // struct AssetLoader<Bess::Renderer2D::Vulkan::VulkanSubTexture> {
    //     static std::shared_ptr<Bess::Renderer2D::Vulkan::VulkanSubTexture> load(std::shared_ptr<Bess::Renderer2D::Vulkan::VulkanTexture> texture, const glm::vec2& min, const glm::vec2& max) {
    //         return std::make_shared<Bess::Renderer2D::Vulkan::VulkanSubTexture>(std::move(texture), min, max);
    //     }
    // };

    template <>
    struct AssetLoader<Bess::Renderer2D::Font> {
        static std::shared_ptr<Bess::Renderer2D::Font> load(const std::string &path) {
            return std::make_shared<Bess::Renderer2D::Font>(path);
        }
    };

    template <>
    struct AssetLoader<Bess::Renderer2D::MsdfFont> {
        static std::shared_ptr<Bess::Renderer2D::MsdfFont> load(const std::string &path, const std::string &json) {
            return std::make_shared<Bess::Renderer2D::MsdfFont>(path, json);
        }
    };

    template <>
    struct AssetLoader<std::string> {
        static std::shared_ptr<std::string> load(const std::string &path) {
            return std::make_shared<std::string>(path);
        }
    };

} // namespace Bess::Assets
