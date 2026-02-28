#include "connection_scene_component.h"
#include "common/bess_assert.h"
#include "common/bess_uuid.h"
#include "common/logger.h"
#include "conn_joint_scene_component.h"
#include "event_dispatcher.h"
#include "fwd.hpp"
#include "pages/main_page/cmds/add_comp_cmd.h"
#include "pages/main_page/main_page.h"
#include "scene/scene_state/components/scene_component.h"
#include "scene/scene_state/components/scene_component_types.h"
#include "scene/scene_state/components/styles/sim_comp_style.h"
#include "scene/scene_state/scene_state.h"
#include "settings/viewport_theme.h"
#include "slot_scene_component.h"
#include "types.h"
#include "ui/ui.h"
#include <cstdint>

namespace Bess::Canvas {
    ConnectionSceneComponent::ConnectionSceneComponent() {
        initDragBehaviour();
    }

    void ConnectionSceneComponent::drawSegments(const SceneState &state,
                                                const glm::vec3 &startPos,
                                                const glm::vec3 &endPos,
                                                const glm::vec4 &color,
                                                const std::shared_ptr<Renderer2D::Vulkan::PathRenderer> &pathRenderer) {

        const auto &segCache = state.getIsSchematicView()
                                   ? m_segCachedSchemeticPos
                                   : m_segmentCachedPositions;

        BESS_ASSERT(!segCache.empty(), "Segment position cache is empty");

        if (startPos != segCache.front() ||
            endPos != segCache.back()) {
            resetSegmentPositionCache(state);
        }

        const float weight = state.getIsSchematicView()
                                 ? Styles::compSchematicStyles.strokeSize
                                 : 2.f;

        PickingId pickingId{m_runtimeId, 0};

        auto pos = segCache.front();
        pos.z = 0.5f;
        pathRenderer->beginPathMode(pos,
                                    m_hoveredSegIdx == 0 ? 3 : weight,
                                    color, pickingId);

        size_t segmentIndex = 0;
        for (size_t i = 1; i < segCache.size(); i++) {
            const auto &segPos = segCache[i];

            const bool isHovered = m_hoveredSegIdx == segmentIndex;
            pickingId.info = segmentIndex++;

            pathRenderer->pathLineTo(glm::vec3(segPos.x, segPos.y, isHovered ? 0.82f : pos.z),
                                     isHovered ? 3 : weight,
                                     color,
                                     pickingId);
        }

        pathRenderer->endPathMode(false, false, glm::vec4(0.f), true,
                                  !state.getIsSchematicView());
    }

    void ConnectionSceneComponent::draw(SceneState &state,
                                        std::shared_ptr<Renderer::MaterialRenderer> materialRenderer,
                                        std::shared_ptr<Renderer2D::Vulkan::PathRenderer> pathRenderer) {

        const auto &startComp = state.getComponentByUuid(m_startSlot);
        const auto &endComp = state.getComponentByUuid(m_endSlot);

        if (!startComp || !endComp)
            return;

        if (m_isFirstDraw) {
            if (m_segments.empty()) {
                BESS_WARN("Segments empty on first draw for {}", (uint64_t)m_uuid);
                if (!m_isFirstSchematicDraw) {
                    m_segments = m_schematicSegments;
                } else {
                    m_shouldReconstructSegments = true;
                }
            }
            m_segmentPosCacheDirty = true;
            m_isFirstDraw = false;
        }

        if (m_shouldReconstructSegments) {
            reconstructSegments(state);
        }

        if (m_segmentPosCacheDirty) {
            resetSegmentPositionCache(state);
        }

        glm::vec4 color;

        if (m_isSelected) {
            color = ViewportTheme::colors.selectedComp;
        } else if (!m_useCustomColor) {
            SimEngine::LogicState startSlotState{}, endSlotState{};
            if (startComp->getType() == SceneComponentType::slot) {
                const auto &slot = startComp->cast<SlotSceneComponent>();
                startSlotState = slot->getSlotState(state).state;
            } else if (startComp->getType() == SceneComponentType::connJoint) {
                const auto &slot = startComp->cast<ConnJointSceneComp>();
                startSlotState = slot->getSlotState(state).state;
            } else {
                BESS_ASSERT(false, "Start slot component not convertable to SlotSceneComponent or ConnJointSceneComp");
            }

            if (endComp->getType() == SceneComponentType::slot) {
                const auto &slot = endComp->cast<SlotSceneComponent>();
                endSlotState = slot->getSlotState(state).state;
            } else if (endComp->getType() == SceneComponentType::connJoint) {
                const auto &slot = endComp->cast<ConnJointSceneComp>();
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

        auto startPos = startComp->getAbsolutePosition(state);
        if (startComp->getType() == SceneComponentType::slot) {
            startPos = startComp->cast<SlotSceneComponent>()->getConnectionPos(state);
        }

        auto endPos = endComp->getAbsolutePosition(state);
        if (endComp->getType() == SceneComponentType::slot) {
            endPos = endComp->cast<SlotSceneComponent>()->getConnectionPos(state);
        }

        drawSegments(state, startPos, endPos, color, pathRenderer);

        if (m_hoveredSegIdx >= 0 && state.getConnectionStartSlot() != UUID::null) {
            materialRenderer->drawCircle(
                {state.getMousePos(), 0.51f},
                6.f,
                ViewportTheme::colors.selectedComp,
                PickingId{m_runtimeId, (uint32_t)m_hoveredSegIdx});
        }
    }

    void ConnectionSceneComponent::drawSchematic(SceneState &state,
                                                 std::shared_ptr<Renderer::MaterialRenderer> materialRenderer,
                                                 std::shared_ptr<Renderer2D::Vulkan::PathRenderer> pathRenderer) {
        if (m_isFirstSchematicDraw) {
            m_isFirstSchematicDraw = false;
            if (m_schematicSegments.empty()) {
                BESS_WARN("Schematic segments empty on first schematic draw for {}, copying from normal segments",
                          (uint64_t)m_uuid);
                m_schematicSegments = m_segments;
            }
            m_shouldReconstructSegments = m_schematicSegments.empty();
            m_segmentPosCacheDirty = true;
        }

        if (m_shouldReconstructSegments) {
            reconstructSegments(state);
        }

        if (m_segmentPosCacheDirty) {
            resetSegmentPositionCache(state);
        }

        const auto startComp = state.getComponentByUuid<SlotSceneComponent>(m_startSlot);
        const auto endComp = state.getComponentByUuid<SlotSceneComponent>(m_endSlot);

        if (!startComp || !endComp)
            return;

        glm::vec4 color;

        if (m_isSelected) {
            color = ViewportTheme::colors.selectedComp;
        } else if (!m_useCustomColor) {
            color = ViewportTheme::schematicViewColors.connection;
        } else {
            color = m_style.color;
        }

        auto startPos = startComp->getAbsolutePosition(state);
        if (startComp->getType() == SceneComponentType::slot) {
            startPos = startComp->cast<SlotSceneComponent>()->getConnectionPos(state);
        }

        auto endPos = endComp->getAbsolutePosition(state);
        if (endComp->getType() == SceneComponentType::slot) {
            endPos = endComp->cast<SlotSceneComponent>()->getConnectionPos(state);
        }

        if (startComp->getType() == SceneComponentType::slot &&
            startComp->isInputSlot()) {
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

        auto &segs = e.sceneState->getIsSchematicView()
                         ? m_schematicSegments
                         : m_segments;

        if (m_draggedSegIdx == 0) {
            ConnSegment newSeg{};
            auto &firstSeg = segs[0];
            newSeg.orientation = firstSeg.orientation == ConnSegOrientaion::horizontal
                                     ? ConnSegOrientaion::vertical
                                     : ConnSegOrientaion::horizontal;
            if (newSeg.orientation == ConnSegOrientaion::horizontal) {
                newSeg.offset.x = e.delta.x;
            } else {
                newSeg.offset.y = e.delta.y;
            }

            segs.insert(segs.begin(), newSeg);

            m_draggedSegIdx++;
            m_hoveredSegIdx++;
        } else if (m_draggedSegIdx == segs.size()) {
            ConnSegment newSeg{};
            auto &lastSeg = segs[segs.size() - 1];
            newSeg.orientation = lastSeg.orientation == ConnSegOrientaion::horizontal
                                     ? ConnSegOrientaion::vertical
                                     : ConnSegOrientaion::horizontal;

            // offset of last segment is set from end point

            segs.emplace_back(newSeg);
        }

        auto &seg = segs[m_draggedSegIdx - 1];

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

    void ConnectionSceneComponent::reconstructSegments(const SceneState &state) {
        if (m_startSlot == UUID::null || m_endSlot == UUID::null) {
            return;
        }

        auto &segments = state.getIsSchematicView()
                             ? m_schematicSegments
                             : m_segments;
        segments.clear();

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
        segments.emplace_back(midPoint);

        if (m_initialSegmentCount == 3) {
            ConnSegment endPoint;
            endPoint.offset = glm::vec2{0, height};
            endPoint.orientation = ConnSegOrientaion::vertical;
            segments.emplace_back(endPoint);
        }

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
        UI::setCursorPointer();
    }

    void ConnectionSceneComponent::onMouseLeave(const Events::MouseLeaveEvent &e) {
        m_hoveredSegIdx = -1;
        UI::setCursorNormal();
    }

    std::vector<UUID> ConnectionSceneComponent::cleanup(SceneState &state, UUID caller) {

        for (auto &jointId : m_associatedJoints) {
            auto jointComp = state.getComponentByUuid<ConnJointSceneComp>(jointId);
            if (jointComp) {
                state.removeComponent(jointId, m_uuid);
                BESS_INFO("[Scene] Removed associated joint component {}", (uint64_t)jointId);
            }
        }

        Canvas::Events::ConnectionRemovedEvent event{m_startSlot,
                                                     m_startSlot};
        EventSystem::EventDispatcher::instance().queue(event);

        return {};
    }

    void ConnectionSceneComponent::onMouseButton(const Events::MouseButtonEvent &e) {
        if (e.action != Events::MouseClickAction::press ||
            e.button != Events::MouseButton::left)
            return;

        const int segIdx = (int)e.details;

        if (e.sceneState->getConnectionStartSlot() != UUID::null) {

            // create new joint
            const auto &oriEven = getSegments()[0].orientation;
            const auto &oriOdd = oriEven == ConnSegOrientaion::horizontal
                                     ? ConnSegOrientaion::vertical
                                     : ConnSegOrientaion::horizontal;

            // since there are only two orientations both alternating
            auto ori = (segIdx % 2 == 0) ? oriEven : oriOdd;

            auto jointComp = std::make_shared<ConnJointSceneComp>(m_uuid, segIdx, ori);

            const auto &startComp = e.sceneState->getComponentByUuid(m_startSlot);
            const auto &endComp = e.sceneState->getComponentByUuid(m_endSlot);

            if (startComp->getType() == SceneComponentType::connJoint) {
                auto startJoint = startComp->cast<ConnJointSceneComp>();
                jointComp->setOutputSlotId(startJoint->getOutputSlotId());
                jointComp->setInputSlotId(startJoint->getInputSlotId());
            } else if (endComp->getType() == SceneComponentType::connJoint) {
                auto endJoint = endComp->cast<ConnJointSceneComp>();
                jointComp->setOutputSlotId(endJoint->getOutputSlotId());
                jointComp->setInputSlotId(endJoint->getInputSlotId());
            } else if (!startComp->cast<SlotSceneComponent>()->isInputSlot()) {
                jointComp->setOutputSlotId(startComp->getUuid());
                jointComp->setInputSlotId(endComp->getUuid());
            } else {
                jointComp->setOutputSlotId(endComp->getUuid());
                jointComp->setInputSlotId(startComp->getUuid());
            }

            m_associatedJoints.emplace_back(jointComp->getUuid());
            BESS_INFO("[Scene] Created joint component {}", (uint64_t)jointComp->getUuid());

            // calculating t value for joint position between segment vertices
            const auto jointPos = e.sceneState->getMousePos();
            const auto segStartPos = getSegVertexPos(*e.sceneState, segIdx);
            const auto segEndPos = getSegVertexPos(*e.sceneState, segIdx + 1);
            float t = 0.f;
            if (glm::distance(segStartPos, segEndPos) > 0.f) {
                t = glm::distance(segStartPos, glm::vec3{jointPos, 0.f}) /
                    glm::distance(segStartPos, segEndPos);
            }
            jointComp->setSegOffset(t);

            // add to scene
            auto &cmdManager = Pages::MainPage::getInstance()->getState().getCommandSystem();
            cmdManager.execute(std::make_unique<Cmd::AddCompCmd<ConnJointSceneComp>>(jointComp));

            // connect with start slot
            jointComp->connectWith(*e.sceneState, e.sceneState->getConnectionStartSlot());

            e.sceneState->setConnectionStartSlot(UUID::null);
            return;
        }
    }

    void ConnectionSceneComponent::resetSegmentPositionCache(const SceneState &state) {
        m_segmentPosCacheDirty = false;

        const auto &startComp = state.getComponentByUuid(m_startSlot);
        const auto &endComp = state.getComponentByUuid(m_endSlot);

        if (!startComp || !endComp)
            return;

        auto startPos = startComp->getAbsolutePosition(state);
        if (startComp->getType() == SceneComponentType::slot) {
            startPos = startComp->cast<SlotSceneComponent>()->getConnectionPos(state);
        }

        auto endPos = endComp->getAbsolutePosition(state);
        if (endComp->getType() == SceneComponentType::slot) {
            endPos = endComp->cast<SlotSceneComponent>()->getConnectionPos(state);
        }

        const float offsetXDecr = state.getIsSchematicView()
                                      ? Styles::compSchematicStyles.pinSize
                                      : 0.f;
        auto pos = startPos;
        auto prevPos = pos;

        auto &segments = state.getIsSchematicView()
                             ? m_schematicSegments
                             : m_segments;

        auto &cache = state.getIsSchematicView()
                          ? m_segCachedSchemeticPos
                          : m_segmentCachedPositions;

        cache.clear();
        cache.push_back(startPos);

        int segmentIndex = 0;
        for (auto &segment : segments) {
            pos += glm::vec3(segment.offset, 0);
            if (segment.orientation == ConnSegOrientaion::horizontal) {
                pos.x -= offsetXDecr;
            }

            // Ensure the last segment ends exactly in straight line to the endPos
            if (segmentIndex == segments.size() - 1) {
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
            cache.push_back(pos);
        }

        cache.emplace_back(endPos);
    }

    glm::vec3 ConnectionSceneComponent::getSegVertexPos(const SceneState &state, size_t vertexIdx) {
        if (m_segmentPosCacheDirty) {
            resetSegmentPositionCache(state);
        }

        auto &segments = state.getIsSchematicView()
                             ? m_segCachedSchemeticPos
                             : m_segmentCachedPositions;

        if (vertexIdx >= segments.size()) {
            BESS_WARN("[ConnectionSceneComponent] Requested segment vertex index {} out of bounds (max {})",
                      vertexIdx,
                      segments.size() - 1);
            return glm::vec3(0.f);
        }

        return segments[vertexIdx];
    }

    void ConnectionSceneComponent::addAssociatedJoint(const UUID &jointId) {
        m_associatedJoints.emplace_back(jointId);
    }

    void ConnectionSceneComponent::removeAssociatedJoint(const UUID &jointId) {
        m_associatedJoints.erase(std::ranges::remove(m_associatedJoints,
                                                     jointId)
                                     .begin(),
                                 m_associatedJoints.end());
    }

    std::vector<UUID> ConnectionSceneComponent::getDependants(const SceneState &state) const {
        std::vector<UUID> dependants;

        // get joints associated with this connection
        for (const auto &jointId : m_associatedJoints) {
            const auto &jointComp = state.getComponentByUuid<ConnJointSceneComp>(jointId);
            const auto &jointDeps = jointComp->getDependants(state);
            dependants.insert(dependants.end(), jointDeps.begin(), jointDeps.end());
            dependants.push_back(jointId);
        }

        // get its depents from ConnectionService
        auto &connectionsSvc = Svc::SvcConnection::instance();
        const auto &connDependants = connectionsSvc.getDependants(getUuid());
        dependants.insert(dependants.end(), connDependants.begin(), connDependants.end());

        return dependants;
    }
} // namespace Bess::Canvas
