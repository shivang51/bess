#pragma once
#include "bess_core/renderer/colors.h"
#include "bess_core/renderer/renderer_types.h"
#include "bess_json/bess_json.h"
#include "fwd.hpp"
#include <cstdint>

namespace Bess::Canvas {
    class Transform {
      public:
        Transform() = default;
        Transform(const Transform &other) = default;

        glm::mat4 getTransform() const;

        operator glm::mat4() const {
            return getTransform();
        }

        glm::vec3 position = {0.f, 0.f, 0.f};
        glm::vec2 scale = {100.f, 100.f};
        float angle = 0.f;
    };

    class Style {
      public:
        Style() = default;
        Style(const Style &other) = default;

        Core::Renderer::Color color = Core::Renderer::Colors::white;
        Core::Renderer::Color borderColor = Core::Renderer::Colors::black;
        glm::vec4 borderSize = glm::vec4(0.f);
        glm::vec4 borderRadius = glm::vec4(0.f);
        Core::Renderer::Color headerColor = Core::Renderer::Colors::slate900;
    };

    // Not serialized
    enum class ConnSegOrientaion : uint8_t { horizontal, vertical };

    struct ConnSegment {
        glm::vec2 offset;
        ConnSegOrientaion orientation = ConnSegOrientaion::horizontal;
    };

} // namespace Bess::Canvas

REFLECT(Bess::Canvas::Transform, position, scale, angle);

REFLECT(Bess::Canvas::Style,
        color,
        borderColor,
        borderSize,
        borderRadius,
        headerColor);

REFLECT_ENUM(Bess::Canvas::ConnSegOrientaion);

REFLECT(Bess::Canvas::ConnSegment, offset, orientation);

REFLECT_VECTOR(Bess::Canvas::ConnSegment)
