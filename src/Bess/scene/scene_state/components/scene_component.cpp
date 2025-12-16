#include "scene/scene_state/components/scene_component.h"

#include <utility>

#include "scene/scene_state/components/styles/comp_style.h"
#include "scene/scene_state/components/styles/sim_comp_style.h"
#include <utility>

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

    glm::vec2 SceneComponent::calculateScale(std::shared_ptr<Renderer::MaterialRenderer> materialRenderer) {
        const auto labelSize = materialRenderer->getTextRenderSize(m_name, Styles::componentStyles.headerFontSize);
        float width = labelSize.x + (componentStyles.paddingX * 2.f);
        return {width, Styles::componentStyles.headerHeight};
    }

    void SceneComponent::onFirstDraw(SceneState &,
                                     std::shared_ptr<Renderer::MaterialRenderer> materialRenderer,
                                     std::shared_ptr<PathRenderer> /*unused*/) {
        setScale(calculateScale(materialRenderer));
        m_isFirstDraw = false;
    }

    void SceneComponent::draw(SceneState &state,
                              std::shared_ptr<Renderer::MaterialRenderer> materialRenderer,
                              std::shared_ptr<PathRenderer> pathRenderer) {
        if (m_isFirstDraw) {
            onFirstDraw(state, std::move(materialRenderer), std::move(pathRenderer));
        }
    }

    void SceneComponent::addChildComponent(const UUID &uuid) {
        m_childComponents.emplace_back(uuid);
    }

} // namespace Bess::Canvas
