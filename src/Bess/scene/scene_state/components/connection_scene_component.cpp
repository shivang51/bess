#include "scene/scene_state/components/connection_scene_component.h"
#include "fwd.hpp"
#include "scene/scene_state/components/scene_component.h"
#include "scene/scene_state/components/slot_scene_component.h"
#include "settings/viewport_theme.h"

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

        if (m_isKeyDirty) {
            calculateKey(state);
        }

        auto startSlotComp = state.getComponentByUuid<SlotSceneComponent>(m_startSlot);
        auto endSlotComp = state.getComponentByUuid<SlotSceneComponent>(m_endSlot);

        glm::vec4 color;

        if (m_isSelected) {
            color = ViewportTheme::colors.selectedComp;
        } else {
            const auto &startSlotState = startSlotComp->getSlotState(state);
            const auto &endSlotState = endSlotComp->getSlotState(state);

            const bool isHigh = startSlotState.state == SimEngine::LogicState::high ||
                                endSlotState.state == SimEngine::LogicState::high;

            if (isHigh) {
                color = ViewportTheme::colors.stateHigh;
            } else {
                color = ViewportTheme::colors.stateLow;
            }
        }

        if (!startSlotComp || !endSlotComp)
            return;

        const auto startPos = startSlotComp->getAbsolutePosition(state);
        const auto endPos = endSlotComp->getAbsolutePosition(state);

        auto pos = startPos;
        auto prevPos = startPos;
        pos.z = 0.5f;
        PickingId pickingId{m_runtimeId, 0};
        pathRenderer->beginPathMode(pos,
                                    m_hoveredSegIdx == 0 ? 3 : 2,
                                    color, pickingId);

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
                                     color,
                                     pickingId);
            prevPos = pos;
        }

        pickingId.info = segmentIndex;
        pathRenderer->pathLineTo({glm::vec2(endPos), pos.z},
                                 m_hoveredSegIdx == segmentIndex ? 3 : 2,
                                 color,
                                 pickingId);
        pathRenderer->endPathMode(false, false, glm::vec4(0.f), true, true);
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
    }

    void ConnectionSceneComponent::onMouseDragBegin(const Events::MouseDraggedEvent &e) {
        m_draggedSegIdx = (int)e.details;
        m_isDragging = true;
    }

    void ConnectionSceneComponent::setStartEndSlots(const UUID &startSlot, const UUID &endSlot) {
        m_startSlot = startSlot;
        m_endSlot = endSlot;
        m_isKeyDirty = true;
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

    void ConnectionSceneComponent::calculateKey(SceneState &state) {
        if (!m_slotsKey.empty()) {
            state.removeSlotConnMapping(m_slotsKey);
        }

        m_slotsKey = genSlotsKey(state, m_startSlot, m_endSlot);

        state.addSlotConnMapping(m_slotsKey, m_uuid);

        m_isKeyDirty = false;
    }

    std::string ConnectionSceneComponent::genSlotsKey(const SceneState &state, const UUID &slotA, const UUID &slotB) {
        const auto startSlot = state.getComponentByUuid<SlotSceneComponent>(slotA);

        if (startSlot->getSlotType() == SlotType::digitalInput) {
            return slotA.toString() + "-" + slotB.toString();
        } else {
            return slotB.toString() + "-" + slotA.toString();
        }
    }

    std::pair<UUID, UUID> ConnectionSceneComponent::parseSlotsKey(const std::string &key) {
        auto delimiterPos = key.find('-');
        if (delimiterPos == std::string::npos) {
            return {UUID::null, UUID::null};
        }

        auto slotAStr = key.substr(0, delimiterPos);
        auto slotBStr = key.substr(delimiterPos + 1);

        return {UUID::fromString(slotAStr), UUID::fromString(slotBStr)};
    }

    std::string ConnectionSceneComponent::getSlotsKey() const {
        return m_slotsKey;
    }
} // namespace Bess::Canvas

namespace Bess::JsonConvert {
    void toJsonValue(const Bess::Canvas::ConnectionSceneComponent &component, Json::Value &j) {
        toJsonValue(static_cast<const Bess::Canvas::SceneComponent &>(component), j);
        toJsonValue(component.getStartSlot(), j["startSlot"]);
        toJsonValue(component.getEndSlot(), j["endSlot"]);

        j["segments"] = Json::Value(Json::arrayValue);
        for (const auto &segment : component.getSegments()) {
            Json::Value segJson;
            toJsonValue(segment, segJson);
            j["segments"].append(segJson);
        }
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
    }
} // namespace Bess::JsonConvert
