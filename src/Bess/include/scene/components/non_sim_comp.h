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

    /// Non Simulation Component
    struct NSComponent {
        NSComponent() = default;
        NSComponent(NSComponentType type, const std::string& name){
            this->type = type;
            this->name = name;
        }
        NSComponent(const NSComponent &other) = default;
        NSComponentType type;
        std::string name;
    };

    std::vector<NSComponent> getNSComponents();
} // namespace Bess::Canvas::Components

namespace Bess::JsonConvert {
    using namespace Bess::Canvas::Components;
    
    // --- TextNodeComponent --- 
    inline void toJsonValue(const TextNodeComponent &comp, Json::Value &j) {
        j = Json::Value(Json::objectValue);

        j["text"] = comp.text;
        j["fontSize"] = comp.fontSize;

        toJsonValue(comp.color, j["color"]);
    }

    inline void fromJsonValue(const Json::Value &j, TextNodeComponent &comp) {
        if (!j.isObject()) {
            return;
        }

        comp.text = j.get("text", "").asString();
        comp.fontSize = j.get("fontSize", 16.f).asFloat();

        if (j.isMember("color")) {
            fromJsonValue(j["color"], comp.color);
        }
    }

    // ---- NSComponent ----
    inline void toJsonValue(const NSComponent &comp, Json::Value &j) {
        j = Json::Value(Json::objectValue);

        j["type"] = static_cast<int>(comp.type);
        j["name"] = comp.name;
    }

    inline void fromJsonValue(const Json::Value &j, NSComponent &comp) {
        if (!j.isObject()) {
            return;
        }

        comp.type = static_cast<NSComponentType>(j.get("type", -1).asInt());
        comp.name = j.get("name", "").asString();
    }

}