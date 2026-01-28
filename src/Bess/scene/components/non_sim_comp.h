#pragma once
#include "glm.hpp"
#include "json/json.h"
#include <string>
#include <unordered_map>

namespace Bess::Canvas::Components {
    /// @brief Non-simulation component types.
    enum class NSComponentType {
        EMPTY = -1,
        text,
    };

    /// Non Simulation Component
    struct NSComponent {
        NSComponent() = default;
        NSComponent(NSComponentType type, const std::string &name) : type(type), name(name) {
        }
        NSComponent(const NSComponent &other) = default;
        NSComponentType type = NSComponentType::EMPTY;
        std::string name;
    };

    std::vector<NSComponent> getNSComponents();
} // namespace Bess::Canvas::Components

namespace Bess::JsonConvert {
    using namespace Bess::Canvas::Components;

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

} // namespace Bess::JsonConvert
