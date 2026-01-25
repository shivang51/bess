#include "scene/scene_state/components/connection_scene_component.h"
#include "commands/commands.h"
#include "common/log.h"
#include "event_dispatcher.h"
#include "fwd.hpp"
#include "scene/scene_state/components/conn_joint_scene_component.h"
#include "scene/scene_state/components/scene_component.h"
#include "scene/scene_state/components/scene_component_types.h"
#include "scene/scene_state/components/slot_scene_component.h"
#include "scene/scene_state/components/styles/sim_comp_style.h"
#include "scene/scene_state/scene_state.h"
#include "settings/viewport_theme.h"
#include "simulation_engine.h"
#include "types.h"

namespace Bess::Canvas {
    ConnectionSceneComponent::ConnectionSceneComponent() {
        initDragBehaviour();
        m_shouldReconstructSegments = true;
    }

    void ConnectionSceneComponent::drawSegments(const SceneState &state,
                                                const glm::vec3 &startPos,
                                                const glm::vec3 &endPos,
                                                const glm::vec4 &color,
                                                const std::shared_ptr<Renderer2D::Vulkan::PathRenderer> &pathRenderer) {

        if (startPos != m_segmentCachedPositions.front() ||
            endPos != m_segmentCachedPositions.back()) {
            resetSegmentPositionCache(state);
        }

        const float weight = state.getIsSchematicView()
                                 ? Styles::compSchematicStyles.strokeSize
                                 : 2.f;

        PickingId pickingId{m_runtimeId, 0};

        auto pos = m_segmentCachedPositions[0];
        pos.z = 0.5f;
        pathRenderer->beginPathMode(pos,
                                    m_hoveredSegIdx == 0 ? 3 : weight,
                                    color, pickingId);

        size_t segmentIndex = 0;
        for (size_t i = 1; i < m_segmentCachedPositions.size(); i++) {
            const auto &segPos = m_segmentCachedPositions[i];

            const bool isHovered = m_hoveredSegIdx == segmentIndex;
            pickingId.info = segmentIndex++;

            pathRenderer->pathLineTo(glm::vec3(segPos.x, segPos.y, 0.5f),
                                     isHovered ? 3 : weight,
                                     color,
                                     pickingId);
        }

        pathRenderer->endPathMode(false, false, glm::vec4(0.f), true, !state.getIsSchematicView());
    }

    void ConnectionSceneComponent::draw(SceneState &state,
                                        std::shared_ptr<Renderer::MaterialRenderer> materialRenderer,
                                        std::shared_ptr<Renderer2D::Vulkan::PathRenderer> pathRenderer) {

        if (m_isFirstDraw) {
            onFirstDraw(state, materialRenderer, pathRenderer);
        }

        auto startSlotComp = state.getComponentByUuid(m_startSlot);
        auto endSlotComp = state.getComponentByUuid(m_endSlot);

        if (!startSlotComp || !endSlotComp)
            return;

        if (m_shouldReconstructSegments) {
            reconsturctSegments(state);
        }

        if (m_segmentPosCacheDirty) {
            resetSegmentPositionCache(state);
        }

        glm::vec4 color;

        if (m_isSelected) {
            color = ViewportTheme::colors.selectedComp;
        } else if (!m_useCustomColor) {
            SimEngine::LogicState startSlotState{}, endSlotState{};

            auto comp = state.getComponentByUuid(m_startSlot);
            if (comp->getType() == SceneComponentType::slot) {
                const auto &slot = comp->cast<SlotSceneComponent>();
                startSlotState = slot->getSlotState(state).state;
            } else if (comp->getType() == SceneComponentType::connJoint) {
                const auto &slot = comp->cast<ConnJointSceneComp>();
                startSlotState = slot->getSlotState(state).state;
            } else {
                BESS_ASSERT(false, "Start slot component not convertable to SlotSceneComponent or ConnJointSceneComp");
            }

            comp = state.getComponentByUuid(m_endSlot);
            if (comp->getType() == SceneComponentType::slot) {
                const auto &slot = comp->cast<SlotSceneComponent>();
                endSlotState = slot->getSlotState(state).state;
            } else if (comp->getType() == SceneComponentType::connJoint) {
                const auto &slot = comp->cast<ConnJointSceneComp>();
                endSlotState = slot->getSlotState(state).state;
            } else {
                BESS_ASSERT(false, "End slot component not convertable to SlotSceneComponent or ConnJointSceneComp");
            }

            const bool isHigh = startSlotState == SimEngine::LogicState::high ||
                                endSlotState == SimEngine::LogicState::high;

            if (isHigh) {
                color = ViewportTheme::colors.stateHigh;
            } else {
                color = ViewportTheme::colors.stateLow;
            }
        } else {
            color = m_style.color;
        }

        const auto startPos = startSlotComp->getAbsolutePosition(state);
        const auto endPos = endSlotComp->getAbsolutePosition(state);

        drawSegments(state, startPos, endPos, color, pathRenderer);
    }

    void ConnectionSceneComponent::drawSchematic(SceneState &state,
                                                 std::shared_ptr<Renderer::MaterialRenderer> materialRenderer,
                                                 std::shared_ptr<Renderer2D::Vulkan::PathRenderer> pathRenderer) {
        if (m_isFirstSchematicDraw) {
            onFirstSchematicDraw(state, materialRenderer, pathRenderer);
        }

        if (m_shouldReconstructSegments) {
            reconsturctSegments(state);
        }

        if (m_segmentPosCacheDirty) {
            resetSegmentPositionCache(state);
        }

        auto startSlotComp = state.getComponentByUuid<SlotSceneComponent>(m_startSlot);
        auto endSlotComp = state.getComponentByUuid<SlotSceneComponent>(m_endSlot);

        if (!startSlotComp || !endSlotComp)
            return;

        glm::vec4 color;

        if (m_isSelected) {
            color = ViewportTheme::colors.selectedComp;
        } else if (!m_useCustomColor) {
            color = ViewportTheme::schematicViewColors.connection;
        } else {
            color = m_style.color;
        }

        auto startPos = startSlotComp->getSchematicPosAbsolute(state);
        auto endPos = endSlotComp->getSchematicPosAbsolute(state);
        if (startSlotComp->isInputSlot()) {
            startPos.x -= Styles::compSchematicStyles.pinSize;
            endPos.x += Styles::compSchematicStyles.pinSize;
        } else {
            startPos.x += Styles::compSchematicStyles.pinSize;
            endPos.x -= Styles::compSchematicStyles.pinSize;
        }
        drawSegments(state, startPos, endPos, color, pathRenderer);
    }

    void ConnectionSceneComponent::onMouseDragged(const Events::MouseDraggedEvent &e) {
        if (e.isMultiDrag)
            return;

        if (!m_isDragging) {
            onMouseDragBegin(e);
        }

        if (m_draggedSegIdx == 0) {
            ConnSegment newSeg{};
            auto &firstSeg = m_segments[0];
            newSeg.orientation = firstSeg.orientation == ConnSegOrientaion::horizontal
                                     ? ConnSegOrientaion::vertical
                                     : ConnSegOrientaion::horizontal;
            if (newSeg.orientation == ConnSegOrientaion::horizontal) {
                newSeg.offset.x = e.delta.x;
            } else {
                newSeg.offset.y = e.delta.y;
            }

            m_segments.insert(m_segments.begin(), newSeg);

            m_draggedSegIdx++;
            m_hoveredSegIdx++;
        } else if (m_draggedSegIdx == m_segments.size()) {
            ConnSegment newSeg{};
            auto &lastSeg = m_segments[m_segments.size() - 1];
            newSeg.orientation = lastSeg.orientation == ConnSegOrientaion::horizontal
                                     ? ConnSegOrientaion::vertical
                                     : ConnSegOrientaion::horizontal;

            // offset of last segment is set from end point

            m_segments.emplace_back(newSeg);
        }

        auto &seg = m_segments[m_draggedSegIdx - 1];

        if (seg.orientation == ConnSegOrientaion::horizontal) {
            seg.offset.x += e.delta.x;
        } else {
            seg.offset.y += e.delta.y;
        }

        m_segmentPosCacheDirty = true;
    }

    void ConnectionSceneComponent::onMouseDragBegin(const Events::MouseDraggedEvent &e) {
        m_draggedSegIdx = (int)e.details;
        m_isDragging = true;
    }

    void ConnectionSceneComponent::setStartEndSlots(const UUID &startSlot, const UUID &endSlot) {
        m_startSlot = startSlot;
        m_endSlot = endSlot;
    }

    void ConnectionSceneComponent::reconsturctSegments(const SceneState &state) {
        if (m_startSlot == UUID::null || m_endSlot == UUID::null) {
            return;
        }

        m_segments.clear();

        auto startSlotComp = state.getComponentByUuid(m_startSlot);
        auto endSlotComp = state.getComponentByUuid(m_endSlot);

        if (!startSlotComp || !endSlotComp)
            return;

        const auto startPos = startSlotComp->getAbsolutePosition(state);
        const auto endPos = endSlotComp->getAbsolutePosition(state);

        const float midX = (endPos.x - startPos.x) / 2.f;
        const float height = (endPos.y - startPos.y) / 2.f;

        ConnSegment midPoint;
        midPoint.offset = glm::vec2{midX, 0};
        midPoint.orientation = ConnSegOrientaion::horizontal;
        m_segments.emplace_back(midPoint);

        ConnSegment endPoint;
        endPoint.offset = glm::vec2{0, height};
        endPoint.orientation = ConnSegOrientaion::vertical;
        m_segments.emplace_back(endPoint);
        m_segmentPosCacheDirty = true;
        m_shouldReconstructSegments = false;
    }

    void ConnectionSceneComponent::onFirstDraw(SceneState &sceneState,
                                               std::shared_ptr<Renderer::MaterialRenderer> materialRenderer,
                                               std::shared_ptr<PathRenderer> pathRenderer) {

        m_isFirstDraw = false;
    }

    void ConnectionSceneComponent::onMouseEnter(const Events::MouseEnterEvent &e) {
        m_hoveredSegIdx = (int)e.details;
    }

    void ConnectionSceneComponent::onMouseLeave(const Events::MouseLeaveEvent &e) {
        m_hoveredSegIdx = -1;
    }

    std::vector<UUID> ConnectionSceneComponent::cleanup(SceneState &state, UUID caller) {
        auto slotCompA = state.getComponentByUuid<SlotSceneComponent>(m_startSlot);
        auto slotCompB = state.getComponentByUuid<SlotSceneComponent>(m_endSlot);

        // clean the connection from simulation engine,
        // if only connection between the slots is being removed
        // in that case caller is null
        // if connection is being removed as part of slot/component removal,
        // the slot/component cleanup will handle removing connections from sim engine
        if (caller == UUID::null) {
            auto &simEngine = SimEngine::SimulationEngine::instance();
            const auto &simCompA = state.getComponentByUuid<SimulationSceneComponent>(
                slotCompA->getParentComponent());
            const auto &simCompB = state.getComponentByUuid<SimulationSceneComponent>(
                slotCompB->getParentComponent());

            const auto pinTypeA = slotCompA->getSlotType() == SlotType::digitalInput
                                      ? SimEngine::SlotType::digitalInput
                                      : SimEngine::SlotType::digitalOutput;
            const auto pinTypeB = slotCompB->getSlotType() == SlotType::digitalInput
                                      ? SimEngine::SlotType::digitalInput
                                      : SimEngine::SlotType::digitalOutput;

            SimEngine::Commands::DelConnectionCommandData data = {simCompA->getSimEngineId(),
                                                                  (uint32_t)slotCompA->getIndex(), pinTypeA,
                                                                  simCompB->getSimEngineId(),
                                                                  (uint32_t)slotCompB->getIndex(), pinTypeB};

            auto &cmdMngr = simEngine.getCmdManager();
            auto _ = cmdMngr.execute<SimEngine::Commands::DelConnectionCommand,
                                     std::string>(std::vector{data});
        }

        if (slotCompA) {
            slotCompB->removeConnection(m_uuid);
        }

        if (slotCompB) {
            slotCompB->removeConnection(m_uuid);
        }

        Events::ConnectionRemovedEvent event{m_startSlot,
                                             m_endSlot};
        EventSystem::EventDispatcher::instance().dispatch(event);
        return {};
    }

    void ConnectionSceneComponent::onMouseButton(const Events::MouseButtonEvent &e) {
        const int segIdx = (int)e.details;
        Events::ConnSegClickEvent event{m_uuid, segIdx, e.button, e.action};
        EventSystem::EventDispatcher::instance().dispatch(event);
    }

    void ConnectionSceneComponent::resetSegmentPositionCache(const SceneState &state) {
        m_segmentPosCacheDirty = false;
        m_segmentCachedPositions.clear();

        auto startSlotComp = state.getComponentByUuid<SlotSceneComponent>(m_startSlot);
        auto endSlotComp = state.getComponentByUuid<SlotSceneComponent>(m_endSlot);

        if (!startSlotComp || !endSlotComp)
            return;

        const auto startPos = startSlotComp->getAbsolutePosition(state);
        const auto endPos = endSlotComp->getAbsolutePosition(state);

        const float offsetXDecr = state.getIsSchematicView()
                                      ? Styles::compSchematicStyles.pinSize
                                      : 0.f;
        auto pos = startPos;
        auto prevPos = pos;

        m_segmentCachedPositions.push_back(startPos);

        int segmentIndex = 0;
        for (auto &segment : m_segments) {
            pos += glm::vec3(segment.offset, 0);
            if (segment.orientation == ConnSegOrientaion::horizontal) {
                pos.x -= offsetXDecr;
            }

            // Ensure the last segment ends exactly in straight line to the endPos
            if (segmentIndex == m_segments.size() - 1) {
                if (segment.orientation == ConnSegOrientaion::horizontal) {
                    pos.x = endPos.x;
                    segment.offset.x = pos.x - prevPos.x;
                } else {
                    pos.y = endPos.y;
                    segment.offset.y = pos.y - prevPos.y;
                }
            }

            segmentIndex++;
            prevPos = pos;
            m_segmentCachedPositions.push_back(pos);
        }

        m_segmentCachedPositions.emplace_back(endPos);
    }

    glm::vec3 ConnectionSceneComponent::getSegVertexPos(const SceneState &state, size_t vertexIdx) {
        if (m_segmentPosCacheDirty) {
            resetSegmentPositionCache(state);
        }

        if (vertexIdx >= m_segmentCachedPositions.size()) {
            BESS_WARN("[ConnectionSceneComponent] Requested segment vertex index {} out of bounds (max {})",
                      vertexIdx,
                      m_segmentCachedPositions.size() - 1);
            return glm::vec3(0.f);
        }

        return m_segmentCachedPositions[vertexIdx];
    }
} // namespace Bess::Canvas

namespace Bess::JsonConvert {
    void toJsonValue(const Bess::Canvas::ConnectionSceneComponent &component, Json::Value &j) {
        toJsonValue(static_cast<const Bess::Canvas::SceneComponent &>(component), j);
        toJsonValue(component.getStartSlot(), j["startSlot"]);
        toJsonValue(component.getEndSlot(), j["endSlot"]);

        j["segments"] = Json::Value(Json::arrayValue);
        for (const auto &segment : component.getSegments()) {
            toJsonValue(segment,
                        j["segments"].append(Json::Value()));
        }

        j["useCustomColor"] = component.getUseCustomColor();
    }

    void fromJsonValue(const Json::Value &j, Bess::Canvas::ConnectionSceneComponent &component) {
        fromJsonValue(j, static_cast<Bess::Canvas::SceneComponent &>(component));

        UUID startSlot, endSlot;
        if (j.isMember("startSlot")) {
            fromJsonValue(j["startSlot"], startSlot);
        }

        if (j.isMember("endSlot")) {
            fromJsonValue(j["endSlot"], endSlot);
        }

        component.setStartEndSlots(startSlot, endSlot);

        if (j.isMember("segments") && j["segments"].isArray()) {
            std::vector<Canvas::ConnSegment> segments;
            for (const auto &segJson : j["segments"]) {
                Canvas::ConnSegment segment;
                fromJsonValue(segJson, segment);
                segments.push_back(segment);
            }
            component.setSegments(segments);
        }

        if (j.isMember("useCustomColor")) {
            component.setUseCustomColor(j["useCustomColor"].asBool());
        }

        component.setShouldReconstructSegments(false);
    }
} // namespace Bess::JsonConvert
