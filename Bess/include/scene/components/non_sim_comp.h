#pragma once
#include "json/json.h"
#include "scene/components/json_converters.h"
#include <string>
#include <unordered_map>

namespace Bess::Canvas::Components {
    // TextNodeComponent
    class TextNodeComponent {
      public:
        TextNodeComponent() = default;
        TextNodeComponent(const TextNodeComponent &other) = default;

        std::string text = "";
        glm::vec4 color = {1.f, 1.f, 1.f, 1.f};
        float fontSize = 16.f;
    };

    /// @brief Non-simulation component types.
    enum class NSComponentType {
        EMPTY = -1,
        text,
    };

    struct NSComponent {
        NSComponentType type;
        std::string name;
    };

    std::vector<NSComponent> getNSComponents();
} // namespace Bess::Canvas::Components

namespace Bess::JsonConvert {
    using namespace Bess::Canvas::Components;
    inline void toJsonValue(const TextNodeComponent &comp, Json::Value &j) {
        j = Json::Value(Json::objectValue);

        j["text"] = comp.text;
        j["fontSize"] = comp.fontSize;

        toJsonValue(comp.color, j["color"]);
    }
}