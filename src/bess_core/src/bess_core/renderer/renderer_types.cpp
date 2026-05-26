#include "bess_core/renderer/renderer_types.h"

namespace Bess::Core::Renderer {
    glm::vec4 Color::toVec4() const { return {r, g, b, a}; }

    void Color::fromHex(uint32_t hex) {
        r = (float)((hex >> 24) & 0xFF) / 255.f;
        g = (float)((hex >> 16) & 0xFF) / 255.f;
        b = (float)((hex >> 8) & 0xFF) / 255.f;
        a = (float)(hex & 0xFF) / 255.f;
    }

    void Color::fromRGBA(uint8_t red, uint8_t green, uint8_t blue,
                         uint8_t alpha) {
        r = (float)red / 255.f;
        g = (float)green / 255.f;
        b = (float)blue / 255.f;
        a = (float)alpha / 255.f;
    }

    void Color::fromRGBA(float red, float green, float blue, float alpha) {
        r = red;
        g = green;
        b = blue;
        a = alpha;
    }

    uint32_t Color::toHex() const {
        uint8_t red = static_cast<uint8_t>(r * 255.f);
        uint8_t green = static_cast<uint8_t>(g * 255.f);
        uint8_t blue = static_cast<uint8_t>(b * 255.f);
        uint8_t alpha = static_cast<uint8_t>(a * 255.f);
        return (red << 24) | (green << 16) | (blue << 8) | alpha;
    }

    Json::Value Color::toJson() const {
        Json::Value json;
        json["r"] = r;
        json["g"] = g;
        json["b"] = b;
        json["a"] = a;
        return json;
    }

    Color Color::fromJson(const Json::Value &json) {
        Color color;
        if (json.isMember("r") && json["r"].isNumeric())
            color.r = json["r"].asFloat();
        if (json.isMember("g") && json["g"].isNumeric())
            color.g = json["g"].asFloat();
        if (json.isMember("b") && json["b"].isNumeric())
            color.b = json["b"].asFloat();
        if (json.isMember("a") && json["a"].isNumeric())
            color.a = json["a"].asFloat();
        return color;
    }
} // namespace Bess::Core::Renderer
