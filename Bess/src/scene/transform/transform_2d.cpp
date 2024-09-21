#include "scene/transform/transform_2d.h"
#include "ext/matrix_transform.hpp"

namespace Bess::Scene::Transform {
    Transform2D::Transform2D(const glm::vec3 &position, const glm::vec2 &scale, float rotation)
        : m_position(position), m_scale(scale), m_rotation(rotation) {}

    void Transform2D::setPosition(const glm::vec3 &position) {
        m_position = position;
        updateTransform();
    }

    void Transform2D::setScale(const glm::vec2 &scale) {
        m_scale = scale;
        updateTransform();
    }

    void Transform2D::setRotation(float rotation) {
        m_rotation = rotation;
        updateTransform();
    }

    const glm::vec3 &Transform2D::getPosition() const { return m_position; }

    const glm::vec2 &Transform2D::getScale() const { return m_scale; }

    float Transform2D::getRotation() const { return m_rotation; }

    const glm::mat4 &Transform2D::getTransform() const { return m_transform; }

    void Transform2D::updateTransform() {
        m_transform = glm::translate(glm::mat4(1.0f), m_position);
        m_transform = glm::rotate(m_transform, glm::radians(m_rotation), glm::vec3(0, 0, 1));
        m_transform = glm::scale(m_transform, glm::vec3(m_scale, 1));
    }

} // namespace Bess::Scene::Transform
