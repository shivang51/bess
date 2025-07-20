#pragma once
#include "json.hpp"
#include "json_convert_helpers.h"
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

    inline void to_json(nlohmann::json &j, const TextNodeComponent &comp) {
        j = nlohmann::json{
            {"text", comp.text},
            {"color", comp.color},
            {"fontSize", comp.fontSize}};
    }
    inline void from_json(const nlohmann::json &j, TextNodeComponent &comp) {
        comp.text = j.at("text").get<std::string>();
        comp.color = j.at("color").get<glm::vec4>();
        comp.fontSize = j.at("fontSize").get<float>();
    }

    /// @brief Non-simulation component types.
    enum class NSComponentType {
        EMPTY = -1,
        text,
    };


    struct NSComponent{
        NSComponentType type;
        std::string name;
    };

    std::vector<NSComponent> getNSComponents();
}