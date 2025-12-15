#include "scene/scene_state/components/scene_component.h"

namespace Bess::Canvas {
    SceneComponent::SceneComponent() : m_uuid{UUID()} {};

    SceneComponent::SceneComponent(UUID uuid)
        : m_uuid(uuid) {}

    SceneComponent::SceneComponent(UUID uuid, const Transform &transform)
        : m_uuid(uuid), m_transform(transform) {}

    bool SceneComponent::isDraggable() const {
        return m_isDraggable;
    }

    void SceneComponent::setPosition(const glm::vec3 &pos) {
        m_transform.position = pos;
        onTransformChanged();
    }

    void SceneComponent::setScale(const glm::vec2 &scale) {
        m_transform.scale = scale;
    }

    SceneComponentType SceneComponent::getType() const {
        return m_type;
    }

    void SceneComponent::setIsDraggable(bool draggable) {
        m_isDraggable = draggable;
    }

    bool SceneComponent::isSelected() const {
        return m_isSelected;
    }

    void SceneComponent::setIsSelected(bool selected) {
        m_isSelected = selected;
    }

    glm::mat4 Transform::getTransform() const {
        auto transform = glm::translate(glm::mat4(1), position);
        transform = glm::rotate(transform, angle, {0.f, 0.f, 1.f});
        transform = glm::scale(transform, glm::vec3(scale, 1.f));
        return transform;
    }
} // namespace Bess::Canvas
