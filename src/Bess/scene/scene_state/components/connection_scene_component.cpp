#include "scene/scene_state/components/connection_scene_component.h"
#include "fwd.hpp"
#include "scene/scene_state/components/scene_component.h"

namespace Bess::Canvas {
    ConnectionSceneComponent::ConnectionSceneComponent(UUID uuid)
        : SceneComponent(uuid) {
        initDragBehaviour();
    }

    ConnectionSceneComponent::ConnectionSceneComponent(UUID uuid, const Transform &transform)
        : SceneComponent(uuid, transform) {
        initDragBehaviour();
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

        const auto startPos = startSlotComp->getAbsolutePosition(state);
        const auto endPos = endSlotComp->getAbsolutePosition(state);

        auto pos = startPos;
        auto prevPos = startPos;
        pos.z = 1.001f;
        PickingId pickingId{m_runtimeId, 0};
        pathRenderer->beginPathMode(pos,
                                    m_hoveredSegIdx == 0 ? 3 : 2,
                                    m_style.color, pickingId);

        int segmentIndex = 0;
        for (auto &segment : m_segments) {
            pos += glm::vec3(segment.offset, 0);

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

            bool isHovered = m_hoveredSegIdx == segmentIndex;
            pickingId.info = segmentIndex++;

            pathRenderer->pathLineTo(pos,
                                     isHovered ? 3 : 2,
                                     m_style.color, pickingId);
            prevPos = pos;
        }

        pickingId.info = segmentIndex;
        pathRenderer->pathLineTo(endPos,
                                 m_hoveredSegIdx == segmentIndex ? 3 : 2,
                                 m_style.color, pickingId);
        pathRenderer->endPathMode(false);
    }

    void ConnectionSceneComponent::onMouseDragged(const Events::MouseDraggedEvent &e) {
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

        const auto newOffset = e.delta;

        auto &seg = m_segments[m_draggedSegIdx - 1];

        if (seg.orientation == ConnSegOrientaion::horizontal) {
            seg.offset.x += newOffset.x;
        } else {
            seg.offset.y += newOffset.y;
        }
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
    }

    void ConnectionSceneComponent::onFirstDraw(SceneState &sceneState,
                                               std::shared_ptr<Renderer::MaterialRenderer> materialRenderer,
                                               std::shared_ptr<PathRenderer> pathRenderer) {

        m_isFirstDraw = false;
        reconsturctSegments(sceneState);
    }

    void ConnectionSceneComponent::onMouseEnter(const Events::MouseEnterEvent &e) {
        m_hoveredSegIdx = (int)e.details;
    }

    void ConnectionSceneComponent::onMouseLeave(const Events::MouseLeaveEvent &e) {
        m_hoveredSegIdx = -1;
    }
} // namespace Bess::Canvas
