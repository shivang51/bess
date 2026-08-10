#include "bess_core/scene/scene_ui/controls/container_comp.h"
#include "bess_core/renderer/renderer_2d.h"
#include "bess_core/scene/scene_state/scene_state.h"
#include "common/logger.h"

namespace Bess::Canvas::UI {
    std::shared_ptr<ContainerComp>
    ContainerComp::create(const CompConfig &config) {
        return create(LayoutDirection::horizontal, config);
    }

    std::shared_ptr<ContainerComp>
    ContainerComp::create(const LayoutDirection &direction,
                          const CompConfig &config) {
        auto container = std::make_shared<ContainerComp>();
        container->setDirection(direction);
        applyCompConfig(container, config);
        return container;
    }

    void ContainerComp::onDraw(SceneDrawContext &state) {
        if (m_drawBg) {
            if (m_drawCallback) {
                m_drawCallback(state, this);
            } else {
                drawBackground(state);
            }
        }

        drawChildren(state);
    }

    void ContainerComp::prepStyle(
        const std::shared_ptr<Core::Style::BessTheme> &theme) {
        UISceneComponent::prepStyle(theme);

        if (m_customStyle.direction.has_value()) {
            m_direction = m_customStyle.direction.value();
        }

        if (m_customStyle.mainAxisAlignment.has_value()) {
            m_mainAxisAlignment = m_customStyle.mainAxisAlignment.value();
        }

        if (m_customStyle.crossAxisAlignment.has_value()) {
            m_crossAxisAlignment = m_customStyle.crossAxisAlignment.value();
        }

        if (m_customStyle.drawBg.has_value()) {
            m_drawBg = m_customStyle.drawBg.value();
        }
    }

    void ContainerComp::prepareUI(SceneUIPrepareCtx &state) {
        prepStyle(state.theme);
        initNode(state.sceneState->getUINodeRegistry());

        m_node->setDirection(m_direction);
        m_node->setMainAxisAlignment(m_mainAxisAlignment);
        m_node->setCrossAxisAlignment(m_crossAxisAlignment);
        applyCustomLayoutStyle();

        if (state.parentNode != nullptr) {
            state.parentNode->addChild(m_node);
        }

        prepChildren(state);
        m_isUIDirty = false;
    }

    void ContainerComp::drawBackground(SceneDrawContext &state) {
        if (m_node == nullptr || state.renderer == nullptr) {
            return;
        }

        Core::Renderer::QuadProps quadProps;
        quadProps.position = m_node->getDrawPos();
        quadProps.size = m_node->getDrawSize();
        quadProps.zIndex = m_node->getDrawPos().z;
        quadProps.color = m_style.backgroundColor;
        quadProps.borderColor = m_style.borderColor;
        quadProps.thickness = m_style.metrics.borderSize.toVec4();
        quadProps.radius = m_style.metrics.borderRadius;
        quadProps.id = PickingId::invalid();
        quadProps.transformMode = state.transformMode;
        quadProps.shadow = m_style.shadowProps;

        state.renderer->drawQuad(quadProps);
    }

    void ContainerComp::prepChildren(SceneUIPrepareCtx &state) {
        auto prevParent = state.parentNode;
        state.parentNode = m_node;
        for (const auto &childId : m_childComponents) {
            auto childComp = state.sceneState->getComponentByUuid(childId);
            if (childComp == nullptr) {
                BESS_WARN("Child component with UUID {} not found in "
                          "scene state.",
                          (uint64_t)childId);
                continue;
            }
            childComp->prepareUI(state);
        }
        state.parentNode = prevParent;
    }

    void ContainerComp::drawChildren(SceneDrawContext &state) {
        for (const auto &childId : m_childComponents) {
            auto childComp = state.sceneState->getComponentByUuid(childId);
            if (childComp != nullptr) {
                childComp->draw(state);
            }
        }
    }
} // namespace Bess::Canvas::UI
