#include "conn_joint_scene_component.h"
#include "bess_core/scene/scene_draw_helpers.h"
#include "bess_core/scene/scene_state/components/scene_component_types.h"
#include "bess_core/scene/scene_state/components/styles/sim_comp_style.h"
#include "bess_core/scene/scene_state/scene_state.h"
#include "bess_core/settings/viewport_theme.h"
#include "common/bess_uuid.h"
#include "connection_scene_component.h"
#include "geometric.hpp"
#include "pages/main_page/comp_edit.h"
#include "sim_scene_component.h"
#include "slot_scene_component.h"

#include "ui/ui_main/ui_main.h"
#include <cstdint>
#include <memory>

namespace Bess::Canvas {
    ConnJointSceneComp::ConnJointSceneComp(UUID connectionId,
                                           int connSegIdx,
                                           ConnSegOrientaion segOrientation)
        : m_connSegIdx(connSegIdx),
          m_connectionId(connectionId),
          m_segOrientation(segOrientation) {
#ifdef DEBUG
        m_name = "ConnJointSceneComp";
#endif
    }

    std::vector<std::shared_ptr<SceneComponent>>
    ConnJointSceneComp::clone(const SceneState &sceneState) const {
        (void)sceneState;
        BESS_ASSERT(false,
                    "Cloning ConnJointSceneComp is supported through "
                    "cloneConnJoin function");
        return {};
    }

    std::vector<std::shared_ptr<SceneComponent>>
    ConnJointSceneComp::cloneConnJoint(
        const SceneState &sceneState,
        std::unordered_map<UUID, UUID> &ogToClonedIdMap) {

        std::vector<std::shared_ptr<SceneComponent>> clonedComps;

        BESS_ASSERT(ogToClonedIdMap.contains(m_connectionId),
                    "Connection of joint has no mapping to its clone");

        auto clone = std::make_shared<ConnJointSceneComp>(*this);
        prepareClone(*clone);
        clone->m_connectionId = ogToClonedIdMap.at(m_connectionId);

        if (m_outputSlotId != UUID::null) {
            BESS_ASSERT(
                ogToClonedIdMap.contains(m_outputSlotId),
                "Connection of joint has no mapping to its outputSlotId");
            clone->m_outputSlotId = ogToClonedIdMap.at(m_outputSlotId);
        }

        if (m_inputSlotId != UUID::null) {
            BESS_ASSERT(
                ogToClonedIdMap.contains(m_inputSlotId),
                "Connection of joint has no mapping to its inputSlotId");
            clone->m_inputSlotId = ogToClonedIdMap.at(m_inputSlotId);
        }

        ogToClonedIdMap[m_uuid] = clone->m_uuid;

        clonedComps.push_back(clone);
        clone->m_connections.clear();

        for (const auto &id : m_connections) {
            const auto &conn =
                sceneState.getComponentByUuid<ConnectionSceneComponent>(id);
            if (!conn || !ogToClonedIdMap.contains(conn->getStartSlot()) ||
                !ogToClonedIdMap.contains(conn->getEndSlot())) {
                continue;
            }

            auto clonedConn = conn->cloneConn(sceneState, ogToClonedIdMap);

            clonedComps.insert(
                clonedComps.end(), clonedConn.begin(), clonedConn.end());
            clone->m_connections.push_back(clonedConn.front()->getUuid());
        }

        return clonedComps;
    }

    void ConnJointSceneComp::draw(SceneDrawContext &context) {

        if (m_isFirstDraw) {
            m_isFirstDraw = false;
            if (m_offset < 0.f) {
                m_offset = 0.5f;
            }
        }

        const auto &state = *context.sceneState;

        const auto &conn =
            state.getComponentByUuid<ConnectionSceneComponent>(m_connectionId);
        const auto &slot =
            state.getComponentByUuid<SlotSceneComponent>(m_outputSlotId);

        auto color = ViewportTheme::colors.stateLow,
             borderColor = ViewportTheme::colors.text;

        if (m_isSelected) {
            borderColor = ViewportTheme::colors.selectedComp;
        }

        if (slot->getSlotState(context).getLogicState() ==
            SimEngine::LogicState::high) {
            color = ViewportTheme::colors.stateHigh;
        }

        float sideLength = 6.f;

        if (m_isHovered) {
            sideLength = 8.f;
        }

        const auto pickingId = PickingId{m_runtimeId, 0};
        SceneDraw::drawQuad(context,
                            getAbsolutePosition(state, context.isSchematicMode),
                            glm::vec2{sideLength, sideLength},
                            color,
                            pickingId,
                            {
                                .angle = 45,
                                .borderColor = borderColor,
                                .borderSize = glm::vec4(1.f),
                            });
    }

    void ConnJointSceneComp::drawSchematic(SceneDrawContext &context) {

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

        const auto &state = *context.sceneState;
        SceneDraw::drawCircle(
            context,
            getAbsolutePosition(state, context.isSchematicMode),
            Styles::compSchematicStyles.connJointRadius,
            color,
            pickingId);
    }

    bool ConnJointSceneComp::onMouseEnter(const Events::MouseEnterEvent &e) {
        (void)e;
        m_isHovered = true;
        return true;
    }

    bool ConnJointSceneComp::onMouseLeave(const Events::MouseLeaveEvent &e) {
        (void)e;
        m_isHovered = false;
        return true;
    }

    Core::Viewport::SceneCursor ConnJointSceneComp::getCursor() const {
        return Core::Viewport::SceneCursor::pointer;
    }

    glm::vec3
    ConnJointSceneComp::getAbsolutePosition(const SceneState &state,
                                            bool isSchematicMode) const {
        const auto &conn =
            state.getComponentByUuid<ConnectionSceneComponent>(m_connectionId);

        const glm::vec3 &segStartPos =
            conn->getSegVertexPos(state, m_connSegIdx, isSchematicMode);
        const glm::vec3 &segEndPos =
            conn->getSegVertexPos(state, m_connSegIdx + 1, isSchematicMode);

        const float offset = isSchematicMode ? m_schematicOffset : m_offset;
        auto pos = glm::mix(segStartPos, segEndPos, offset);
        pos.z += 0.0001f;

        return pos;
    }

    void
    ConnJointSceneComp::onMouseDragged(const Events::MouseDraggedEvent &e) {
        if (e.isMultiDrag)
            return;

        if (!m_isDragging) {
            m_dragBefore = toJson();
            m_dragScene =
                e.sceneState ? e.sceneState->getSceneId() : UUID::null;
            onMouseDragBegin(e);
        }

        float delta = m_segOrientation == ConnSegOrientaion::horizontal
                          ? e.delta.x
                          : e.delta.y;

        const auto &conn =
            e.sceneState->getComponentByUuid<ConnectionSceneComponent>(
                m_connectionId);
        const auto &slot = e.sceneState->getComponentByUuid<SlotSceneComponent>(
            m_outputSlotId);

        const bool isSchematic =
            Bess::UI::UIMain::getTargetSceneViewportPanel()->isSchematicMode();

        const glm::vec3 &segStartPos =
            conn->getSegVertexPos(*e.sceneState, m_connSegIdx, isSchematic);

        const glm::vec3 &segEndPos =
            conn->getSegVertexPos(*e.sceneState, m_connSegIdx + 1, isSchematic);
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

        if (isSchematic) {
            m_schematicOffset += delta;
            m_schematicOffset = glm::clamp(m_schematicOffset, 0.0f, 1.0f);
        } else {
            m_offset += delta;
            m_offset = glm::clamp(m_offset, 0.0f, 1.0f);
        }
    }

    void ConnJointSceneComp::onMouseDragEnd() {
        m_isDragging = false;
        (void)Edit::trackComp(*this, std::move(m_dragBefore), "joint-position");
        m_dragBefore = {};
        m_dragScene = UUID::null;
    }

    bool ConnJointSceneComp::onMouseButton(const Events::MouseButtonEvent &e) {
        if (e.action == Events::MouseClickAction::press) {
            if (e.button == Events::MouseButton::left) {
                onMouseLeftClick(e);
                return true;
            }
        }
        return false;
    }

    SimEngine::PortState
    ConnJointSceneComp::getSlotState(const SceneState &state) const {
        const auto &slotComp =
            state.getComponentByUuid<SlotSceneComponent>(m_outputSlotId);
        return slotComp->getSlotState(state);
    }

    SimEngine::PortState
    ConnJointSceneComp::getSlotState(const SceneDrawContext &context) const {
        const auto &slotComp =
            context.sceneState->getComponentByUuid<SlotSceneComponent>(
                m_outputSlotId);
        return slotComp->getSlotState(context);
    }

    void ConnJointSceneComp::addConnection(const UUID &connectionId) {
        m_connections.emplace_back(connectionId);
    }

    bool
    ConnJointSceneComp::onMouseLeftClick(const Events::MouseButtonEvent &e) {
        auto &connStartSlot = e.sceneState->getConnectionStartSlot();

        if (connStartSlot == UUID::null) {
            connStartSlot = m_uuid;
            return true;
        }

        connectWith(*e.sceneState, connStartSlot);
        connStartSlot = UUID::null;
        return true;
    }

    bool ConnJointSceneComp::connectWith(SceneState &sceneState,
                                         const UUID &slotId) {
        auto startSlot =
            sceneState.getComponentByUuid<SlotSceneComponent>(slotId);

        if (startSlot->getType() != SceneComponentType::slot) {
            BESS_WARN("[Scene] Two joints can't be connected directly");
            return false;
        }

        auto endComp =
            sceneState.getComponentByUuid<ConnJointSceneComp>(m_uuid);
        auto endSlot = sceneState.getComponentByUuid<SlotSceneComponent>(
            endComp->getOutputSlotId());

        auto conn = std::make_shared<ConnectionSceneComponent>();
        conn->setInitialSegmentCount(2);
        conn->setStartEndSlots(startSlot->getUuid(), endComp->getUuid());

        if (!sceneState.addConnTx(conn)) {
            BESS_WARN("Could not add connection");
            return false;
        }

        BESS_INFO("[Scene] Created connection {} between slots {} and {}",
                  (uint64_t)conn->getUuid(),
                  (uint64_t)startSlot->getUuid(),
                  (uint64_t)endComp->getUuid());

        return true;
    }

    void ConnJointSceneComp::removeConnection(const UUID &connectionId) {
        m_connections.erase(
            std::ranges::remove(m_connections, connectionId).begin(),
            m_connections.end());
    }

    std::vector<UUID> ConnJointSceneComp::cleanup(SceneState &state,
                                                  UUID caller) {
        auto ids = SceneComponent::cleanup(state, caller);

        for (const auto &connId : m_connections) {
            state.removeComponent(connId, m_uuid);
            ids.emplace_back(connId);
        }

        if (caller != m_connectionId) {
            auto connComp = state.getComponentByUuid<ConnectionSceneComponent>(
                m_connectionId);
            if (connComp) {
                connComp->removeAssociatedJoint(m_uuid);
            }
        }

        return ids;
    }

    std::vector<UUID>
    ConnJointSceneComp::getDependants(const SceneState &state) const {
        std::vector<UUID> dependants;

        for (const auto &connId : m_connections) {
            const auto &connComp =
                state.getComponentByUuid<ConnectionSceneComponent>(connId);
            BESS_ASSERT(connComp,
                        "[ConnJointDeps] connComp not found {} in joint {}",
                        (uint64_t)connId,
                        (uint64_t)m_uuid);
            if (!connComp)
                continue;
            const auto &ids = connComp->getDependants(state);
            dependants.insert(dependants.end(), ids.begin(), ids.end());
            dependants.emplace_back(connId);
        }

        return dependants;
    }

    void ConnJointSceneComp::onAttach(SceneState &state) {
        auto connComp =
            state.getComponentByUuid<ConnectionSceneComponent>(m_connectionId);
        if (connComp) {
            auto &associatedJoints = connComp->getAssociatedJoints();
            if (!std::ranges::any_of(
                    associatedJoints,
                    [&](const UUID &jointId) { return jointId == m_uuid; })) {
                connComp->addAssociatedJoint(m_uuid);
            }
        }
    }
} // namespace Bess::Canvas
