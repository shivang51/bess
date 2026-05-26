#pragma once

#include "ext/vector_float4.hpp"
#include "json/value.h"

namespace Bess::Core::Renderer {

    struct Color {
        float r = 0.f, g = 0.f, b = 0.f, a = 1.f;

        Color() = default;

        Color &operator=(const glm::vec4 &vec) {
            r = vec.r;
            g = vec.g;
            b = vec.b;
            a = vec.a;
            return *this;
        }

        operator glm::vec4() const { return {r, g, b, a}; }

        glm::vec4 toVec4() const;

        void fromHex(uint32_t hex);

        // RGBA order each val between 0-255, alpha defaults to 255 (opaque)
        void fromRGBA(uint8_t red, uint8_t green, uint8_t blue,
                      uint8_t alpha = 255);

        // RGBA order each val between 0-1, alpha defaults to 255 (opaque)
        void fromRGBA(float red, float green, float blue, float alpha = 1.f);

        // Converts the color to a 32-bit hex value in RGBA order
        uint32_t toHex() const;

        Json::Value toJson() const;

        static Color fromJson(const Json::Value &json);
    };
} // namespace Bess::Core::Renderer
