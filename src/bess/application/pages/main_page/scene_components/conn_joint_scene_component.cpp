#include "conn_joint_scene_component.h"
#include "common/bess_uuid.h"
#include "connection_scene_component.h"
#include "geometric.hpp"
#include "pages/main_page/cmds/add_comp_cmd.h"
#include "pages/main_page/main_page.h"
#include "scene/scene_state/components/scene_component_types.h"
#include "scene/scene_state/components/styles/sim_comp_style.h"
#include "scene/scene_state/scene_state.h"
#include "settings/viewport_theme.h"
#include "sim_scene_component.h"
#include "slot_scene_component.h"
#include "types.h"
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

        if (m_isFirstDraw) {
            m_isFirstDraw = false;
            if (m_offset < 0.f) {
                m_offset = 0.5f;
            }
        }

        const auto &conn = state.getComponentByUuid<ConnectionSceneComponent>(m_connectionId);
        const auto &slot = state.getComponentByUuid<SlotSceneComponent>(m_outputSlotId);

        auto color = ViewportTheme::colors.stateLow;

        if (m_isSelected) {
            color = ViewportTheme::colors.selectedComp;
        } else if (slot->getSlotState(state).state == SimEngine::LogicState::high) {
            color = ViewportTheme::colors.stateHigh;
        }

        float radius = 3.f;

        if (m_isHovered) {
            radius = 4.f;
        }

        const auto pickingId = PickingId{m_runtimeId, 0};
        materialRenderer->drawCircle(getAbsolutePosition(state),
                                     radius,
                                     color,
                                     pickingId);
    }

    void ConnJointSceneComp::drawSchematic(SceneState &state,
                                           std::shared_ptr<Renderer::MaterialRenderer> materialRenderer,
                                           std::shared_ptr<Renderer2D::Vulkan::PathRenderer> pathRenderer) {

        if (m_isFirstSchematicDraw) {
            if (m_schematicOffset < 0.f)
                m_schematicOffset = m_offset >= 0.f ? m_offset : 0.5f;
            m_isFirstSchematicDraw = false;
        }

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
        m_isHovered = true;
    }

    void ConnJointSceneComp::onMouseLeave(const Events::MouseLeaveEvent &e) {
        UI::setCursorNormal();
        m_isHovered = false;
    }

    glm::vec3 ConnJointSceneComp::getAbsolutePosition(const SceneState &state) const {
        const auto &conn = state.getComponentByUuid<ConnectionSceneComponent>(m_connectionId);

        const glm::vec3 &segStartPos = conn->getSegVertexPos(state, m_connSegIdx);
        const glm::vec3 &segEndPos = conn->getSegVertexPos(state, m_connSegIdx + 1);

        const float offset = state.getIsSchematicView()
                                 ? m_schematicOffset
                                 : m_offset;
        auto pos = glm::mix(segStartPos, segEndPos, offset);
        pos.z += 0.0001f;

        return pos;
    }

    void ConnJointSceneComp::onMouseDragged(const Events::MouseDraggedEvent &e) {
        if (e.isMultiDrag)
            return;

        if (!m_isDragging) {
            onMouseDragBegin(e);
        }

        float delta = m_segOrientation == ConnSegOrientaion::horizontal
                          ? e.delta.x
                          : e.delta.y;

        const auto &conn = e.sceneState->getComponentByUuid<ConnectionSceneComponent>(m_connectionId);
        const auto &slot = e.sceneState->getComponentByUuid<SlotSceneComponent>(m_outputSlotId);
        const glm::vec3 &segStartPos = conn->getSegVertexPos(*e.sceneState, m_connSegIdx);
        const glm::vec3 &segEndPos = conn->getSegVertexPos(*e.sceneState, m_connSegIdx + 1);
        const auto &segLen = glm::distance(segEndPos, segStartPos);

        float startCoord = (m_segOrientation == ConnSegOrientaion::horizontal)
                               ? segStartPos.x
                               : segStartPos.y;
        float endCoord = (m_segOrientation == ConnSegOrientaion::horizontal)
                             ? segEndPos.x
                             : segEndPos.y;

        if (endCoord >= startCoord) {
            delta = delta / segLen;
        } else {
            delta = -delta / segLen;
        }

        if (e.sceneState->getIsSchematicView()) {
            m_schematicOffset += delta;
            m_schematicOffset = glm::clamp(m_schematicOffset, 0.0f, 1.0f);
        } else {
            m_offset += delta;
            m_offset = glm::clamp(m_offset, 0.0f, 1.0f);
        }
    }

    void ConnJointSceneComp::onMouseButton(const Events::MouseButtonEvent &e) {
        if (e.action == Events::MouseClickAction::press) {
            if (e.button == Events::MouseButton::left) {
                onMouseLeftClick(e);
            }
        }
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

        connectWith(*e.sceneState, connStartSlot);
        connStartSlot = UUID::null;
    }

    bool ConnJointSceneComp::connectWith(SceneState &sceneState, const UUID &slotId) {
        auto startSlot = sceneState.getComponentByUuid<SlotSceneComponent>(slotId);

        if (startSlot->getType() != SceneComponentType::slot) {
            BESS_WARN("[Scene] Two joints can't be connected directly");
            return false;
        }

        auto endComp = sceneState.getComponentByUuid<ConnJointSceneComp>(m_uuid);
        auto endSlot = sceneState.getComponentByUuid<
            SlotSceneComponent>(endComp->getOutputSlotId());

        const auto startParent = sceneState.getComponentByUuid<
            SimulationSceneComponent>(startSlot->getParentComponent());
        const auto endParent = sceneState.getComponentByUuid<
            SimulationSceneComponent>(endSlot->getParentComponent());

        const auto startPinType = startSlot->getSlotType() == SlotType::digitalInput
                                      ? SimEngine::SlotType::digitalInput
                                      : SimEngine::SlotType::digitalOutput;

        const auto endPinType = endSlot->getSlotType() == SlotType::digitalInput
                                    ? SimEngine::SlotType::digitalInput
                                    : SimEngine::SlotType::digitalOutput;

        auto conn = std::make_shared<ConnectionSceneComponent>();
        conn->setInitialSegmentCount(2);
        conn->setStartEndSlots(startSlot->getUuid(), endComp->getUuid());
        startSlot->addConnection(conn->getUuid());
        endComp->addConnection(conn->getUuid());

        auto &cmdManager = Pages::MainPage::getInstance()->getState().getCommandSystem();
        cmdManager.execute(std::make_unique<Cmd::AddCompCmd<ConnectionSceneComponent>>(conn));

        BESS_INFO("[Scene] Created connection {} between slots {} and {}",
                  (uint64_t)conn->getUuid(),
                  (uint64_t)startSlot->getUuid(),
                  (uint64_t)endComp->getUuid());

        return true;
    }

    void ConnJointSceneComp::removeConnection(const UUID &connectionId) {
        m_connections.erase(std::ranges::remove(m_connections,
                                                connectionId)
                                .begin(),
                            m_connections.end());
    }

    std::vector<UUID> ConnJointSceneComp::cleanup(SceneState &state, UUID caller) {
        auto ids = SceneComponent::cleanup(state, caller);

        for (const auto &connId : m_connections) {
            state.removeComponent(connId, m_uuid);
            ids.emplace_back(connId);
        }

        if (caller != m_connectionId) {
            auto connComp = state.getComponentByUuid<ConnectionSceneComponent>(m_connectionId);
            if (connComp) {
                connComp->removeAssociatedJoint(m_uuid);
            }
        }

        return ids;
    }

    std::vector<UUID> ConnJointSceneComp::getDependants(const SceneState &state) const {
        std::vector<UUID> dependants;

        for (const auto &connId : m_connections) {
            const auto &connComp = state.getComponentByUuid<ConnectionSceneComponent>(connId);
            const auto &ids = connComp->getDependants(state);
            dependants.insert(dependants.end(), ids.begin(), ids.end());
            dependants.emplace_back(connId);
        }

        return dependants;
    }
} // namespace Bess::Canvas
