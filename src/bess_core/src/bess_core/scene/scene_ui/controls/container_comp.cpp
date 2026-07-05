#include "bess_core/scene/scene_ui/controls/container_comp.h"
#include "bess_core/scene/scene_state/scene_state.h"
#include "common/logger.h"

namespace Bess::Canvas::UI {
    std::shared_ptr<ContainerComp>
    ContainerComp::create(const LayoutDirection &direction) {
        auto container = std::make_shared<ContainerComp>();
        container->setDirection(direction);
        return container;
    }

    void ContainerComp::draw(SceneDrawContext &state) {
        if (m_drawBg) {
            drawBgQuad(state);
        }

        drawChildren(state);
    }

    void ContainerComp::prepareUI(SceneUIPrepareCtx &state) {
        initNode(state.sceneState->getUINodeRegistry());
        prepStyle(state.theme);

        m_node->setDirection(m_direction);
        m_node->setMainAxisAlignment(m_mainAxisAlignment);
        m_node->setCrossAxisAlignment(m_crossAxisAlignment);

        if (state.parentNode != nullptr) {
            state.parentNode->addChild(m_node);
        }

        prepChildren(state);
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
