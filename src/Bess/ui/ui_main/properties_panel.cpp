#include "ui/ui_main/properties_panel.h"
#include "gtc/type_ptr.hpp"
#include "imgui_internal.h"
#include "init_components.h"
#include "scene/scene.h"
#include "scene/scene_state/components/connection_scene_component.h"
#include "scene/scene_state/components/scene_component_types.h"
#include "scene/scene_state/components/sim_scene_component.h"
#include "simulation_engine.h"
#include "ui/icons/FontAwesomeIcons.h"
#include "ui/widgets/m_widgets.h"
#include <imgui.h>

#include <algorithm>

using namespace Bess::Canvas::Components;

namespace Bess::UI {
    bool PropertiesPanel::isShown = true;

    bool MyCollapsingHeader(const char *label) {
        const ImGuiContext &g = *ImGui::GetCurrentContext();
        ImGuiWindow *window = g.CurrentWindow;

        const ImGuiID id = window->GetID(label);
        ImVec2 pos = window->DC.CursorPos;
        const ImRect bb(pos, ImVec2(pos.x + ImGui::GetContentRegionAvail().x, pos.y + g.FontSize + g.Style.FramePadding.y * 2));
        // bool opened = ImGui::TreeNodeBehaviorIsOpen(id, ImGuiTreeNodeFlags_DefaultOpen);
        const bool opened = true;
        bool hovered, held;

        const auto style = ImGui::GetStyle();
        const auto rounding = style.FrameRounding;

        if (ImGui::ButtonBehavior(bb, id, &hovered, &held, ImGuiButtonFlags_PressedOnClick))
            window->DC.StateStorage->SetInt(id, opened ? 0 : 1);
        if (hovered || held)
            window->DrawList->AddRectFilled(bb.Min, bb.Max, ImGui::GetColorU32(held ? ImGuiCol_HeaderActive : ImGuiCol_HeaderHovered), rounding);

        // Icon, text
        const float button_sz = g.FontSize;
        pos.x += rounding / 2.f;
        const auto icon = opened ? Icons::FontAwesomeIcons::FA_CARET_DOWN : Icons::FontAwesomeIcons::FA_CARET_RIGHT;
        ImGui::RenderText(ImVec2(pos.x + g.Style.ItemInnerSpacing.x, pos.y + g.Style.FramePadding.y), icon);
        ImGui::RenderText(ImVec2(pos.x + button_sz + g.Style.ItemInnerSpacing.x, pos.y + g.Style.FramePadding.y), label);

        ImGui::ItemSize(bb, g.Style.FramePadding.y);
        ImGui::ItemAdd(bb, id);

        if (opened) {
            ImGui::Indent();
        }
        return opened;
    }

    void drawClockTrait(const std::shared_ptr<SimEngine::ClockTrait> &trait, const UUID &uuid) {
        if (MyCollapsingHeader("Input Behaviour")) {
            if (ImGui::SliderFloat("Frequency", &trait->frequency, 0.1f, 3.0f, "%.1f Hz", ImGuiSliderFlags_AlwaysClamp)) {
                const float stepSize = 0.1f;
                trait->frequency = roundf(trait->frequency / stepSize) * stepSize; // Force step increments
            }

            static std::vector<std::string> frequencies = {"Hz", "kHz", "MHz"};
            std::string currFreq = frequencies[(int)trait->frequencyUnit];
            if (UI::Widgets::ComboBox("Unit", currFreq, frequencies)) {
                auto idx = std::distance(frequencies.begin(), std::ranges::find(frequencies, currFreq));
                trait->frequencyUnit = static_cast<SimEngine::FrequencyUnit>(idx);
            }
            ImGui::Unindent();
        }
    }

    void drawConnectionComponent(const std::shared_ptr<Canvas::ConnectionSceneComponent> &comp) {
        Widgets::CheckboxWithLabel("Use Custom Color", &comp->getUseCustomColor());
        if (comp->getUseCustomColor()) {
            auto &style = comp->getStyle();
            ImGui::ColorEdit4("Color", glm::value_ptr(style.color));
        }
    }

    void PropertiesPanel::draw() {
        if (!isShown)
            return;

        ImGui::Begin(windowName.data(), nullptr, ImGuiWindowFlags_NoFocusOnAppearing);
        auto &sceneState = Canvas::Scene::instance()->getState();
        if (sceneState.getSelectedComponents().empty()) {
            ImGui::TextUnformatted("No component selected.");
            ImGui::End();
            return;
        }

        // for now only showing first selected component's properties
        const UUID &compId = sceneState.getSelectedComponents().begin()->first;
        auto comp = sceneState.getComponentByUuid(compId);
        const auto compType = comp->getType();

        if (!comp->getName().empty()) {
            ImGui::TextWrapped("%s", comp->getName().c_str());
        }

        Widgets::TextBox("Name", comp->getName());

        if (compType == Canvas::SceneComponentType::simulation) {
            auto simComp = comp->cast<Canvas::SimulationSceneComponent>();
            auto &simEngine = SimEngine::SimulationEngine::instance();
            auto &def = simEngine.getComponentDefinition(simComp->getSimEngineId());

            if (def->hasTrait<SimEngine::ClockTrait>()) {
                drawClockTrait(def->getTrait<SimEngine::ClockTrait>(), compId);
            }
        } else if (compType == Canvas::SceneComponentType::connection) {
            auto connComp = comp->cast<Canvas::ConnectionSceneComponent>();
            drawConnectionComponent(connComp);
        }

        ImGui::End();
    }
} // namespace Bess::UI
