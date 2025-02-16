#pragma once

#include <cstdint>
#define GLM_ENABLE_EXPERIMENTAL
#include "ext/vector_float3.hpp"
#include "ext/vector_float4.hpp"
#include "fwd.hpp"
#include "gtx/matrix_decompose.hpp"
#include <functional>
#include <glm.hpp>
#include <string>

namespace Bess::Canvas::Components {
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
        uint64_t id = 0;
    };

    class SelectedComponent {
      public:
        SelectedComponent() = default;
        SelectedComponent(SelectedComponent &other) = default;
        uint64_t id = 0;
    };

    class SimulationComponent {
      public:
        SimulationComponent() = default;
        SimulationComponent(SimulationComponent &other) = default;
        uint64_t id = 0;
    };

    class RenderComponent {
      public:
        RenderComponent() = default;
        RenderComponent(RenderComponent &other) = default;
        std::function<void(TransformComponent &, SpriteComponent &, TagComponent &)> render = [&](TransformComponent &tc, SpriteComponent &rc, TagComponent &tgc) {};
    };
} // namespace Bess::Canvas::Components
