#pragma once

#include "common/bess_api.h"
#include "bess_core/renderer/colors.h"
#include "bess_core/renderer/renderer_types.h"
#include "bess_json/bess_json.h"
#include "fwd.hpp"
#include <cstdint>

namespace Bess::Canvas {
    class BESS_API Transform {
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

    enum class PinLabelAlignment : uint8_t {
        adjacent, // left or right of the pin, depending on inp or out
        topCenter,
        bottomCenter,
    };

    enum class SchematicLableAlignement : uint8_t {
        center, // center of the component
        topCenter,
        bottomCenter,
    };

    struct BESS_API SchematicStyle {
        PinLabelAlignment pinLabelAlign = PinLabelAlignment::adjacent;
        bool showPinLabels = true;
        SchematicLableAlignement schematicLabelAlign =
            SchematicLableAlignement::center;
        bool showName = true;
        bool flipSlotsX = false;
    };

    class BESS_API Style {
      public:
        Style() = default;
        Style(const Style &other) = default;

        SchematicStyle schematicStyle = {};
        Core::Renderer::Color headerColor = Core::Renderer::Colors::slate900;
        Core::Renderer::Color color = Core::Renderer::Colors::white;
    };

    // Not serialized
    enum class ConnSegOrientaion : uint8_t { horizontal, vertical };

    struct BESS_API ConnSegment {
        glm::vec2 offset;
        ConnSegOrientaion orientation = ConnSegOrientaion::horizontal;
    };

} // namespace Bess::Canvas

REFLECT(Bess::Canvas::Transform, position, scale, angle);
REFLECT_ENUM(Bess::Canvas::PinLabelAlignment);
REFLECT_ENUM(Bess::Canvas::SchematicLableAlignement);
REFLECT(Bess::Canvas::SchematicStyle,
        pinLabelAlign,
        showPinLabels,
        showName,
        flipSlotsX,
        schematicLabelAlign);

REFLECT(Bess::Canvas::Style, schematicStyle, headerColor, color);

REFLECT_ENUM(Bess::Canvas::ConnSegOrientaion);

REFLECT(Bess::Canvas::ConnSegment, offset, orientation);

REFLECT_VECTOR(Bess::Canvas::ConnSegment)
