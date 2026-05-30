#pragma once

#include "common/types.h"
#include "ext/vector_float2.hpp"
#include "ext/vector_float4.hpp"
#include "json/value.h"

namespace Bess::Core::Renderer {

    struct Color {
        float r = 0.f, g = 0.f, b = 0.f, a = 1.f;

        constexpr Color() noexcept = default;

        constexpr Color(float red, float green, float blue,
                        float alpha = 1.f) noexcept
            : r(red),
              g(green),
              b(blue),
              a(alpha) {}

        Color &operator=(const glm::vec4 &vec) {
            r = vec.r;
            g = vec.g;
            b = vec.b;
            a = vec.a;
            return *this;
        }

        constexpr Color(const glm::vec4 &vec) noexcept
            : r(vec.r),
              g(vec.g),
              b(vec.b),
              a(vec.a) {}

        constexpr operator glm::vec4() const noexcept { return {r, g, b, a}; }

        static Color fromHex(uint32_t hex) noexcept;

        // RGBA order each val between 0-255, alpha defaults to 255 (opaque)
        static Color fromRGBA8(uint8_t red, uint8_t green, uint8_t blue,
                               uint8_t alpha = 255) noexcept;

        // Converts the color to a 32-bit hex value in RGBA order
        constexpr uint32_t toHex() const noexcept;

        Json::Value toJson() const;
        static Color fromJson(const Json::Value &json);
    };

    typedef uint32_t TextureHandle;

    struct QuadProps {
        glm::vec2 position{0.f, 0.f};
        glm::vec2 size{1.f, 1.f};
        float rotation = 0.f;
        // Lower values are rendered first; higher values appear on top.
        float zIndex = 0.f;

        Color color{1.f, 1.f, 1.f, 1.f};
        TextureHandle texture = 0;            // 0 = No texture (flat color)
        glm::vec4 uvRect{0.f, 0.f, 1.f, 1.f}; // min u/v, max u/v
        PickingId id = PickingId::invalid();
    };

    struct RoundedBorderProps {
        glm::vec4 radius{0.f}; // Top-left, top-right, bottom-right, bottom-left
        glm::vec4 thickness{
            0.f}; // Thickness for each edge in the same order as radius
        Color color{0.f, 0.f, 0.f, 1.f};
    };
} // namespace Bess::Core::Renderer
