#include "scene/scene_state/components/scene_component.h"
#include "json/value.h"
#include <utility>

#include "ext/matrix_transform.hpp"
#include "scene/scene_state/components/scene_component_types.h"
#include "scene/scene_state/components/styles/comp_style.h"
#include "scene/scene_state/scene_state.h"

namespace Bess::Canvas {
    SceneComponent::SceneComponent() : m_uuid{UUID()} {};

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

    void SceneComponent::setIsDraggable(bool draggable) {
        m_isDraggable = draggable;
    }

    glm::mat4 Transform::getTransform() const {
        auto transform = glm::translate(glm::mat4(1), position);
        transform = glm::rotate(transform, angle, {0.f, 0.f, 1.f});
        transform = glm::scale(transform, glm::vec3(scale, 1.f));
        return transform;
    }

    glm::vec2 SceneComponent::calculateScale(SceneState &_, std::shared_ptr<Renderer::MaterialRenderer> materialRenderer) {
        const auto labelSize = materialRenderer->getTextRenderSize(m_name, Styles::componentStyles.headerFontSize);
        float width = labelSize.x + (Styles::componentStyles.paddingX * 2.f);
        return {width, Styles::componentStyles.headerHeight};
    }

    void SceneComponent::onFirstDraw(SceneState &_,
                                     std::shared_ptr<Renderer::MaterialRenderer> materialRenderer,
                                     std::shared_ptr<PathRenderer> /*unused*/) {
        setScale(calculateScale(_, std::move(materialRenderer)));
        m_isFirstDraw = false;
    }

    void SceneComponent::draw(SceneState &state,
                              std::shared_ptr<Renderer::MaterialRenderer> materialRenderer,
                              std::shared_ptr<PathRenderer> pathRenderer) {
        if (m_isFirstDraw) {
            onFirstDraw(state, materialRenderer, pathRenderer);
        }

        for (const auto &childId : m_childComponents) {
            auto child = state.getComponentByUuid(childId);
            child->draw(state, materialRenderer, pathRenderer);
        }
    }

    void SceneComponent::drawSchematic(SceneState &state,
                                       std::shared_ptr<Renderer::MaterialRenderer> materialRenderer,
                                       std::shared_ptr<PathRenderer> pathRenderer) {
        if (m_isFirstSchematicDraw) {
            onFirstSchematicDraw(state, materialRenderer, pathRenderer);
        }

        for (const auto &childId : m_childComponents) {
            auto child = state.getComponentByUuid(childId);
            child->drawSchematic(state, materialRenderer, pathRenderer);
        }
    }

    void SceneComponent::addChildComponent(const UUID &uuid) {
        if (m_childComponents.contains(uuid))
            return;
        m_childComponents.insert(uuid);
    }

    glm::vec3 SceneComponent::getAbsolutePosition(const SceneState &state) const {
        if (m_parentComponent == UUID::null) {
            return m_transform.position;
        }

        auto parentComp = state.getComponentByUuid(m_parentComponent);
        if (!parentComp) {
            return m_transform.position;
        }

        return parentComp->getAbsolutePosition(state) + m_transform.position;
    }

    std::vector<UUID> SceneComponent::cleanup(SceneState &state, UUID caller) {
        auto removedIds = std::vector<UUID>{};
        for (const auto &childUuid : m_childComponents) {
            auto childComp = state.getComponentByUuid(childUuid);
            if (childComp) {
                auto ids = state.removeComponent(childUuid, m_uuid);
                removedIds.insert(removedIds.end(), ids.begin(), ids.end());
            }
        }
        return removedIds;
    }

    void SceneComponent::onFirstSchematicDraw(SceneState &state,
                                              std::shared_ptr<Renderer::MaterialRenderer> materialRenderer,
                                              std::shared_ptr<PathRenderer> pathRenderer) {

        m_isFirstSchematicDraw = false;
    }

    void SceneComponent::removeChildComponent(const UUID &uuid) {
        m_childComponents.erase(uuid);
    }

    void SceneComponent::onAttach(SceneState &state) {}

    Json::Value SceneComponent::toJson() const {
        auto json = SERIALIZE_PROPS(("uuid", getUuid, setUuid),
                                    ("transform", getTransform, setTransform),
                                    ("style", getStyle, setStyle),
                                    ("name", getName, setName),
                                    ("parentComponent", getParentComponent, setParentComponent),
                                    ("childComponents", getChildComponents, setChildComponents));

        return json;
    }

    void SceneComponent::fromJson(const Json::Value &j,
                                  const std::shared_ptr<SceneComponent> &ptr) {
        DESERIALIZE_PROPS(ptr, ("uuid", getUuid, setUuid),
                          ("transform", getTransform, setTransform),
                          ("style", getStyle, setStyle),
                          ("name", getName, setName),
                          ("parentComponent", getParentComponent, setParentComponent),
                          ("childComponents", getChildComponents, setChildComponents));
    }

    std::vector<UUID> SceneComponent::getDependants(SceneState &state) const {
        return m_childComponents.empty()
                   ? std::vector<UUID>{}
                   : std::vector<UUID>(m_childComponents.begin(), m_childComponents.end());
    }
} // namespace Bess::Canvas
