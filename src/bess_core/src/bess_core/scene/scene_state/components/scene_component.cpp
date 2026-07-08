#include "bess_core/scene/scene_state/components/scene_component.h"
#include "json/value.h"

#include "bess_core/renderer/renderer_2d.h"
#include "bess_core/scene/scene_draw_context.h"
#include "bess_core/scene/scene_state/components/scene_component_types.h"
#include "bess_core/scene/scene_state/components/styles/comp_style.h"
#include "bess_core/scene/scene_state/scene_state.h"
#include "ext/matrix_transform.hpp"
#include "ui/icons/FontAwesomeIcons_Remapped.h"

namespace Icons = Bess::UI::Icons;

namespace Bess::Canvas {
    SceneComponent::SceneComponent()
        : m_uuid{UUID()},
          m_icon(Icons::FontAwesomeIcons::FA_CUBE) {
    }

    bool SceneComponent::isDraggable() const {
        return m_isDraggable;
    }

    void SceneComponent::setPosition(const glm::vec3 &pos) {
        m_transform.position = pos;
        onTransformChanged();
    }

    void SceneComponent::setScale(const glm::vec2 &scale) {
        m_transform.scale = scale;
        onScaleChanged();
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

    glm::vec2 SceneComponent::calculateScale(const SceneState &_) {
        const auto labelSize = Core::Renderer::IRenderer2D::getTextRenderSize(
            m_name, {.fontSize = Styles::componentStyles.headerFontSize});
        float width = labelSize.x + (Styles::componentStyles.paddingX * 2.f);
        return {width, Styles::componentStyles.headerHeight};
    }

    void SceneComponent::onFirstDraw(SceneDrawContext &context) {
        setScale(calculateScale(*context.sceneState));
        m_isFirstDraw = false;
    }

    void SceneComponent::prepareUI(SceneUIPrepareCtx &ctx) {
        m_isUIDirty = false;
    }

    void SceneComponent::draw(SceneDrawContext &context) {
        if (m_isFirstDraw) {
            onFirstDraw(context);
        }

        for (const auto &childId : m_childComponents) {
            auto child = context.sceneState->getComponentByUuid(childId);
            child->draw(context);
        }
    }

    void SceneComponent::drawSchematic(SceneDrawContext &context) {
        if (m_isFirstSchematicDraw) {
            onFirstSchematicDraw(context);
        }

        for (const auto &childId : m_childComponents) {
            auto child = context.sceneState->getComponentByUuid(childId);
            child->drawSchematic(context);
        }
    }

    void SceneComponent::addChildComponent(const UUID &uuid) {
        if (m_childComponents.contains(uuid))
            return;
        m_childComponents.insert(uuid);
        onChildrenChanged();
    }

    glm::vec3 SceneComponent::getAbsolutePosition(const SceneState &state,
                                                  bool isSchematicMode) const {
        if (m_parentComponent == UUID::null) {
            return m_transform.position;
        }

        auto parentComp = state.getComponentByUuid(m_parentComponent);
        if (!parentComp) {
            return m_transform.position;
        }

        return parentComp->getAbsolutePosition(state, isSchematicMode) +
               m_transform.position;
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

    void SceneComponent::onFirstSchematicDraw(SceneDrawContext &context) {

        m_isFirstSchematicDraw = false;
    }

    void SceneComponent::removeChildComponent(const UUID &uuid) {
        m_childComponents.erase(uuid);
        onChildrenChanged();
    }

    void SceneComponent::onAttach(SceneState &state) {
    }

    Json::Value SceneComponent::toJson() const {
        auto json = SERIALIZE_PROPS(
            ("uuid", getUuid, setUuid),
            ("transform", getTransform, setTransform),
            ("style", getStyle, setStyle),
            ("name", getName, setName),
            ("parentComponent", getParentComponent, setParentComponent),
            ("childComponents", getChildComponents, setChildComponents),
            ("typeName", getTypeName));

        return json;
    }

    void SceneComponent::fromJson(const Json::Value &j,
                                  const std::shared_ptr<SceneComponent> &ptr) {
        DESERIALIZE_PROPS(
            ptr,
            ("uuid", getUuid, setUuid),
            ("transform", getTransform, setTransform),
            ("style", getStyle, setStyle),
            ("name", getName, setName),
            ("parentComponent", getParentComponent, setParentComponent),
            ("childComponents", getChildComponents, setChildComponents));
    }

    std::vector<UUID>
    SceneComponent::getDependants(const SceneState &state) const {
        auto deps = std::vector<UUID>{};
        for (const auto &childId : m_childComponents) {
            const auto &childComp = state.getComponentByUuid(childId);
            const auto &childDeps = childComp->getDependants(state);
            deps.insert(deps.end(), childDeps.begin(), childDeps.end());
            deps.push_back(childId);
        }
        return deps;
    }

    void SceneComponent::onChildrenChanged() {
    }

    void SceneComponent::onScaleChanged() {
    }

    void SceneComponent::drawPropertiesUI(SceneState &sceneState) {
    }

    std::vector<std::shared_ptr<SceneComponent>>
    SceneComponent::clone(const SceneState &sceneState) const {
        (void)sceneState;
        auto clonedComponent = std::make_shared<SceneComponent>(*this);
        prepareClone(*clonedComponent);
        return {clonedComponent};
    }

    void SceneComponent::prepareClone(SceneComponent &clonedComponent) const {
        clonedComponent.setUuid(UUID{});
        clonedComponent.setRuntimeId(PickingId::invalidRuntimeId);
        clonedComponent.setParentComponent(UUID::null);
        clonedComponent.setChildComponents({});
        clonedComponent.setIsSelected(false);
        clonedComponent.setUIDirty(true);
        clonedComponent.m_isFirstDraw = true;
        clonedComponent.m_isFirstSchematicDraw = true;
        clonedComponent.resetCloneRuntimeState();
    }

    void SceneComponent::resetCloneRuntimeState() {
    }

} // namespace Bess::Canvas
