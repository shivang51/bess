#pragma once

#include "bess_uuid.h"
#include "entt/entity/entity.hpp"
#include "entt/entity/fwd.hpp"
#include "json.hpp"
#include <cstdint>
#include <vector>
#define GLM_ENABLE_EXPERIMENTAL
#include "ext/vector_float3.hpp"
#include "ext/vector_float4.hpp"
#include "fwd.hpp"
#include "gtx/matrix_decompose.hpp"
#include "simulation_engine.h"
#include <glm.hpp>
#include <string>

namespace nlohmann {
    inline void to_json(nlohmann::json &j, const Bess::UUID &uuid) {
        j = (uint64_t)uuid;
    }
    inline void from_json(const nlohmann::json &j, Bess::UUID &uuid) {
        uuid = j.get<uint64_t>();
    }

    inline void to_json(nlohmann::json &j, const glm::vec2 &vec) {
        j = nlohmann::json{vec.x, vec.y};
    }
    inline void from_json(const nlohmann::json &j, glm::vec2 &vec) {
        vec.x = j.at(0).get<float>();
        vec.y = j.at(1).get<float>();
    }

    // glm::vec3
    inline void to_json(nlohmann::json &j, const glm::vec3 &vec) {
        j = nlohmann::json{vec.x, vec.y, vec.z};
    }
    inline void from_json(const nlohmann::json &j, glm::vec3 &vec) {
        vec.x = j.at(0).get<float>();
        vec.y = j.at(1).get<float>();
        vec.z = j.at(2).get<float>();
    }

    // glm::vec4
    inline void to_json(nlohmann::json &j, const glm::vec4 &vec) {
        j = nlohmann::json{vec.x, vec.y, vec.z, vec.w};
    }
    inline void from_json(const nlohmann::json &j, glm::vec4 &vec) {
        vec.x = j.at(0).get<float>();
        vec.y = j.at(1).get<float>();
        vec.z = j.at(2).get<float>();
        vec.w = j.at(3).get<float>();
    }

    // glm::mat4 (stored as an array of 16 floats in column-major order)
    inline void to_json(nlohmann::json &j, const glm::mat4 &mat) {
        std::vector<float> vals;
        vals.reserve(16);
        // GLM matrices are column-major. We store row-major for simplicity.
        for (int row = 0; row < 4; ++row) {
            for (int col = 0; col < 4; ++col) {
                vals.push_back(mat[col][row]);
            }
        }
        j = vals;
    }
    inline void from_json(const nlohmann::json &j, glm::mat4 &mat) {
        std::vector<float> vals = j.get<std::vector<float>>();
        if (vals.size() != 16)
            throw std::runtime_error("Invalid matrix size");
        for (int row = 0; row < 4; ++row) {
            for (int col = 0; col < 4; ++col) {
                mat[col][row] = vals[row * 4 + col];
            }
        }
    }

} // namespace nlohmann

namespace Bess::Canvas::Components {
    // IdComponent
    struct IdComponent {
        IdComponent() = default;
        IdComponent(UUID uuid) { this->uuid = uuid; }
        IdComponent(const IdComponent &) = default;
        UUID uuid;
    };

    inline void to_json(nlohmann::json &j, const IdComponent &comp) {
        j = nlohmann::json{{"uuid", (uint64_t)comp.uuid}};
    }
    inline void from_json(const nlohmann::json &j, IdComponent &comp) {
        comp.uuid = j.at("uuid").get<UUID>();
    }

    // TransformComponent
    class TransformComponent {
      public:
        TransformComponent() = default;
        TransformComponent(const TransformComponent &other) = default;

        std::array<glm::vec3, 3> decompose() {
            glm::vec3 scale;
            glm::quat orientation;
            glm::vec3 translation;
            glm::vec3 skew;
            glm::vec4 perspective;
            glm::decompose(m_transform, scale, orientation, translation, skew, perspective);
            return {translation, glm::vec3(orientation.x, orientation.y, orientation.z), scale};
        }

        void translate(const glm::vec3 &pos) {
            m_transform[3] = glm::vec4(pos, m_transform[3][3]);
        }

        void scale(const glm::vec2 &scale) {
            glm::vec3 xAxis = glm::vec3(m_transform[0]);
            glm::vec3 yAxis = glm::vec3(m_transform[1]);

            if (glm::length(xAxis) > 0.0f)
                xAxis = glm::normalize(xAxis) * scale.x;
            else
                xAxis = glm::vec3(scale.x, 0.0f, 0.0f);

            if (glm::length(yAxis) > 0.0f)
                yAxis = glm::normalize(yAxis) * scale.y;
            else
                yAxis = glm::vec3(0.0f, scale.y, 0.0f);

            m_transform[0] = glm::vec4(xAxis, 0.0f);
            m_transform[1] = glm::vec4(yAxis, 0.0f);
        }

        glm::vec3 getPosition() {
            return glm::vec3(m_transform[3]);
        }

        operator glm::mat4() { return m_transform; }

        void operator=(glm::mat4 transform) { m_transform = transform; }

        glm::mat4 m_transform{};
    };

    inline void to_json(nlohmann::json &j, const TransformComponent &comp) {
        j = nlohmann::json{{"transform", comp.m_transform}};
    }
    inline void from_json(const nlohmann::json &j, TransformComponent &comp) {
        comp.m_transform = j.at("transform").get<glm::mat4>();
    }

    // SpriteComponent
    class SpriteComponent {
      public:
        SpriteComponent() = default;
        SpriteComponent(const SpriteComponent &other) = default;
        glm::vec4 color = glm::vec4(1.f);
        glm::vec4 borderColor = glm::vec4(1.f);
        glm::vec4 borderSize = glm::vec4(0.f);
        glm::vec4 borderRadius = glm::vec4(0.f);
    };

    inline void to_json(nlohmann::json &j, const SpriteComponent &comp) {
        j = nlohmann::json{
            {"color", comp.color},
            {"borderColor", comp.borderColor},
            {"borderSize", comp.borderSize},
            {"borderRadius", comp.borderRadius}};
    }
    inline void from_json(const nlohmann::json &j, SpriteComponent &comp) {
        comp.color = j.at("color").get<glm::vec4>();
        comp.borderColor = j.at("borderColor").get<glm::vec4>();
        comp.borderSize = j.at("borderSize").get<glm::vec4>();
        comp.borderRadius = j.at("borderRadius").get<glm::vec4>();
    }

    // TagComponent
    class TagComponent {
      public:
        TagComponent() = default;
        TagComponent(const TagComponent &other) = default;
        std::string name = "";
    };

    inline void to_json(nlohmann::json &j, const TagComponent &comp) {
        j = nlohmann::json{{"name", comp.name}};
    }
    inline void from_json(const nlohmann::json &j, TagComponent &comp) {
        comp.name = j.at("name").get<std::string>();
    }

    // SelectedComponent
    class SelectedComponent {
      public:
        SelectedComponent() = default;
        SelectedComponent(const SelectedComponent &other) = default;
        // Assuming entt::entity is convertible to an integer.
        unsigned int id = 0;
    };

    inline void to_json(nlohmann::json &j, const SelectedComponent &comp) {
        j = nlohmann::json{{"id", comp.id}};
    }
    inline void from_json(const nlohmann::json &j, SelectedComponent &comp) {
        comp.id = j.at("id").get<unsigned int>();
    }

    // SlotType enum
    enum class SlotType {
        digitalInput,
        digitalOutput,
    };

    // Helper conversion for SlotType
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

    // SlotComponent
    class SlotComponent {
      public:
        SlotComponent() = default;
        SlotComponent(const SlotComponent &other) = default;
        UUID parentId = 0;
        uint32_t idx = 0;
        SlotType slotType = SlotType::digitalInput;
    };

    inline void to_json(nlohmann::json &j, const SlotComponent &comp) {
        j = nlohmann::json{
            {"parentId", comp.parentId},
            {"idx", comp.idx},
            {"slotType", slotTypeToString(comp.slotType)}};
    }
    inline void from_json(const nlohmann::json &j, SlotComponent &comp) {
        comp.parentId = j.at("parentId").get<UUID>();
        comp.idx = j.at("idx").get<uint32_t>();
        comp.slotType = stringToSlotType(j.at("slotType").get<std::string>());
    }

    // SimulationComponent
    class SimulationComponent {
      public:
        SimulationComponent() = default;
        SimulationComponent(const SimulationComponent &other) = default;
        UUID simEngineEntity = UUID::null; // mapped to entity in simulator
        std::vector<UUID> inputSlots = {};
        std::vector<UUID> outputSlots = {};
    };

    inline void to_json(nlohmann::json &j, const SimulationComponent &comp) {
        j = nlohmann::json{
            {"simEngineEntity", comp.simEngineEntity},
            {"inputSlots", comp.inputSlots},
            {"outputSlots", comp.outputSlots}};
    }
    inline void from_json(const nlohmann::json &j, SimulationComponent &comp) {
        comp.simEngineEntity = j.at("simEngineEntity").get<UUID>();
        comp.inputSlots = j.at("inputSlots").get<std::vector<UUID>>();
        comp.outputSlots = j.at("outputSlots").get<std::vector<UUID>>();
    }

    // ConnectionSegmentComponent
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

    inline void to_json(nlohmann::json &j, const ConnectionSegmentComponent &comp) {
        j = nlohmann::json{
            {"pos", comp.pos},
            {"parent", comp.parent},
            {"prev", comp.prev},
            {"next", comp.next}};
    }
    inline void from_json(const nlohmann::json &j, ConnectionSegmentComponent &comp) {
        comp.pos = j.at("pos").get<glm::vec2>();
        comp.parent = j.at("parent").get<UUID>();
        comp.prev = j.at("prev").get<UUID>();
        comp.next = j.at("next").get<UUID>();
    }

    // ConnectionComponent
    class ConnectionComponent {
      public:
        ConnectionComponent() = default;
        ConnectionComponent(const ConnectionComponent &other) = default;
        UUID inputSlot = 0;
        UUID outputSlot = 0;
        UUID segmentHead = 0;
    };

    inline void to_json(nlohmann::json &j, const ConnectionComponent &comp) {
        j = nlohmann::json{
            {"inputSlot", comp.inputSlot},
            {"outputSlot", comp.outputSlot},
            {"segmentHead", comp.segmentHead}};
    }
    inline void from_json(const nlohmann::json &j, ConnectionComponent &comp) {
        comp.inputSlot = j.at("inputSlot").get<UUID>();
        comp.outputSlot = j.at("outputSlot").get<UUID>();
        comp.segmentHead = j.at("segmentHead").get<UUID>();
    }

    // HoveredEntityComponent
    class HoveredEntityComponent {
      public:
        HoveredEntityComponent() = default;
        HoveredEntityComponent(HoveredEntityComponent &other) = default;
        // Assuming entt::entity is convertible to an unsigned int.
        unsigned int prevHovered = 0;
    };

    inline void to_json(nlohmann::json &j, const HoveredEntityComponent &comp) {
        j = nlohmann::json{{"prevHovered", comp.prevHovered}};
    }
    inline void from_json(const nlohmann::json &j, HoveredEntityComponent &comp) {
        comp.prevHovered = j.at("prevHovered").get<unsigned int>();
    }

    // SimulationOutputComponent
    class SimulationOutputComponent {
      public:
        SimulationOutputComponent() = default;
        SimulationOutputComponent(const SimulationOutputComponent &other) = default;
        bool recordOutput = false;
    };

    inline void to_json(nlohmann::json &j, const SimulationOutputComponent &comp) {
        j = nlohmann::json{{"recordOutput", comp.recordOutput}};
    }
    inline void from_json(const nlohmann::json &j, SimulationOutputComponent &comp) {
        comp.recordOutput = j.at("recordOutput").get<bool>();
    }

    // SimulationInputComponent
    class SimulationInputComponent {
      public:
        SimulationInputComponent() = default;
        SimulationInputComponent(const SimulationInputComponent &other) = default;

        bool updateClock(const UUID &uuid) {
            return SimEngine::SimulationEngine::instance().updateClock(uuid, clockBhaviour, frequency, frequencyUnit);
        }

        bool clockBhaviour = false;
        float frequency = 1.f;
        SimEngine::FrequencyUnit frequencyUnit = SimEngine::FrequencyUnit::hz;
    };

    inline void to_json(nlohmann::json &j, const SimulationInputComponent &comp) {
        j = nlohmann::json{{"clockBhaviour", comp.clockBhaviour}};
        j["frequency"] = comp.frequency;
        j["frequencyUnit"] = (int)comp.frequencyUnit;
    }
    inline void from_json(const nlohmann::json &j, SimulationInputComponent &comp) {
        comp.clockBhaviour = j.at("clockBhaviour").get<bool>();
        comp.frequencyUnit = j.at("frequencyUnit").get<SimEngine::FrequencyUnit>();
        comp.frequency = j.at("frequency").get<float>();
    }
} // namespace Bess::Canvas::Components
