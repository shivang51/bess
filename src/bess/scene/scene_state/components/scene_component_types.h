#pragma once
#include "bess_json/bess_json.h"
#include "fwd.hpp"
#include <cstdint>

namespace Bess::Canvas {
    class Transform {
      public:
        Transform() = default;
        Transform(const Transform &other) = default;

        glm::mat4 getTransform() const;

        operator glm::mat4() const { return getTransform(); }

        glm::vec3 position = {0.f, 0.f, 0.f};
        glm::vec2 scale = {100.f, 100.f};
        float angle = 0.f;
    };

    class Style {
      public:
        Style() = default;
        Style(const Style &other) = default;

        glm::vec4 color = glm::vec4(1.f);
        glm::vec4 borderColor = glm::vec4(1.f);
        glm::vec4 borderSize = glm::vec4(0.f);
        glm::vec4 borderRadius = glm::vec4(0.f);
        glm::vec4 headerColor = glm::vec4(0.2f, 0.2f, 0.2f, 1.f);
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
