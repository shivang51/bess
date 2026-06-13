#pragma once

#include "bess_core/renderer/subtexture.h"
#include "bess_core/renderer/texture.h"
#include "common/logger.h"
#include <filesystem>
#include <fstream>
#include <string>

namespace Bess::Core::Renderer {

    struct MsdfGlyph {
        uint32_t codepoint = 0;
        float advance = 0.f;
        glm::vec4 planeBounds{0.f}; // left, bottom, right, top in em units
        SubTexture atlasRegion;
        bool drawable = false;
    };

    template <typename TTexture>
        requires std::derived_from<TTexture, ITexture>
    class MsdfFontAtlas {
      public:
        bool load(const std::filesystem::path &fontDirectory,
                  const std::string &fontName) {
            const std::filesystem::path jsonPath =
                fontDirectory / (fontName + ".json");
            const std::filesystem::path pngPath =
                fontDirectory / (fontName + ".png");

            std::ifstream input(jsonPath);
            if (!input.is_open()) {
                BESS_WARN("[WgpuRenderer2D] Failed to open MSDF font json: "
                          "{}",
                          jsonPath.string());
                return false;
            }

            Json::Value root;
            input >> root;
            if (!root.isMember("atlas") || !root.isMember("metrics") ||
                !root.isMember("glyphs")) {
                BESS_WARN("[WgpuRenderer2D] Invalid MSDF font json: {}",
                          jsonPath.string());
                return false;
            }

            const Json::Value &atlas = root["atlas"];
            const Json::Value &metrics = root["metrics"];
            m_atlasSize = {atlas.get("width", 1).asFloat(),
                           atlas.get("height", 1).asFloat()};
            m_fontSize = atlas.get("size", 32.f).asFloat();
            m_pxRange = atlas.get("distanceRange", 4.f).asFloat();
            m_lineHeight = metrics.get("lineHeight", 1.f).asFloat();
            m_ascender = metrics.get("ascender", 1.f).asFloat();
            m_descender = metrics.get("descender", 0.f).asFloat();

            try {
                m_texture = std::make_shared<TTexture>(pngPath.string());
                m_texture->init();
            } catch (const std::exception &error) {
                BESS_WARN("[WgpuRenderer2D] Failed to load MSDF font "
                          "atlas texture {}: {}",
                          pngPath.string(),
                          error.what());
                m_texture = nullptr;
                return false;
            }

            m_glyphs.clear();
            const Json::Value &glyphs = root["glyphs"];
            for (const auto &glyphJson : glyphs) {
                if (!glyphJson.isObject() || !glyphJson.isMember("unicode")) {
                    continue;
                }

                MsdfGlyph glyph;
                glyph.codepoint = static_cast<uint32_t>(
                    glyphJson.get("unicode", 0).asUInt64());
                glyph.advance = glyphJson.get("advance", 0.f).asFloat();

                if (glyphJson.isMember("planeBounds") &&
                    glyphJson.isMember("atlasBounds")) {
                    const Json::Value &plane = glyphJson["planeBounds"];
                    const Json::Value &bounds = glyphJson["atlasBounds"];
                    glyph.planeBounds = {plane.get("left", 0.f).asFloat(),
                                         plane.get("bottom", 0.f).asFloat(),
                                         plane.get("right", 0.f).asFloat(),
                                         plane.get("top", 0.f).asFloat()};

                    const float left = bounds.get("left", 0.f).asFloat();
                    const float bottom = bounds.get("bottom", 0.f).asFloat();
                    const float right = bounds.get("right", 0.f).asFloat();
                    const float top = bounds.get("top", 0.f).asFloat();
                    glyph.atlasRegion.reset(m_atlasSize,
                                            {left, bottom},
                                            {std::max(0.f, right - left),
                                             std::max(0.f, top - bottom)},
                                            TextureOrigin::BottomLeft);
                    glyph.drawable = true;
                }

                m_glyphs[glyph.codepoint] = glyph;
            }

            return m_texture != nullptr && m_texture->getHandle() != 0 &&
                   !m_glyphs.empty();
        }

        [[nodiscard]] bool valid() const noexcept {
            return m_texture != nullptr && m_texture->getHandle() != 0 &&
                   !m_glyphs.empty();
        }

        [[nodiscard]] const MsdfGlyph *
        findGlyph(uint32_t codepoint) const noexcept {
            auto it = m_glyphs.find(codepoint);
            if (it != m_glyphs.end()) {
                return &it->second;
            }

            it = m_glyphs.find('?');
            if (it != m_glyphs.end()) {
                return &it->second;
            }

            it = m_glyphs.find(' ');
            return it != m_glyphs.end() ? &it->second : nullptr;
        }

        [[nodiscard]] std::shared_ptr<TTexture> getTexture() const {
            return m_texture;
        }

        [[nodiscard]] float pxRange() const noexcept {
            return m_pxRange;
        }
        [[nodiscard]] float lineHeight() const noexcept {
            return m_lineHeight;
        }
        [[nodiscard]] float ascender() const noexcept {
            return m_ascender;
        }
        [[nodiscard]] float fontSize() const noexcept {
            return m_fontSize;
        }

      private:
        std::shared_ptr<TTexture> m_texture;
        std::unordered_map<uint32_t, MsdfGlyph> m_glyphs;
        glm::vec2 m_atlasSize{1.f};
        float m_fontSize = 32.f;
        float m_pxRange = 4.f;
        float m_lineHeight = 1.f;
        float m_ascender = 1.f;
        float m_descender = 0.f;
    };
} // namespace Bess::Core::Renderer
