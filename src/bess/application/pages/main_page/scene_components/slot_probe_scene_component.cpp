#include "slot_probe_scene_component.h"
#include "common/bess_uuid.h"
#include "imgui.h"
#include "pages/main_page/main_page.h"
#include "pages/main_page/scene_components/sim_scene_component.h"
#include "pages/main_page/scene_components/slot_scene_component.h"
#include "renderer/material_renderer.h"
#include "scene_draw_context.h"
#include "scene_state/scene_state.h"
#include "settings/viewport_theme.h"
#include "simulation_engine.h"
#include "types.h"

namespace Bess::Canvas {
    std::vector<std::shared_ptr<SceneComponent>> SlotProbeSceneComponent::clone(const SceneState &sceneState) const {
        (void)sceneState;
        auto clonedComponent = std::make_shared<SlotProbeSceneComponent>(*this);
        prepareClone(*clonedComponent);
        clonedComponent->setProbeData({});
        clonedComponent->m_probedSlotUuid = UUID::null;
        return {clonedComponent};
    }

    void SlotProbeSceneComponent::draw(SceneDrawContext &context) {

        const auto &textSize = Renderer::MaterialRenderer::getTextRenderSize(m_name, 9);
        if (m_isFirstDraw) {
            m_scaleDirty = true;
            m_isFirstDraw = false;
        }

        if (m_scaleDirty) {
            m_transform.scale = {textSize.x + 16.f, textSize.y + 8.f};
            m_scaleDirty = false;
        }

        Renderer::QuadRenderProperties props;
        props.borderColor = m_isSelected
                                ? ViewportTheme::colors.selectedComp
                                : ViewportTheme::colors.componentBorder;
        props.isMica = true;
        props.borderRadius = glm::vec4(4.f);
        props.borderSize = glm::vec4(1.f);

        const auto &sceneState = *context.sceneState;
        const auto &scale = m_transform.scale;
        const auto &startPos = getAbsolutePosition(sceneState);
        const bool isProbed = m_probedSlotUuid != UUID::null;

        context.materialRenderer->drawQuad(startPos,
                                           scale,
                                           isProbed
                                               ? ViewportTheme::colors.clockConnectionLow
                                               : ViewportTheme::colors.componentBG,
                                           PickingId{m_runtimeId, 0},
                                           props);

        context.materialRenderer->drawText(m_name,
                                           startPos + glm::vec3(-textSize.x / 2.f, (textSize.y / 2.f) - 2.f, 0.0001f),
                                           9,
                                           isProbed
                                               ? ViewportTheme::colors.clockConnectionHigh
                                               : ViewportTheme::colors.text,
                                           PickingId{m_runtimeId, 0});

        if (m_probedSlotUuid != UUID::null) {
            const auto &comp = sceneState.getComponentByUuid<SlotSceneComponent>(m_probedSlotUuid);
            if (!comp) {
                return;
            }
            auto endPos = comp->getConnectionPos(sceneState);

            // This looks awesome, just hit and trial :)
            const glm::vec2 ctrl1 = glm::mix(glm::vec2(startPos.x, startPos.y),
                                             glm::vec2(endPos.x, startPos.y),
                                             0.25f);
            const glm::vec2 ctrl2 = glm::mix(glm::vec2(endPos.x, startPos.y),
                                             glm::vec2(endPos.x, endPos.y),
                                             0.75f);

            const auto &color = !m_probeData.empty() &&
                                        m_probeData.back().second == SimEngine::LogicState::high
                                    ? ViewportTheme::colors.stateHigh
                                    : ViewportTheme::colors.stateLow;

            context.pathRenderer->beginPathMode({startPos.x - (textSize.x / 2.f), startPos.y, 0.51},
                                                1.f,
                                                color,
                                                PickingId{m_runtimeId, 1});

            endPos.z = 0.51f;
            context.pathRenderer->pathCubicBeizerTo(endPos,
                                                    ctrl1,
                                                    ctrl2,
                                                    1.f,
                                                    color,
                                                    PickingId{m_runtimeId, 1});

            context.pathRenderer->endPathMode();
        }
    }

    void SlotProbeSceneComponent::update(TimeMs frameTime, SceneState &state) {
        if (m_probedSlotUuid == UUID::null)
            return;

        if (m_unsubscribeFlag) {
            m_unsubscribeFlag = false;
            unsubscribeFromSlot(state);
        }

        if (m_subscribeFlag) {
            m_subscribeFlag = false;
            subscribeToSlot(state, m_probedSlotUuid);
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
        m_subscribeFlag = true;
    }

    void SlotProbeSceneComponent::onAttach(SceneState &state) {
        auto &mainPageState = Pages::MainPage::getInstance()->getState();
        mainPageState.getProbes().insert(getUuid());
    }

    std::vector<UUID> SlotProbeSceneComponent::cleanup(SceneState &state, UUID caller) {
        auto &mainPageState = Pages::MainPage::getInstance()->getState();
        mainPageState.getProbes().erase(getUuid());
        return NonSimSceneComponent::cleanup(state, caller);
    }

    SlotProbeSceneComponent::SlotProbeSceneComponent() {
        m_name = "Slot Probe";
    }
    std::type_index SlotProbeSceneComponent::getTypeIndex() {
        return typeid(SlotProbeSceneComponent);
    }

    void SlotProbeSceneComponent::onNameChanged() {
        m_scaleDirty = true;
    }

    void SlotProbeSceneComponent::drawPropertiesUI(SceneState& sceneState) {
        // render 20 most recent probe data entries in imgui table
        ImGui::Text("Probed Slot: %s",
                    m_probedSlotUuid != UUID::null
                        ? m_probedSlotUuid.toString().c_str()
                        : "None");
        if (ImGui::BeginTable("ProbeDataTable", 2, ImGuiTableFlags_Borders)) {
            ImGui::TableSetupColumn("Time");
            ImGui::TableSetupColumn("State");
            ImGui::TableHeadersRow();
            for (auto it = m_probeData.rbegin();
                 it != m_probeData.rend() &&
                 std::distance(m_probeData.rbegin(), it) < 20;
                 ++it) {
                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0);
                ImGui::Text("%.3f s", std::chrono::duration<float>(it->first).count());
                ImGui::TableSetColumnIndex(1);
                ImGui::Text("%s", it->second == SimEngine::LogicState::high ? "High" : "Low");
            }
            ImGui::EndTable();
        }
    }

    void SlotProbeSceneComponent::subscribeToSlot(const SceneState &sceneState, const UUID &slotUuid) {
        const auto &comp = sceneState.getComponentByUuid<SlotSceneComponent>(slotUuid);
        if (!comp)
            return;

        const auto &simId = sceneState.getComponentByUuid<SimulationSceneComponent>(
                                          comp->getParentComponent())
                                ->getSimEngineId();
        const auto &digComp = SimEngine::SimulationEngine::instance().getDigitalComponent(simId);
        digComp->addOnStateChangeCB(m_uuid, [this, slotComp = comp](const SimEngine::ComponentState &oldState,
                                                                    const SimEngine::ComponentState &newState) {
            SimEngine::SlotState slotState;

            if (slotComp->isInputSlot()) {
                slotState = newState.inputStates[slotComp->getIndex()];
            } else {
                slotState = newState.outputStates[slotComp->getIndex()];
            }

            if (m_probeData.empty()) {
                m_probeData.emplace_back(slotState.lastChangeTime,
                                         slotState.state);
            } else {
                auto &lastEntry = m_probeData.back();
                if (slotState.state != lastEntry.second) {
                    m_probeData.emplace_back(slotState.lastChangeTime,
                                             slotState.state);
                }
            }
        });
    }

    void SlotProbeSceneComponent::unsubscribeFromSlot(const SceneState &sceneState) {
        if (m_unsubscribeSlotUuid == UUID::null)
            return;

        const auto &comp = sceneState.getComponentByUuid<SlotSceneComponent>(m_unsubscribeSlotUuid);
        if (!comp)
            return;

        const auto &simId = sceneState.getComponentByUuid<SimulationSceneComponent>(
                                          comp->getParentComponent())
                                ->getSimEngineId();
        const auto &digComp = SimEngine::SimulationEngine::instance().getDigitalComponent(simId);
        digComp->removeOnStateChangeCB(m_uuid);
    }

    void SlotProbeSceneComponent::onBeforeProbedSlotChanged() {
        m_unsubscribeFlag = true;
        m_unsubscribeSlotUuid = m_probedSlotUuid;
    }
} // namespace Bess::Canvas
