#pragma once

#include "bess_uuid.h"
#include "entt/entity/entity.hpp"
#include "entt/entity/fwd.hpp"
#include <cstdint>
#include <vector>
#define GLM_ENABLE_EXPERIMENTAL
#include "ext/vector_float3.hpp"
#include "ext/vector_float4.hpp"
#include "fwd.hpp"
#include "gtx/matrix_decompose.hpp"
#include <glm.hpp>
#include <string>

namespace Bess::Canvas::Components {
    struct IdComponent {
        IdComponent() = default;
        IdComponent(UUID uuid) { this->uuid = uuid; }
        IdComponent(const IdComponent &) = default;
        UUID uuid;
    };

    class TransformComponent {
      public:
        TransformComponent() = default;
        TransformComponent(TransformComponent &other) = default;

        std::array<glm::vec3, 3> decompose() {
            glm::vec3 scale;
            glm::quat orientation;
            glm::vec3 translation;
            glm::vec3 _;
            glm::vec4 __;
            glm::decompose(m_transform, scale, orientation, translation, _, __);
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

        operator glm::mat4() {
            return m_transform;
        }

        void operator=(glm::mat4 transform) {
            m_transform = transform;
        }

      private:
        glm::mat4 m_transform{};
    };

    class SpriteComponent {
      public:
        SpriteComponent() = default;
        SpriteComponent(SpriteComponent &other) = default;
        glm::vec4 color = glm::vec4(1.f);
        glm::vec4 borderColor = glm::vec4(1.f);
        glm::vec4 borderSize = glm::vec4(0.f);
        glm::vec4 borderRadius = glm::vec4(0.f);
    };

    class TagComponent {
      public:
        TagComponent() = default;
        TagComponent(TagComponent &other) = default;

        std::string name = "";
    };

    class SelectedComponent {
      public:
        SelectedComponent() = default;
        SelectedComponent(SelectedComponent &other) = default;
        entt::entity id = entt::null;
    };

    enum class SlotType {
        digitalInput,
        digitalOutput,
    };

    class SlotComponent {
      public:
        SlotComponent() = default;
        SlotComponent(SlotComponent &other) = default;
        UUID parentId = 0;
        uint idx = 0;
        SlotType slotType = SlotType::digitalInput;
    };

    class SimulationComponent {
      public:
        SimulationComponent() = default;
        SimulationComponent(SimulationComponent &other) = default;
        UUID simEngineEntity = UUID::null; // this should be mapped to entity in simulator, will be used to fetch state from simulator
        std::vector<UUID> inputSlots = {};
        std::vector<UUID> outputSlots = {};
    };

    class ConnectionSegmentComponent {
      public:
        ConnectionSegmentComponent() = default;
        ConnectionSegmentComponent(ConnectionSegmentComponent &other) = default;

        bool isHead() {
            return prev == UUID::null;
        }

        bool isTail() {
            return next == UUID::null;
        }

        glm::vec2 pos = {};
        UUID parent = UUID::null;
        UUID prev = UUID::null;
        UUID next = UUID::null;
    };

    class ConnectionComponent {
      public:
        ConnectionComponent() = default;
        ConnectionComponent(ConnectionComponent &other) = default;

        UUID inputSlot;
        UUID outputSlot;

        UUID segmentHead = UUID::null;
    };

    class HoveredEntityComponent {
      public:
        HoveredEntityComponent() = default;
        HoveredEntityComponent(HoveredEntityComponent &other) = default;

        entt::entity prevHovered = entt::null;
    };

    class SimulationOutputComponent {
      public:
        SimulationOutputComponent() = default;
        SimulationOutputComponent(SimulationOutputComponent &other) = default;

        bool recordOutput = false;
    };

    class SimulationInputComponent {
      public:
        SimulationInputComponent() = default;
        SimulationInputComponent(SimulationInputComponent &other) = default;

        bool clockBhaviour = false;
    };

} // namespace Bess::Canvas::Components
