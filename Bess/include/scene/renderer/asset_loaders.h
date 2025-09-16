#pragma once
#include "asset_manager/asset_loader.h"
#include "font.h"
#include "gl/shader.h"
#include "gl/texture.h"
#include "msdf_font.h"

#include <memory>

namespace Bess::Assets {
    template <>
    struct AssetLoader<Bess::Gl::Shader> {
        static std::shared_ptr<Bess::Gl::Shader> load(const std::string &vertex, const std::string &fragment) {
            return std::make_shared<Bess::Gl::Shader>(vertex, fragment);
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
            return std::make_shared<Bess::Renderer2D::MsdfFont>(path, json);
        }
    };

    template <>
    struct AssetLoader<std::string> {
        static std::shared_ptr<std::string> load(const std::string &path) {
            return std::make_shared<std::string>(path);
        }
    };

    template <>
    struct AssetLoader<Bess::Gl::Texture> {
        static std::shared_ptr<Bess::Gl::Texture> load(const std::string &path) {
            return std::make_shared<Bess::Gl::Texture>(path);
        }
    };
} // namespace Bess::Assets
