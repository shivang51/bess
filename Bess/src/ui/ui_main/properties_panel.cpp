#include "ui/ui_main/properties_panel.h"
#include "imgui_internal.h"
#include "scene/components/components.h"
#include "scene/scene.h"
#include "ui/m_widgets.h"
#include <imgui.h>

using namespace Bess::Canvas::Components;

namespace Bess::UI {

    void drawTagComponent(const TagComponent &comp) {
        ImGui::Text("%s", comp.name.c_str());
    }

    void drawSimulationOutputComponent(SimulationOutputComponent &comp) {
        ImGui::Checkbox("Record Ouput", &comp.recordOutput);
    }

    bool CheckboxWithLabel(const char *label, bool *value) {
        ImGui::Text("%s", label);
        auto style = ImGui::GetStyle();
        float availWidth = ImGui::GetContentRegionAvail().x;
        ImGui::SameLine();
        float checkboxWidth = ImGui::CalcTextSize("X").x + style.FramePadding.x;
        ImGui::SetCursorPosX(availWidth - checkboxWidth);
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(2, 2));
        bool changed = ImGui::Checkbox(("##" + std::string(label)).c_str(), value);
        ImGui::PopStyleVar();
        return changed;
    }

    bool MyCollapsingHeader(const char *label) {
        auto flags = ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_FramePadding;
        ImGui::PushStyleColor(ImGuiCol_Header, ImVec4(0, 0, 0, 0));
        bool opened = ImGui::CollapsingHeader(label, flags);
        ImGui::PopStyleColor();
        return opened;
    }

    void drawSimulationInputComponent(SimulationInputComponent &comp, const UUID &uuid) {
        if (MyCollapsingHeader("Input Behaviour")) {
            ImGui::Indent();
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
        ImGui::End();
    }
} // namespace Bess::UI
