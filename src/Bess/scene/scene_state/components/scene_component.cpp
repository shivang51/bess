#include "scene/scene_state/components/scene_component.h"

#include <algorithm>
#include <utility>

#include "scene/scene_state/components/styles/comp_style.h"
#include "scene/scene_state/components/styles/sim_comp_style.h"
#include "scene/scene_state/components/types.h"
#include "settings/viewport_theme.h"
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

    void SceneComponent::onFirstDraw(SceneState &_,
                                     std::shared_ptr<Renderer::MaterialRenderer> materialRenderer,
                                     std::shared_ptr<PathRenderer> /*unused*/) {
        setScale(calculateScale(std::move(materialRenderer)));
        m_isFirstDraw = false;
    }

    void SceneComponent::draw(SceneState &state,
                              std::shared_ptr<Renderer::MaterialRenderer> materialRenderer,
                              std::shared_ptr<PathRenderer> pathRenderer) {
        if (m_isFirstDraw) {
            onFirstDraw(state, std::move(materialRenderer), std::move(pathRenderer));
        }
    }

    void SceneComponent::drawSchematic(SceneState &state,
                                       std::shared_ptr<Renderer::MaterialRenderer> materialRenderer,
                                       std::shared_ptr<PathRenderer> pathRenderer) {
        if (m_isFirstSchematicDraw) {
            onFirstSchematicDraw(state, materialRenderer, pathRenderer);
        }
    }

    void SceneComponent::addChildComponent(const UUID &uuid) {
        m_childComponents.emplace_back(uuid);
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
        for (const auto &childUuid : m_childComponents) {
            auto childComp = state.getComponentByUuid(childUuid);
            if (childComp) {
                state.removeComponent(childUuid, m_uuid);
            }
        }
        return m_childComponents;
    }

    void SceneComponent::onFirstSchematicDraw(SceneState &state,
                                              std::shared_ptr<Renderer::MaterialRenderer> materialRenderer,
                                              std::shared_ptr<PathRenderer> pathRenderer) {

        m_isFirstSchematicDraw = false;
        if (m_isFirstDraw) {
            onFirstDraw(state, std::move(materialRenderer), std::move(pathRenderer));
        }
    }
} // namespace Bess::Canvas

namespace Bess::JsonConvert {
    void toJsonValue(const Bess::Canvas::SceneComponent &component, Json::Value &j) {
        toJsonValue(component.getTransform(), j["transform"]);
        toJsonValue(component.getStyle(), j["style"]);
        toJsonValue(component.getUuid(), j["uuid"]);
        toJsonValue(component.getParentComponent(), j["parentComponent"]);
        j["name"] = component.getName();
        j["childComponents"] = Json::Value(Json::arrayValue);
        j["type"] = static_cast<int>(component.getType());
        for (const auto &childUuid : component.getChildComponents()) {
            Json::Value childJson;
            toJsonValue(childUuid, childJson);
            j["childComponents"].append(childJson);
        }
    }

    void fromJsonValue(const Json::Value &j, Bess::Canvas::SceneComponent &component) {
        if (j.isMember("transform")) {
            fromJsonValue(j["transform"], component.getTransform());
        }

        if (j.isMember("style")) {
            fromJsonValue(j["style"], component.getStyle());
        }

        if (j.isMember("uuid")) {
            UUID uuid;
            fromJsonValue(j["uuid"], uuid);
            component.setUuid(uuid);
        }

        if (j.isMember("parentComponent")) {
            UUID parentUuid;
            fromJsonValue(j["parentComponent"], parentUuid);
            component.setParentComponent(parentUuid);
        }

        if (j.isMember("name")) {
            component.setName(j["name"].asString());
        }

        if (j.isMember("childComponents") && j["childComponents"].isArray()) {
            std::vector<UUID> childUuids;
            for (const auto &childJson : j["childComponents"]) {
                UUID childUuid;
                fromJsonValue(childJson, childUuid);
                childUuids.push_back(childUuid);
            }
            component.setChildComponents(childUuids);
        }
    }
} // namespace Bess::JsonConvert
