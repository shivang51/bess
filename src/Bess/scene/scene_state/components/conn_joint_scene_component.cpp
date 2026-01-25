#include "scene/scene_state/components/conn_joint_scene_component.h"
#include "commands/commands.h"
#include "scene/scene_state/components/connection_scene_component.h"
#include "scene/scene_state/components/scene_component_types.h"
#include "scene/scene_state/components/styles/sim_comp_style.h"
#include "scene/scene_state/scene_state.h"
#include "settings/viewport_theme.h"
#include "simulation_engine.h"
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

        glm::vec4 color;
        if (m_isSelected) {
            color = ViewportTheme::colors.selectedComp;
        } else {
            color = ViewportTheme::schematicViewColors.connection;
        }

        materialRenderer->drawCircle(getAbsolutePosition(state),
                                     Styles::compSchematicStyles.connJointRadius,
                                     color,
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
        if (e.action == Events::MouseClickAction::press) {
            if (e.button == Events::MouseButton::left) {
                onMouseLeftClick(e);
            }
        }
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

    void ConnJointSceneComp::onMouseLeftClick(const Events::MouseButtonEvent &e) {
        auto &connStartSlot = e.sceneState->getConnectionStartSlot();

        if (connStartSlot == UUID::null) {
            connStartSlot = m_uuid;
            return;
        }

        auto startSlot = e.sceneState->getComponentByUuid<
            SlotSceneComponent>(connStartSlot);

        if (startSlot->getType() != SceneComponentType::slot) {
            BESS_WARN("[Scene] Two joints can't be connected directly");
            connStartSlot = UUID::null;
            return;
        }

        auto endComp = e.sceneState->getComponentByUuid<ConnJointSceneComp>(m_uuid);
        auto endSlot = e.sceneState->getComponentByUuid<
            SlotSceneComponent>(endComp->getOutputSlotId());

        const auto startParent = e.sceneState->getComponentByUuid<
            SimulationSceneComponent>(startSlot->getParentComponent());
        const auto endParent = e.sceneState->getComponentByUuid<
            SimulationSceneComponent>(endSlot->getParentComponent());

        const auto startPinType = startSlot->getSlotType() == SlotType::digitalInput
                                      ? SimEngine::SlotType::digitalInput
                                      : SimEngine::SlotType::digitalOutput;

        const auto endPinType = endSlot->getSlotType() == SlotType::digitalInput
                                    ? SimEngine::SlotType::digitalInput
                                    : SimEngine::SlotType::digitalOutput;

        auto &cmdMngr = SimEngine::SimulationEngine::instance().getCmdManager();
        const auto res = cmdMngr.execute<SimEngine::Commands::ConnectCommand,
                                         std::string>(startParent->getSimEngineId(),
                                                      startSlot->getIndex(),
                                                      startPinType,
                                                      endParent->getSimEngineId(),
                                                      endSlot->getIndex(),
                                                      endPinType);

        if (!res.has_value()) {
            BESS_WARN("[Scene] Failed to create connection between slots, {}",
                      res.error());
            connStartSlot = UUID::null;
            return;
        }

        auto conn = std::make_shared<ConnectionSceneComponent>();
        e.sceneState->addComponent<ConnectionSceneComponent>(conn);

        conn->setStartEndSlots(startSlot->getUuid(), endComp->getUuid());
        startSlot->addConnection(conn->getUuid());
        endComp->addConnection(conn->getUuid());

        BESS_INFO("[Scene] Created connection between slots {} and {}",
                  (uint64_t)startSlot->getUuid(),
                  (uint64_t)endComp->getUuid());

        connStartSlot = UUID::null;
    }
} // namespace Bess::Canvas
