#include "scene/scene_state/components/conn_joint_scene_component.h"
#include "scene/scene_state/components/connection_scene_component.h"
#include "scene/scene_state/components/scene_component_types.h"
#include "scene/scene_state/scene_state.h"
#include "settings/viewport_theme.h"
#include "ui/ui.h"

namespace Bess::Canvas {
    ConnJointSceneComp::ConnJointSceneComp(UUID connectionId,
                                           int connSegIdx,
                                           ConnSegOrientaion segOrientation)
        : m_connSegIdx(connSegIdx), m_connectionId(connectionId), m_segOrientation(segOrientation) {
    }

    void ConnJointSceneComp::draw(SceneState &state,
                                  std::shared_ptr<Renderer::MaterialRenderer> materialRenderer,
                                  std::shared_ptr<Renderer2D::Vulkan::PathRenderer> pathRenderer) {

        const auto &conn = state.getComponentByUuid<ConnectionSceneComponent>(m_connectionId);
        const glm::vec3 &segStartPos = conn->getSegVertexPos(state, m_connSegIdx);
        const glm::vec3 &segEndPos = conn->getSegVertexPos(state, m_connSegIdx + 1);
        m_segLen = glm::length(segEndPos - segStartPos);

        const auto pickingId = PickingId{m_runtimeId, 0};
        materialRenderer->drawCircle(getAbsolutePosition(state),
                                     4.f,
                                     ViewportTheme::colors.selectedComp,
                                     pickingId);
    }

    void ConnJointSceneComp::drawSchematic(SceneState &state,
                                           std::shared_ptr<Renderer::MaterialRenderer> materialRenderer,
                                           std::shared_ptr<Renderer2D::Vulkan::PathRenderer> pathRenderer) {
        const auto pickingId = PickingId{m_runtimeId, 0};
        materialRenderer->drawCircle(getAbsolutePosition(state),
                                     4.f,
                                     ViewportTheme::colors.selectedComp,
                                     pickingId);
    }

    void ConnJointSceneComp::onMouseEnter(const Events::MouseEnterEvent &e) {
        UI::setCursorPointer();
    }

    void ConnJointSceneComp::onMouseLeave(const Events::MouseLeaveEvent &e) {
        UI::setCursorNormal();
    }

    glm::vec3 ConnJointSceneComp::getAbsolutePosition(const SceneState &state) const {
        const auto &conn = state.getComponentByUuid<ConnectionSceneComponent>(m_connectionId);

        const glm::vec3 &segStartPos = conn->getSegVertexPos(state, m_connSegIdx);
        const glm::vec3 &segEndPos = conn->getSegVertexPos(state, m_connSegIdx + 1);

        auto pos = glm::mix(segStartPos, segEndPos, m_segOffset);
        pos.z += 0.0001f;

        return pos;
    }

    void ConnJointSceneComp::onMouseDragged(const Events::MouseDraggedEvent &e) {
        if (!m_isDragging) {
            onMouseDragBegin(e);
        }

        float delta = m_segOrientation == ConnSegOrientaion::horizontal
                          ? e.delta.x
                          : -e.delta.y;

        m_segOffset += delta / m_segLen;

        m_segOffset = glm::clamp(m_segOffset, 0.0f, 1.0f);
    }

    void ConnJointSceneComp::onMouseButton(const Events::MouseButtonEvent &e) {
        Events::ConnJointClickEvent event{m_uuid, e.button, e.action};
        EventSystem::EventDispatcher::instance().dispatch(event);
    }

    SimEngine::SlotState ConnJointSceneComp::getSlotState(const SceneState &state) const {
        const auto &slotComp = state.getComponentByUuid<SlotSceneComponent>(m_outputSlotId);
        return slotComp->getSlotState(state);
    }
    void ConnJointSceneComp::addConnection(const UUID &connectionId) {
        m_connections.emplace_back(connectionId);
    }
} // namespace Bess::Canvas
