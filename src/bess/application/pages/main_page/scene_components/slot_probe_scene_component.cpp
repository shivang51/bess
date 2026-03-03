#include "slot_probe_scene_component.h"
#include "common/bess_uuid.h"
#include "pages/main_page/scene_components/slot_scene_component.h"
#include "renderer/material_renderer.h"
#include "scene_state/scene_state.h"
#include "settings/viewport_theme.h"

namespace Bess::Canvas {
    void SlotProbeSceneComponent::draw(SceneState &sceneState,
                                       std::shared_ptr<Renderer::MaterialRenderer> materialRenderer,
                                       std::shared_ptr<PathRenderer> pathRenderer) {

        const auto &textSize = materialRenderer->getTextRenderSize("Probed", 9);
        if (m_isFirstDraw) {
            m_transform.scale = {textSize.x + 16.f, textSize.y + 8.f};
            m_isFirstDraw = false;
        }

        Renderer::QuadRenderProperties props;
        props.borderColor = m_isSelected
                                ? ViewportTheme::colors.selectedComp
                                : ViewportTheme::colors.componentBorder;
        props.isMica = true;
        props.borderRadius = glm::vec4(4.f);
        props.borderSize = glm::vec4(1.f);

        const auto &scale = m_transform.scale;
        const auto &startPos = getAbsolutePosition(sceneState);
        const bool isProbed = m_probedSlotUuid != UUID::null;

        materialRenderer->drawQuad(startPos,
                                   scale,
                                   isProbed
                                       ? ViewportTheme::colors.clockConnectionLow
                                       : ViewportTheme::colors.componentBG,
                                   PickingId{m_runtimeId, 0},
                                   props);

        materialRenderer->drawText(isProbed ? "Probed" : "Probe",
                                   startPos + glm::vec3(-textSize.x / 2.f, (textSize.y / 2.f) - 2.f, 0.0001f),
                                   9,
                                   isProbed
                                       ? ViewportTheme::colors.clockConnectionHigh
                                       : ViewportTheme::colors.text,
                                   PickingId{m_runtimeId, 0});

        if (m_probedSlotUuid != UUID::null) {
            const auto &comp = sceneState.getComponentByUuid<SlotSceneComponent>(m_probedSlotUuid);
            auto endPos = comp->getConnectionPos(sceneState);

            // This looks awesome, just hit and trial :)
            const glm::vec2 ctrl1 = glm::mix(glm::vec2(startPos.x, 0),
                                             glm::vec2(endPos.x, 0),
                                             0.25f);
            const glm::vec2 ctrl2 = glm::mix(glm::vec2(0, startPos.y),
                                             glm::vec2(0, endPos.y),
                                             0.75f);

            const auto &color = !m_probeData.empty() &&
                                        m_probeData.back().second == SimEngine::LogicState::high
                                    ? ViewportTheme::colors.stateHigh
                                    : ViewportTheme::colors.stateLow;

            pathRenderer->beginPathMode({startPos.x - (textSize.x / 2.f), startPos.y, 0.51},
                                        1.f,
                                        color,
                                        PickingId{m_runtimeId, 1});

            endPos.z = 0.51f;
            pathRenderer->pathCubicBeizerTo(endPos,
                                            ctrl1,
                                            ctrl2,
                                            1.f,
                                            color,
                                            PickingId{m_runtimeId, 1});

            pathRenderer->endPathMode();
        }
    }

    void SlotProbeSceneComponent::update(TimeMs frameTime, SceneState &state) {
        if (m_probedSlotUuid == UUID::null)
            return;

        const auto &comp = state.getComponentByUuid<SlotSceneComponent>(m_probedSlotUuid);
        if (!comp)
            return;

        auto slotState = comp->getSlotState(state);
        if (m_probeData.empty()) {
            BESS_DEBUG("Probe data is empty, adding first entry");
            m_probeData.emplace_back(slotState.lastChangeTime,
                                     slotState.state);
        } else {
            auto &lastEntry = m_probeData.back();
            if (slotState.state != lastEntry.second) {
                BESS_DEBUG("Slot state changed: {} -> {}, adding entry to probe data",
                           lastEntry.second == SimEngine::LogicState::high ? "high" : "stateLow",
                           slotState.state == SimEngine::LogicState::high ? "high" : "stateLow");
                m_probeData.emplace_back(slotState.lastChangeTime,
                                         slotState.state);
            }
        }
    }

    void SlotProbeSceneComponent::onMouseButton(const Events::MouseButtonEvent &e) {
        if (e.action == Events::MouseClickAction::press && e.button == Events::MouseButton::left) {
            const auto &connStartSlot = e.sceneState->getConnectionStartSlot();
            if (connStartSlot != UUID::null) {
                const auto &comp = e.sceneState->getComponentByUuid<SlotSceneComponent>(connStartSlot);
                if (comp &&
                    comp->getType() == SceneComponentType::slot &&
                    comp->getSlotType() != SlotType::inputsResize &&
                    comp->getSlotType() != SlotType::outputsResize) {
                    setProbedSlotUuid(e.sceneState->getConnectionStartSlot());
                    e.sceneState->setConnectionStartSlot(UUID::null);
                }
            }
        }
    }

    void SlotProbeSceneComponent::onProbedSlotChanged() {
        m_probeData.clear();
    }

    void SlotProbeSceneComponent::onAttach(SceneState &state) {
        m_name = "Slot Probe";
    }
} // namespace Bess::Canvas
