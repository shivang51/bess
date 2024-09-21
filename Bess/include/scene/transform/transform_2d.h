#pragma once

#include "glm.hpp"

namespace Bess::Scene::Transform {
    class Transform2D {
      public:
        Transform2D() = default;
        Transform2D(const glm::vec3 &position, const glm::vec2 &scale, float rotation);

        void setPosition(const glm::vec3 &position);
        void setScale(const glm::vec2 &scale);
        void setRotation(float rotation);

        const glm::vec3 &getPosition() const;
        const glm::vec2 &getScale() const;
        float getRotation() const;

        const glm::mat4 &getTransform() const;

      protected:
        void updateTransform();

      protected:
        glm::vec3 m_position = {0, 0, 0};
        glm::vec2 m_scale = {1, 1};
        float m_rotation = 0;

        glm::mat4 m_transform = glm::mat4(1.0f);
    };
} // namespace Bess::Scene::Transform
