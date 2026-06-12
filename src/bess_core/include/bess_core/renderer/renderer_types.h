#pragma once

#include "common/types.h"
#include "ext/vector_float2.hpp"
#include "ext/vector_float4.hpp"
#include "spdlog/fmt/bundled/format.h"
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

        static constexpr Color fromHex(uint32_t hex) noexcept {
            Color col;
            col.r = (float)((hex >> 24) & 0xFF) / 255.f;
            col.g = (float)((hex >> 16) & 0xFF) / 255.f;
            col.b = (float)((hex >> 8) & 0xFF) / 255.f;
            col.a = (float)(hex & 0xFF) / 255.f;
            return col;
        }

        // RGBA order each val between 0-255, alpha defaults to 255 (opaque)
        static constexpr Color fromRGBA8(uint8_t red, uint8_t green,
                                         uint8_t blue,
                                         uint8_t alpha = 255) noexcept {
            Color col;
            col.r = (float)red / 255.f;
            col.g = (float)green / 255.f;
            col.b = (float)blue / 255.f;
            col.a = (float)alpha / 255.f;
            return col;
        }

        // Converts the color to a 32-bit hex value in RGBA order
        constexpr uint32_t toHex() const noexcept {
            uint8_t red = static_cast<uint8_t>(r * 255.f);
            uint8_t green = static_cast<uint8_t>(g * 255.f);
            uint8_t blue = static_cast<uint8_t>(b * 255.f);
            uint8_t alpha = static_cast<uint8_t>(a * 255.f);
            return (red << 24) | (green << 16) | (blue << 8) | alpha;
        }

        Json::Value toJson() const;
        static Color fromJson(const Json::Value &json);

        // std::cout operator
        friend std::ostream &operator<<(std::ostream &os, const Color &color) {
            os << "Color(r=" << color.r << ", g=" << color.g
               << ", b=" << color.b << ", a=" << color.a << ")";
            return os;
        }
    };

    typedef uint32_t TextureHandle;

    enum class QuadRenderPass : uint8_t { Auto, Opaque, Transparent };

    enum class PathLineJoin : uint8_t {
        Miter,
        Bevel,
        Round,
    };

    enum class PathLineCap : uint8_t {
        Butt,
        Round,
        Square,
    };

    enum class PathFillRule : uint8_t {
        NonZero,
        EvenOdd,
    };

    struct ShadowProps {
        bool enabled = false;
        // Offset is in the same coordinate space as the geometry. For normal
        // scene geometry this means world units; for screen-space custom quads
        // this means render-target pixels.
        glm::vec2 offset{0.f, 4.f};
        // CSS-like blur radius in geometry units. Larger values produce softer
        // shadows and expand the generated shadow bounds.
        float blur = 8.f;
        // Positive spread expands the caster silhouette before blur; negative
        // spread contracts it.
        float spread = 0.f;
        Color color{0.f, 0.f, 0.f, 0.35f};
    };

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
        QuadRenderPass renderPass = QuadRenderPass::Auto;

        glm::vec4 radius{0.f}; // Top-left, top-right, bottom-right, bottom-left
        glm::vec4 thickness{0.f}; // same order as radius
        Color borderColor{0.f, 0.f, 0.f, 0.f};
        ShadowProps shadow{};
    };
    struct CircleProps {
        glm::vec2 position{0.f, 0.f};
        float radius = 0.5f;
        float thickness = 0.f; // 0 for filled circle
        float zIndex = 0.f;
        Color color{1.f, 1.f, 1.f, 1.f};
        PickingId id = PickingId::invalid();
        QuadRenderPass renderPass = QuadRenderPass::Auto;
        ShadowProps shadow{};
    };

    struct LineProps {
        glm::vec2 p0{0.f, 0.f};
        glm::vec2 p1{1.f, 1.f};
        float thickness = 1.f;
        float zIndex = 0.f;
        Color color{1.f, 1.f, 1.f, 1.f};
        PickingId id = PickingId::invalid();
        QuadRenderPass renderPass = QuadRenderPass::Auto;
        ShadowProps shadow{};
    };

    struct PathProps {
        Color fillColor{1.f, 1.f, 1.f, 1.f};
        Color strokeColor{1.f, 1.f, 1.f, 1.f};
        float strokeSize = 4.f;
        float miterLimit = 4.f;
        float curveTolerance = 0.25f;
        bool renderFill = false;
        float zIndex = 0.f;
        PickingId id = PickingId::invalid();
        QuadRenderPass renderPass = QuadRenderPass::Auto;
        PathFillRule fillRule = PathFillRule::EvenOdd;
        PathLineJoin lineJoin = PathLineJoin::Miter;
        PathLineCap lineCap = PathLineCap::Round;
        bool closePath = true;
    };

    struct FontProps {
        glm::vec2 position{0.f, 0.f};
        float fontSize = 16.f;
        Color color{1.f, 1.f, 1.f, 1.f};
        float zIndex = 0.f;
        float letterSpacing = 0.f;
        // 0 uses the loaded font's line height at fontSize.
        float lineHeight = 0.f;
        float tabSize = 4.f;
        bool antiAlias = true;
        float antiAliasFringeScale = 1.f;
        PickingId id = PickingId::invalid();
        QuadRenderPass renderPass = QuadRenderPass::Auto;
    };
} // namespace Bess::Core::Renderer

template <>
struct fmt::formatter<Bess::Core::Renderer::Color>
    : fmt::formatter<std::string> {
    auto format(Bess::Core::Renderer::Color color, format_context &ctx) const
        -> decltype(ctx.out()) {
        return fmt::format_to(ctx.out(), "Color({}, {}, {}, {})", color.r,
                              color.g, color.b, color.a);
    }
};
