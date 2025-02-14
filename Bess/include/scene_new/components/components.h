#pragma once

#include <functional>
#include <glm.hpp>

namespace Bess::Canvas::Components {
    class TransformComponent {
      public:
        TransformComponent() = default;
        TransformComponent(TransformComponent &other) = default;

        operator glm::mat4() {
            return m_transform;
        }

      private:
        glm::mat4 m_transform{};
    };

    class RenderComponent {
      public:
        RenderComponent() = default;
        RenderComponent(RenderComponent &other) = default;
        std::function<void()> render = []() {};
    };
} // namespace Bess::Canvas::Components
