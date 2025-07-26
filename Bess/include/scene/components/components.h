#pragma once

#include "bess_uuid.h"
#include "non_sim_comp.h"
#include "common/log.h"
#include "scene/components/json_converters.h"
#include "entt/entity/entity.hpp"
#include "entt/entity/fwd.hpp"
#include "json/json.h"
#include <cstdint>
#include <vector>
#define GLM_ENABLE_EXPERIMENTAL
#include "glm.hpp"
#include "gtx/matrix_decompose.hpp"
#include "simulation_engine.h"
#include <glm.hpp>
#include <string>

namespace Bess::Canvas::Components {

    using IdComponent = Bess::SimEngine::IdComponent;

    class TransformComponent {
      public:
        TransformComponent() = default;
        TransformComponent(const TransformComponent &other) = default;

        glm::mat4 getTransform() {
            auto transform = glm::translate(glm::mat4(1), position);
            transform = glm::rotate(transform, angle, {0.f, 0.f, 1.f});
            transform = glm::scale(transform, glm::vec3(scale, 1.f));
            return transform;
        }

        operator glm::mat4() { return getTransform(); }

        glm::vec3 position = {0.f, 0.f, 0.f};
        glm::vec2 scale = {1.f, 1.f};
        float angle = 0.f;
    };

    class SpriteComponent {
      public:
        SpriteComponent() = default;
        SpriteComponent(const SpriteComponent &other) = default;
        glm::vec4 color = glm::vec4(1.f);
        glm::vec4 borderColor = glm::vec4(1.f);
        glm::vec4 borderSize = glm::vec4(0.f);
        glm::vec4 borderRadius = glm::vec4(0.f);
    };

    class TagComponent {
      public:
        TagComponent() = default;
        TagComponent(const TagComponent &other) = default;
        std::string name = "";

        union CompType{
            int typeId = -1;
            SimEngine::ComponentType simCompType;
            Components::NSComponentType nsCompType;
        } type;

        bool isSimComponent = false;
    };

    class SelectedComponent {
      public:
        SelectedComponent() = default;
        SelectedComponent(const SelectedComponent &other) = default;
        // Assuming entt::entity is convertible to an integer.
        unsigned int id = 0;
    };

    enum class SlotType {
        digitalInput,
        digitalOutput,
    };

    inline std::string slotTypeToString(SlotType type) {
        switch (type) {
        case SlotType::digitalInput:
            return "digitalInput";
        case SlotType::digitalOutput:
            return "digitalOutput";
        default:
            return "unknown";
        }
    }

    inline SlotType stringToSlotType(const std::string &s) {
        if (s == "digitalInput")
            return SlotType::digitalInput;
        if (s == "digitalOutput")
            return SlotType::digitalOutput;
        throw std::runtime_error("Invalid SlotType string: " + s);
    }

    class SlotComponent {
      public:
        SlotComponent() = default;
        SlotComponent(const SlotComponent &other) = default;
        UUID parentId = 0;
        uint32_t idx = 0;
        SlotType slotType = SlotType::digitalInput;
    };

    class SimulationComponent {
      public:
        SimulationComponent() = default;
        SimulationComponent(const SimulationComponent &other) = default;
        UUID simEngineEntity = UUID::null; // mapped to entity in simulator
        std::vector<UUID> inputSlots = {};
        std::vector<UUID> outputSlots = {};
    };

    class ConnectionSegmentComponent {
      public:
        ConnectionSegmentComponent() = default;
        ConnectionSegmentComponent(const ConnectionSegmentComponent &other) = default;
        bool isHead() const { return prev == UUID::null; }
        bool isTail() const { return next == UUID::null; }
        glm::vec2 pos = {};
        UUID parent = UUID::null;
        UUID prev = UUID::null;
        UUID next = UUID::null;
    };

    class ConnectionComponent {
      public:
        ConnectionComponent() = default;
        ConnectionComponent(const ConnectionComponent &other) = default;
        UUID inputSlot = 0;
        UUID outputSlot = 0;
        UUID segmentHead = 0;
        bool useCustomColor = false;
    };

    class HoveredEntityComponent {
      public:
        HoveredEntityComponent() = default;
        HoveredEntityComponent(HoveredEntityComponent &other) = default;
        // Assuming entt::entity is convertible to an unsigned int.
        unsigned int prevHovered = 0;
    };

    class SimulationOutputComponent {
      public:
        SimulationOutputComponent() = default;
        SimulationOutputComponent(const SimulationOutputComponent &other) = default;
        bool recordOutput = false;
    };

    class SimulationInputComponent {
      public:
        SimulationInputComponent() = default;
        SimulationInputComponent(const SimulationInputComponent &other) = default;

        bool updateClock(const UUID &uuid) const {
            return SimEngine::SimulationEngine::instance().updateClock(uuid, clockBhaviour, frequency, frequencyUnit);
        }

        bool clockBhaviour = false;
        float frequency = 1.f;
        SimEngine::FrequencyUnit frequencyUnit = SimEngine::FrequencyUnit::hz;
    };
} // namespace Bess::Canvas::Components

namespace Bess::JsonConvert {
    using namespace Bess::Canvas::Components;

    // --- TransformComponent ---

    /**
     * @brief Converts a TransformComponent to a Json::Value object.
     * @param comp The source TransformComponent.
     * @param j The destination Json::Value to be populated.
     */
    inline void toJsonValue(const TransformComponent &comp, Json::Value &j) {
        j = Json::Value(Json::objectValue);

        toJsonValue(comp.position, j["position"]);
        toJsonValue(comp.scale, j["scale"]);

        j["angle"] = comp.angle;
    }

    /**
     * @brief Converts a Json::Value object back to a TransformComponent.
     * @param j The source Json::Value.
     * @param comp The destination TransformComponent to be populated.
     */
    inline void fromJsonValue(const Json::Value &j, TransformComponent &comp) {
        if (!j.isObject()) {
            return;
        }

        if (j.isMember("position")) {
            fromJsonValue(j["position"], comp.position);
        }
        if (j.isMember("scale")) {
            fromJsonValue(j["scale"], comp.scale);
        }
        if (j.isMember("angle")) {
			comp.angle = j.get("angle", 0.f).asFloat();
        }
    }

    // --- SpriteComponent ---

    /**
     * @brief Converts a SpriteComponent to a Json::Value object.
     */
    inline void toJsonValue(const SpriteComponent &comp, Json::Value &j) {
        j = Json::Value(Json::objectValue);
        toJsonValue(comp.color, j["color"]);
        toJsonValue(comp.borderColor, j["borderColor"]);
        toJsonValue(comp.borderSize, j["borderSize"]);
        toJsonValue(comp.borderRadius, j["borderRadius"]);
    }

    /**
     * @brief Converts a Json::Value object back to a SpriteComponent.
     */
    inline void fromJsonValue(const Json::Value &j, SpriteComponent &comp) {
        if (!j.isObject()) {
            return;
        }
        if (j.isMember("color")) {
            fromJsonValue(j["color"], comp.color);
        }
        if (j.isMember("borderColor")) {
            fromJsonValue(j["borderColor"], comp.borderColor);
        }
        if (j.isMember("borderSize")) {
            fromJsonValue(j["borderSize"], comp.borderSize);
        }
        if (j.isMember("borderRadius")) {
            fromJsonValue(j["borderRadius"], comp.borderRadius);
        }
    }

    // --- TagComponent ---

    /**
     * @brief Converts a TagComponent to a Json::Value object.
     */
    inline void toJsonValue(const TagComponent &comp, Json::Value &j) {
        j = Json::Value(Json::objectValue);
        j["name"] = comp.name;
        j["type"] = comp.type.typeId;
        j["isSimComponent"] = comp.isSimComponent;
    }

    /**
     * @brief Converts a Json::Value object back to a TagComponent.
     */
    inline void fromJsonValue(const Json::Value &j, TagComponent &comp) {
        if (!j.isObject()) {
            return;
        }
        comp.name = j.get("name", "").asString();
        comp.type.typeId = j.get("type", -1).asInt();
        comp.isSimComponent = j.get("isSimComponent", true).asBool();
    }

    // --- SelectedComponent ---

    /**
     * @brief Converts a SelectedComponent to a Json::Value object.
     */
    inline void toJsonValue(const SelectedComponent &comp, Json::Value &j) {
        j = Json::Value(Json::objectValue);
        j["id"] = comp.id;
    }

    /**
     * @brief Converts a Json::Value object back to a SelectedComponent.
     */
    inline void fromJsonValue(const Json::Value &j, SelectedComponent &comp) {
        if (j.isObject()) {
            comp.id = j.get("id", 0).asUInt();
        }
    }

    // --- SlotComponent ---

    /**
     * @brief Converts a SlotComponent to a Json::Value object.
     */
    inline void toJsonValue(const SlotComponent &comp, Json::Value &j) {
        j = Json::Value(Json::objectValue);
        toJsonValue(comp.parentId, j["parentId"]);
        j["idx"] = comp.idx;
        j["slotType"] = slotTypeToString(comp.slotType);
    }

    /**
     * @brief Converts a Json::Value object back to a SlotComponent.
     */
    inline void fromJsonValue(const Json::Value &j, SlotComponent &comp) {
        if (!j.isObject()) {
            return;
        }
        if (j.isMember("parentId")) {
            fromJsonValue(j["parentId"], comp.parentId);
        }
        comp.idx = j.get("idx", 0).asUInt();
        if (j.isMember("slotType")) {
            comp.slotType = stringToSlotType(j["slotType"].asString());
        }
    }

    // --- SimulationComponent ---

    /**
     * @brief Converts a SimulationComponent to a Json::Value object.
     */
    inline void toJsonValue(const SimulationComponent &comp, Json::Value &j) {
        j = Json::Value(Json::objectValue);
        toJsonValue(comp.simEngineEntity, j["simEngineEntity"]);

        Json::Value &inputSlotsArray = j["inputSlots"] = Json::Value(Json::arrayValue);
        for (const auto &slotId : comp.inputSlots) {
            inputSlotsArray.append(static_cast<Json::UInt64>(slotId));
        }

        Json::Value &outputSlotsArray = j["outputSlots"] = Json::Value(Json::arrayValue);
        for (const auto &slotId : comp.outputSlots) {
            outputSlotsArray.append(static_cast<Json::UInt64>(slotId));
        }
    }

    /**
     * @brief Converts a Json::Value object back to a SimulationComponent.
     */
    inline void fromJsonValue(const Json::Value &j, SimulationComponent &comp) {
        if (!j.isObject()) {
            return;
        }
        if (j.isMember("simEngineEntity")) {
            fromJsonValue(j["simEngineEntity"], comp.simEngineEntity);
        }

        if (j.isMember("inputSlots")) {
            comp.inputSlots.clear();
            for (const auto &slotJson : j["inputSlots"]) {
                comp.inputSlots.push_back(static_cast<UUID>(slotJson.asUInt64()));
            }
        }

        if (j.isMember("outputSlots")) {
            comp.outputSlots.clear();
            for (const auto &slotJson : j["outputSlots"]) {
                comp.outputSlots.push_back(static_cast<UUID>(slotJson.asUInt64()));
            }
        }
    }

    // --- ConnectionSegmentComponent ---

    /**
     * @brief Converts a ConnectionSegmentComponent to a Json::Value object.
     */
    inline void toJsonValue(const ConnectionSegmentComponent &comp, Json::Value &j) {
        j = Json::Value(Json::objectValue);
        toJsonValue(comp.pos, j["pos"]);
        toJsonValue(comp.parent, j["parent"]);
        toJsonValue(comp.prev, j["prev"]);
        toJsonValue(comp.next, j["next"]);
    }

    /**
     * @brief Converts a Json::Value object back to a ConnectionSegmentComponent.
     */
    inline void fromJsonValue(const Json::Value &j, ConnectionSegmentComponent &comp) {
        if (!j.isObject()) {
            return;
        }
        if (j.isMember("pos")) {
            fromJsonValue(j["pos"], comp.pos);
        }
        if (j.isMember("parent")) {
            fromJsonValue(j["parent"], comp.parent);
        }
        if (j.isMember("prev")) {
            fromJsonValue(j["prev"], comp.prev);
        }
        if (j.isMember("next")) {
            fromJsonValue(j["next"], comp.next);
        }
    }

    // --- ConnectionComponent ---

    /**
     * @brief Converts a ConnectionComponent to a Json::Value object.
     */
    inline void toJsonValue(const ConnectionComponent &comp, Json::Value &j) {
        j = Json::Value(Json::objectValue);
        toJsonValue(comp.inputSlot, j["inputSlot"]);
        toJsonValue(comp.outputSlot, j["outputSlot"]);
        toJsonValue(comp.segmentHead, j["segmentHead"]);
        j["useCustomColor"] = comp.useCustomColor;
    }

    /**
     * @brief Converts a Json::Value object back to a ConnectionComponent.
     */
    inline void fromJsonValue(const Json::Value &j, ConnectionComponent &comp) {
        if (!j.isObject()) {
            return;
        }
        if (j.isMember("inputSlot")) {
            fromJsonValue(j["inputSlot"], comp.inputSlot);
        }
        if (j.isMember("outputSlot")) {
            fromJsonValue(j["outputSlot"], comp.outputSlot);
        }
        if (j.isMember("segmentHead")) {
            fromJsonValue(j["segmentHead"], comp.segmentHead);
        }
        comp.useCustomColor = j.get("useCustomColor", false).asBool();
    }

    // --- HoveredEntityComponent ---

    /**
     * @brief Converts a HoveredEntityComponent to a Json::Value object.
     */
    inline void toJsonValue(const HoveredEntityComponent &comp, Json::Value &j) {
        j = Json::Value(Json::objectValue);
        j["prevHovered"] = comp.prevHovered;
    }

    /**
     * @brief Converts a Json::Value object back to a HoveredEntityComponent.
     */
    inline void fromJsonValue(const Json::Value &j, HoveredEntityComponent &comp) {
        if (j.isObject()) {
            comp.prevHovered = j.get("prevHovered", 0).asUInt();
        }
    }

    // --- SimulationOutputComponent ---

    /**
     * @brief Converts a SimulationOutputComponent to a Json::Value object.
     */
    inline void toJsonValue(const SimulationOutputComponent &comp, Json::Value &j) {
        j = Json::Value(Json::objectValue);
        j["recordOutput"] = comp.recordOutput;
    }

    /**
     * @brief Converts a Json::Value object back to a SimulationOutputComponent.
     */
    inline void fromJsonValue(const Json::Value &j, SimulationOutputComponent &comp) {
        if (j.isObject()) {
            comp.recordOutput = j.get("recordOutput", false).asBool();
        }
    }

    // --- SimulationInputComponent ---

    /**
     * @brief Converts a SimulationInputComponent to a Json::Value object.
     */
    inline void toJsonValue(const SimulationInputComponent &comp, Json::Value &j) {
        j = Json::Value(Json::objectValue);
        j["clockBhaviour"] = comp.clockBhaviour;
        j["frequency"] = comp.frequency;
        j["frequencyUnit"] = (int)comp.frequencyUnit;
    }

    /**
     * @brief Converts a Json::Value object back to a SimulationInputComponent.
     */
    inline void fromJsonValue(const Json::Value &j, SimulationInputComponent &comp) {
        if (!j.isObject()) {
            return;
        }
        comp.clockBhaviour = j.get("clockBhaviour", false).asBool();
        comp.frequency = j.get("frequency", 1.f).asFloat();
        if (j.isMember("frequencyUnit")) {
            comp.frequencyUnit = static_cast<SimEngine::FrequencyUnit>(j["frequencyUnit"].asInt());
        }
    }
}
