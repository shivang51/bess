#include "scene/scene_state/components/input_scene_component.h"
#include "scene/scene_state/components/styles/sim_comp_style.h"
#include "types.h"
#include "ui/ui.h"

#include "simulation_engine.h"

namespace Bess::Canvas {
    InputSceneComponent::InputSceneComponent(UUID uuid) : SimulationSceneComponent(uuid) {}

    void InputSceneComponent::draw(SceneState &state,
                                   std::shared_ptr<Renderer::MaterialRenderer> materialRenderer,
                                   std::shared_ptr<Renderer2D::Vulkan::PathRenderer> pathRenderer) {

        if (m_isFirstDraw) {
            onFirstDraw(state, materialRenderer, pathRenderer);
        }

        SimulationSceneComponent::draw(state, materialRenderer, pathRenderer);

        int i = 0;
        for (const auto &slotUuid : m_outputSlots) {
            drawToggleButton(state, materialRenderer, pathRenderer, slotUuid, i++);
        }
    }

    void InputSceneComponent::drawToggleButton(SceneState &state,
                                               std::shared_ptr<Renderer::MaterialRenderer> materialRenderer,
                                               std::shared_ptr<Renderer2D::Vulkan::PathRenderer> pathRenderer,
                                               UUID slotUuid,
                                               int buttonIndex) {
        const auto slotComp = state.getComponentByUuid<SlotSceneComponent>(slotUuid);
        const auto slotPos = slotComp->getAbsolutePosition(state);

        const bool isHigh = slotComp->getSlotState(state).state == SimEngine::LogicState::high;

        constexpr float buttonWidth = 30.f;
        constexpr float buttonHeight = Styles::SIM_COMP_SLOT_ROW_SIZE - (Styles::simCompStyles.rowMargin * 2.f);
        constexpr Renderer::QuadRenderProperties buttonProps{.borderRadius = glm::vec4(4.f)};
        constexpr glm::vec2 buttonSize = glm::vec2(buttonWidth, buttonHeight);

        const float buttonPosX = m_transform.position.x - (m_transform.scale.x / 2.f) +
                                 Styles::simCompStyles.paddingX + (buttonSize.x / 2.f);
        const glm::vec3 buttonPos = glm::vec3(buttonPosX,
                                              slotPos.y,
                                              m_transform.position.z + 0.001f);

        const float buttonHeadPosX = isHigh
                                         ? buttonPosX + (buttonSize.x / 2.f) - (buttonSize.y / 2.f)
                                         : buttonPosX - (buttonSize.x / 2.f) + (buttonSize.y / 2.f);

        const glm::vec3 buttonHeadPos = glm::vec3(buttonHeadPosX,
                                                  slotPos.y,
                                                  m_transform.position.z + 0.001f);

        const auto pickingId = PickingId{m_runtimeId, static_cast<uint32_t>(buttonIndex + 1)};

        // Button background / track
        materialRenderer->drawQuad(buttonPos,
                                   buttonSize,
                                   isHigh ? ViewportTheme::colors.stateHigh : ViewportTheme::colors.background,
                                   pickingId,
                                   buttonProps);

        // Toggle head
        materialRenderer->drawQuad(buttonHeadPos,
                                   {buttonSize.y, buttonSize.y},
                                   ViewportTheme::colors.stateLow,
                                   pickingId,
                                   buttonProps);

        // Button label
        const std::string label = isHigh ? "ON" : "OFF";
        const auto textSize = materialRenderer->getTextRenderSize(label, Styles::simCompStyles.slotLabelSize);

        const float textPosX = buttonPos.x + (buttonSize.x / 2.f) + 8.f;
        const glm::vec3 textPos = glm::vec3(textPosX,
                                            slotPos.y + (textSize.y / 2.f) - 2.f, // FIXME: why -2.f, maybe the baseline?
                                            m_transform.position.z + 0.001f);

        materialRenderer->drawText(label,
                                   textPos,
                                   Styles::simCompStyles.slotLabelSize,
                                   ViewportTheme::colors.text,
                                   PickingId{m_runtimeId, 0});
    }

    void InputSceneComponent::onMouseHovered(const Events::MouseHoveredEvent &e) {
        int buttonIndex = (int)e.details - 1; // since 0 is component body

        if (buttonIndex < 0) {
            UI::setCursorNormal();
        } else {
            UI::setCursorPointer();
        }
    }

    void InputSceneComponent::onMouseEnter(const Events::MouseEnterEvent &e) {
        int buttonIndex = (int)e.details - 1; // since 0 is component body

        if (buttonIndex < 0) {
            UI::setCursorNormal();
        } else {
            UI::setCursorPointer();
        }
    }

    void InputSceneComponent::onMouseLeave(const Events::MouseLeaveEvent &e) {
        UI::setCursorNormal();
    }

    void InputSceneComponent::onMouseButton(const Events::MouseButtonEvent &e) {
        if (e.button != Events::MouseButton::left || e.action != Events::MouseClickAction::press) {
            return;
        }

        const int buttonIndex = (int)e.details - 1; // since 0 is component body

        if (buttonIndex < 0) {
            return;
        }

        const auto slotUuid = m_outputSlots[buttonIndex];
        const auto slotComp = e.sceneState->getComponentByUuid<SlotSceneComponent>(slotUuid);
        const auto slotParentComp = e.sceneState->getComponentByUuid<SimulationSceneComponent>(
            slotComp->getParentComponent());

        auto &simEngine = SimEngine::SimulationEngine::instance();

        simEngine.setOutputPinState(slotParentComp->getSimEngineId(),
                                    slotComp->getIndex(),
                                    slotComp->getSlotState(e.sceneState).state == SimEngine::LogicState::high
                                        ? SimEngine::LogicState::low
                                        : SimEngine::LogicState::high);
    }
} // namespace Bess::Canvas
