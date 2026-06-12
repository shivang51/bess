#pragma once

#include "ext/scalar_common.hpp"
#include "ext/vector_float2.hpp"
#include "ext/vector_float4.hpp"

#include <algorithm>
#include <array>

namespace Bess::Core::Renderer {
    enum class TextureOrigin {
        TopLeft,
        BottomLeft,
    };

    class SubTexture {
      public:
        SubTexture() = default;

        SubTexture(const glm::vec2 &textureSize, const glm::vec2 &pixelPos,
                   const glm::vec2 &pixelSize,
                   TextureOrigin origin = TextureOrigin::TopLeft) {
            reset(textureSize, pixelPos, pixelSize, origin);
        }

        static SubTexture fromGrid(const glm::vec2 &textureSize,
                                   const glm::vec2 &coord,
                                   const glm::vec2 &spriteSize,
                                   float margin = 0.f,
                                   const glm::vec2 &cellSize = {1.f, 1.f}) {
            const glm::vec2 pixelPos = coord * (spriteSize + glm::vec2(margin));
            const glm::vec2 pixelSize = spriteSize * cellSize;
            return SubTexture(textureSize, pixelPos, pixelSize);
        }

        void reset(const glm::vec2 &textureSize, const glm::vec2 &pixelPos,
                   const glm::vec2 &pixelSize,
                   TextureOrigin origin = TextureOrigin::TopLeft) {
            m_textureSize = glm::max(textureSize, glm::vec2(1.f));
            m_pixelPos = pixelPos;
            m_pixelSize = glm::max(pixelSize, glm::vec2(0.f));

            glm::vec2 normalizedPos = m_pixelPos / m_textureSize;
            const glm::vec2 normalizedSize = m_pixelSize / m_textureSize;
            if (origin == TextureOrigin::BottomLeft) {
                normalizedPos.y =
                    (m_textureSize.y - m_pixelPos.y - m_pixelSize.y) /
                    m_textureSize.y;
            }

            m_startWH = {normalizedPos.x, normalizedPos.y, normalizedSize.x,
                         normalizedSize.y};
            m_texCoords = {
                glm::vec2(m_startWH.x, m_startWH.y + m_startWH.w),
                glm::vec2(m_startWH.x, m_startWH.y),
                glm::vec2(m_startWH.x + m_startWH.z, m_startWH.y),
                glm::vec2(m_startWH.x + m_startWH.z, m_startWH.y + m_startWH.w),
            };
        }

        [[nodiscard]] const glm::vec2 &getTextureSize() const noexcept {
            return m_textureSize;
        }

        [[nodiscard]] const glm::vec2 &getPixelPos() const noexcept {
            return m_pixelPos;
        }

        [[nodiscard]] const glm::vec2 &getPixelSize() const noexcept {
            return m_pixelSize;
        }

        [[nodiscard]] const glm::vec4 &getStartWH() const noexcept {
            return m_startWH;
        }

        [[nodiscard]] const std::array<glm::vec2, 4> &
        getTexCoords() const noexcept {
            return m_texCoords;
        }

      private:
        glm::vec2 m_textureSize{1.f};
        glm::vec2 m_pixelPos{0.f};
        glm::vec2 m_pixelSize{0.f};
        glm::vec4 m_startWH{0.f};
        std::array<glm::vec2, 4> m_texCoords{};
    };
} // namespace Bess::Core::Renderer
