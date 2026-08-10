#pragma once

#include "common/bess_api.h"

#include "common/types.h"
#include "ext/vector_float2.hpp"
#include "ext/vector_float4.hpp"
#include "json/value.h"
#include <cstdint>
#include <format>

namespace Bess::Core::Renderer {

    struct BESS_API Color {
        float r = 0.f, g = 0.f, b = 0.f, a = 1.f;

        constexpr Color() noexcept = default;

        constexpr Color(float red,
                        float green,
                        float blue,
                        float alpha = 1.f) noexcept
            : r(red),
              g(green),
              b(blue),
              a(alpha) {
        }

        constexpr Color withAlpha(float alpha) const {
            return {r, g, b, alpha};
        }

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
              a(vec.a) {
        }

        constexpr operator glm::vec4() const noexcept {
            return {r, g, b, a};
        }

        static constexpr Color fromHex(uint32_t hex) noexcept {
            Color col;
            col.r = (float)((hex >> 24) & 0xFF) / 255.f;
            col.g = (float)((hex >> 16) & 0xFF) / 255.f;
            col.b = (float)((hex >> 8) & 0xFF) / 255.f;
            col.a = (float)(hex & 0xFF) / 255.f;
            return col;
        }

        // RGBA order each val between 0-255, alpha defaults to 255 (opaque)
        static constexpr Color fromRGBA8(uint8_t red,
                                         uint8_t green,
                                         uint8_t blue,
                                         uint8_t alpha = 255) noexcept {
            Color col;
            col.r = (float)red / 255.f;
            col.g = (float)green / 255.f;
            col.b = (float)blue / 255.f;
            col.a = (float)alpha / 255.f;
            return col;
        }

        static constexpr Color fromARGB(uint32_t argb) noexcept {
            uint8_t alpha = (argb >> 24) & 0xFF;
            uint8_t red = (argb >> 16) & 0xFF;
            uint8_t green = (argb >> 8) & 0xFF;
            uint8_t blue = argb & 0xFF;
            return fromRGBA8(red, green, blue, alpha);
        }

        // Converts the color to a 32-bit hex value in RGBA order
        constexpr uint32_t toHex() const noexcept {
            uint8_t red = static_cast<uint8_t>(r * 255.f);
            uint8_t green = static_cast<uint8_t>(g * 255.f);
            uint8_t blue = static_cast<uint8_t>(b * 255.f);
            uint8_t alpha = static_cast<uint8_t>(a * 255.f);
            return (red << 24) | (green << 16) | (blue << 8) | alpha;
        }

        // Converts the color to a 32-bit hex value in ABGR order
        // Useful for dear imgui
        constexpr uint32_t toHexRev() const noexcept {
            uint8_t red = static_cast<uint8_t>(r * 255.f);
            uint8_t green = static_cast<uint8_t>(g * 255.f);
            uint8_t blue = static_cast<uint8_t>(b * 255.f);
            uint8_t alpha = static_cast<uint8_t>(a * 255.f);
            return (alpha << 24) | (blue << 16) | (green << 8) | red;
        }

        constexpr uint32_t toARGB8() const noexcept {
            uint8_t alpha = static_cast<uint8_t>(a * 255.f);
            uint8_t red = static_cast<uint8_t>(r * 255.f);
            uint8_t green = static_cast<uint8_t>(g * 255.f);
            uint8_t blue = static_cast<uint8_t>(b * 255.f);
            return (alpha << 24) | (red << 16) | (green << 8) | blue;
        }

        Json::Value toJson() const;
        static Color fromJson(const Json::Value &json);

        Color operator*(float scalar) const {
            return {r * scalar, g * scalar, b * scalar, a * scalar};
        }

        // std::cout operator
        friend std::ostream &operator<<(std::ostream &os, const Color &color) {
            os << "Color(r=" << color.r << ", g=" << color.g
               << ", b=" << color.b << ", a=" << color.a << ")";
            return os;
        }

        float *data();
    };

    typedef uint32_t TextureHandle;

    enum class QuadRenderPass : uint8_t { Auto, Opaque, Transparent };

    enum class RenderTransformMode : uint8_t {
        // Positions and sizes are scene/world units and the active camera
        // transform is applied.
        Camera,
        // Positions and sizes are render-target pixels with origin at the
        // target center; the active camera transform is ignored.
        Screen,
    };

    struct BESS_API RendererScissorRect {
        uint32_t x = 0;
        uint32_t y = 0;
        uint32_t width = 0;
        uint32_t height = 0;

        [[nodiscard]] constexpr bool empty() const noexcept {
            return width == 0 || height == 0;
        }

        [[nodiscard]] constexpr bool
        operator==(const RendererScissorRect &other) const noexcept {
            return x == other.x && y == other.y && width == other.width &&
                   height == other.height;
        }

        [[nodiscard]] constexpr bool
        operator!=(const RendererScissorRect &other) const noexcept {
            return !(*this == other);
        }
    };

    struct BESS_API RendererScissorState {
        bool enabled = false;
        RendererScissorRect rect{};

        [[nodiscard]] constexpr bool empty() const noexcept {
            return enabled && rect.empty();
        }

        [[nodiscard]] constexpr bool
        operator==(const RendererScissorState &other) const noexcept {
            return enabled == other.enabled && rect == other.rect;
        }

        [[nodiscard]] constexpr bool
        operator!=(const RendererScissorState &other) const noexcept {
            return !(*this == other);
        }
    };

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

    struct BESS_API ShadowProps {
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

    struct BESS_API QuadProps {
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
        RenderTransformMode transformMode = RenderTransformMode::Camera;

        glm::vec4 radius{0.f}; // Top-left, top-right, bottom-right, bottom-left
        glm::vec4 thickness{0.f}; // same order as radius
        Color borderColor{0.f, 0.f, 0.f, 0.f};
        ShadowProps shadow{};
    };
    struct BESS_API CircleProps {
        glm::vec2 position{0.f, 0.f};
        float radius = 0.5f;
        float thickness = 0.f; // 0 for filled circle
        float zIndex = 0.f;
        Color color{1.f, 1.f, 1.f, 1.f};
        PickingId id = PickingId::invalid();
        QuadRenderPass renderPass = QuadRenderPass::Auto;
        RenderTransformMode transformMode = RenderTransformMode::Camera;
        ShadowProps shadow{};
    };

    struct BESS_API LineProps {
        glm::vec2 p0{0.f, 0.f};
        glm::vec2 p1{1.f, 1.f};
        float thickness = 1.f;
        float zIndex = 0.f;
        Color color{1.f, 1.f, 1.f, 1.f};
        PickingId id = PickingId::invalid();
        QuadRenderPass renderPass = QuadRenderPass::Auto;
        RenderTransformMode transformMode = RenderTransformMode::Camera;
        ShadowProps shadow{};
    };

    struct BESS_API PathProps {
        Color fillColor{1.f, 1.f, 1.f, 1.f};
        Color strokeColor{1.f, 1.f, 1.f, 1.f};
        float strokeSize = 4.f;
        float miterLimit = 4.f;
        float jointRadius = 0.f;
        float curveTolerance = 0.25f;
        bool renderFill = false;
        float zIndex = 0.f;
        PickingId id = PickingId::invalid();
        QuadRenderPass renderPass = QuadRenderPass::Auto;
        PathFillRule fillRule = PathFillRule::EvenOdd;
        PathLineJoin lineJoin = PathLineJoin::Miter;
        PathLineCap lineCap = PathLineCap::Round;
        bool closePath = true;
        RenderTransformMode transformMode = RenderTransformMode::Camera;
        glm::vec2 position{0.f, 0.f};
        glm::vec2 scale{1.f, 1.f};
        float rotation = 0.f;
        bool hasRotationPivot = false;
        glm::vec2 rotationPivot{0.f, 0.f};
    };

    struct BESS_API FontProps {
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
        RenderTransformMode transformMode = RenderTransformMode::Camera;
    };
} // namespace Bess::Core::Renderer

template <>
struct BESS_API std::formatter<Bess::Core::Renderer::Color>
    : std::formatter<std::string> {
    auto format(Bess::Core::Renderer::Color color,
                std::format_context &ctx) const -> decltype(ctx.out()) {
        return std::format_to(ctx.out(),
                              "Color({}, {}, {}, {})",
                              color.r,
                              color.g,
                              color.b,
                              color.a);
    }
};

namespace Bess::JsonConvert {
    BESS_API void toJsonValue(const Core::Renderer::Color &color,
                              Json::Value &j);
    BESS_API void fromJsonValue(const Json::Value &j,
                                Core::Renderer::Color &color);
} // namespace Bess::JsonConvert
