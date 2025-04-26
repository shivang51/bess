#include "ui/ui_main/properties_panel.h"
#include "common/helpers.h"
#include "gtc/type_ptr.hpp"
#include "imgui_internal.h"
#include "scene/components/components.h"
#include "scene/scene.h"
#include "ui/icons/FontAwesomeIcons.h"
#include "ui/m_widgets.h"
#include <imgui.h>

using namespace Bess::Canvas::Components;

namespace Bess::UI {

    void drawTagComponent(TagComponent &comp) {
        auto icon = Common::Helpers::getComponentIcon(comp.type);
        MWidgets::TextBox("Name", comp.name);
    }

    bool CheckboxWithLabel(const char *label, bool *value) {
        ImGui::Text("%s", label);
        auto style = ImGui::GetStyle();
        float availWidth = ImGui::GetContentRegionAvail().x;
        ImGui::SameLine();
        float checkboxWidth = ImGui::CalcTextSize("X").x + style.FramePadding.x;
        ImGui::SetCursorPosX(availWidth - checkboxWidth);
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(0, 0));
        bool changed = ImGui::Checkbox(("##" + std::string(label)).c_str(), value);
        ImGui::PopStyleVar();
        return changed;
    }

    bool MyCollapsingHeader(const char *label) {
        ImGuiContext &g = *ImGui::GetCurrentContext();
        ImGuiWindow *window = g.CurrentWindow;

        ImGuiID id = window->GetID(label);
        ImVec2 pos = window->DC.CursorPos;
        ImRect bb(pos, ImVec2(pos.x + ImGui::GetContentRegionAvail().x, pos.y + g.FontSize + g.Style.FramePadding.y * 2));
        bool opened = ImGui::TreeNodeBehaviorIsOpen(id, ImGuiTreeNodeFlags_DefaultOpen);
        bool hovered, held;

        auto style = ImGui::GetStyle();
        auto rounding = style.FrameRounding;

        if (ImGui::ButtonBehavior(bb, id, &hovered, &held, ImGuiButtonFlags_PressedOnClick))
            window->DC.StateStorage->SetInt(id, opened ? 0 : 1);
        if (hovered || held)
            window->DrawList->AddRectFilled(bb.Min, bb.Max, ImGui::GetColorU32(held ? ImGuiCol_HeaderActive : ImGuiCol_HeaderHovered), rounding);

        // Icon, text
        float button_sz = g.FontSize;
        pos.x += rounding / 2.f;
        auto icon = opened ? Icons::FontAwesomeIcons::FA_CARET_DOWN : Icons::FontAwesomeIcons::FA_CARET_RIGHT;
        ImGui::RenderText(ImVec2(pos.x + g.Style.ItemInnerSpacing.x, pos.y + g.Style.FramePadding.y), icon);
        ImGui::RenderText(ImVec2(pos.x + button_sz + g.Style.ItemInnerSpacing.x, pos.y + g.Style.FramePadding.y), label);

        ImGui::ItemSize(bb, g.Style.FramePadding.y);
        ImGui::ItemAdd(bb, id);

        if (opened) {
            ImGui::Indent();
        }
        return opened;
    }

    void drawSimulationOutputComponent(SimulationOutputComponent &comp) {
        ImGui::Spacing();
        CheckboxWithLabel("Record Ouput", &comp.recordOutput);
    }

    void drawSimulationInputComponent(SimulationInputComponent &comp, const UUID &uuid) {
        if (MyCollapsingHeader("Input Behaviour")) {
            if (CheckboxWithLabel("Clocked", &comp.clockBhaviour)) {
                comp.updateClock(uuid);
            }
            if (comp.clockBhaviour) {

                if (ImGui::SliderFloat("Frequency", &comp.frequency, 0.1f, 3.0f, "%.1f Hz", ImGuiSliderFlags_AlwaysClamp)) {
                    float stepSize = 0.1f;
                    comp.frequency = roundf(comp.frequency / stepSize) * stepSize; // Force step increments
                    comp.updateClock(uuid);
                }

                static std::vector<std::string> frequencies = {"Hz", "kHz", "MHz"};
                std::string currFreq = frequencies[(int)comp.frequencyUnit];
                if (UI::MWidgets::ComboBox("Unit", currFreq, frequencies)) {
                    auto idx = std::distance(frequencies.begin(), std::find(frequencies.begin(), frequencies.end(), currFreq));
                    comp.frequencyUnit = static_cast<SimEngine::FrequencyUnit>(idx);
                    comp.updateClock(uuid);
                }
            }
            ImGui::Unindent();
        }
    }

    void drawSpriteComponent(SpriteComponent &comp) {
        ImGui::Indent();
        ImGui::ColorEdit4("Color", glm::value_ptr(comp.color));
        ImGui::Unindent();
    }

    void PropertiesPanel::draw() {
        ImGui::Begin("Properties");

        auto &registry = Canvas::Scene::instance().getEnttRegistry();
        auto view = registry.view<SelectedComponent>();

        if (view.size() == 0) {
            ImGui::Text("No Component Selected");
            ImGui::End();
            return;
        }

        auto entt = view.front();
        if (!registry.valid(entt)) {
            ImGui::End();
            return;
        }

        if (registry.all_of<TagComponent>(entt)) {
            drawTagComponent(registry.get<TagComponent>(entt));
        }

        if (registry.all_of<SimulationOutputComponent>(entt)) {
            drawSimulationOutputComponent(registry.get<SimulationOutputComponent>(entt));
        }

        if (registry.all_of<SimulationInputComponent>(entt)) {
            auto simulationComp = registry.get<SimulationComponent>(entt);
            drawSimulationInputComponent(registry.get<SimulationInputComponent>(entt), simulationComp.simEngineEntity);
        }

        if (registry.all_of<ConnectionComponent>(entt)) {
            auto &connectionComponent = registry.get<ConnectionComponent>(entt);
            CheckboxWithLabel("Use Custom Color", &connectionComponent.useCustomColor);
            if (connectionComponent.useCustomColor) {
                drawSpriteComponent(registry.get<SpriteComponent>(entt));
            }
        }

        ImGui::End();
    }
} // namespace Bess::UI
