#include "input_scene_component.h"
#include "scene/scene_state/components/styles/sim_comp_style.h"
#include "scene/scene_state/scene_state.h"
#include "scene/scene_ui/scene_ui.h"
#include "settings/viewport_theme.h"
#include "sim_scene_component.h"
#include "types.h"
#include "ui/ui.h"

#include "simulation_engine.h"

namespace Bess::Canvas {

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
                                               const std::shared_ptr<Renderer::MaterialRenderer> &materialRenderer,
                                               const std::shared_ptr<Renderer2D::Vulkan::PathRenderer> &pathRenderer,
                                               UUID slotUuid,
                                               int buttonIndex) {
        constexpr float buttonWidth = 30.f;
        constexpr float buttonHeight = Styles::SIM_COMP_SLOT_ROW_SIZE - (Styles::simCompStyles.rowMargin * 2.f);
        constexpr glm::vec2 buttonSize = glm::vec2(buttonWidth, buttonHeight);

        const auto slotComp = state.getComponentByUuid<SlotSceneComponent>(slotUuid);
        const auto slotType = slotComp->getSlotType();

        if (slotType == SlotType::inputsResize ||
            slotType == SlotType::outputsResize) {
            return;
        }

        const auto slotPosY = slotComp->getAbsolutePosition(state).y;
        const bool isHigh = slotComp->getSlotState(state).state == SimEngine::LogicState::high;

        const float buttonPosX = m_transform.position.x - (m_transform.scale.x / 2.f) +
                                 Styles::simCompStyles.paddingX + (buttonSize.x / 2.f);
        const glm::vec3 buttonPos = glm::vec3(buttonPosX,
                                              slotPosY,
                                              m_transform.position.z + 0.001f);

        const auto pickingId = PickingId{m_runtimeId, static_cast<uint32_t>(buttonIndex + 1)};

        SceneUI::drawToggleButton(pickingId, isHigh, buttonPos, buttonSize, materialRenderer, pathRenderer);

        // Button label
        const std::string label = isHigh ? "1" : "0";
        const auto textSize = materialRenderer->getTextRenderSize(label, Styles::simCompStyles.slotLabelSize);

        const float textPosX = buttonPos.x + (buttonSize.x / 2.f) + 8.f;
        const glm::vec3 textPos = glm::vec3(textPosX,
                                            slotPosY + (textSize.y / 2.f) - 1.f, // FIXME: why -2.f, maybe the baseline?
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

        simEngine.setOutputSlotState(slotParentComp->getSimEngineId(),
                                     slotComp->getIndex(),
                                     slotComp->getSlotState(*e.sceneState).state == SimEngine::LogicState::high
                                         ? SimEngine::LogicState::low
                                         : SimEngine::LogicState::high);
    }

    void InputSceneComponent::calculateSchematicScale(SceneState &state,
                                                      const std::shared_ptr<Renderer::MaterialRenderer> &materialRenderer) {
        SimulationSceneComponent::calculateSchematicScale(state, materialRenderer);
        m_schematicTransform.scale.x = 50.f;
    }

} // namespace Bess::Canvas
