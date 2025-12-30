#include "truth_table_window.h"
#include "digital_component.h"
#include "imgui.h"
#include "scene/components/components.h"
#include "scene/scene.h"
#include "simulation_engine.h"
#include "types.h"
#include "ui/ui_main/project_explorer.h"
#include "ui/widgets/m_widgets.h"

namespace Bess::UI {

    std::string TruthTableWindow::selectedNetName;
    UUID TruthTableWindow::selectedNetId = UUID::null;
    bool TruthTableWindow::isDirty = true;
    SimEngine::TruthTable TruthTableWindow::currentTruthTable;
    std::unordered_map<UUID, std::string> TruthTableWindow::compIdToNameMap;

    void TruthTableWindow::draw() {
        if (!isShown)
            return;

        static constexpr auto tableFlags = ImGuiTableFlags_Borders |
                                           ImGuiTableFlags_RowBg |
                                           ImGuiTableFlags_SizingStretchProp |
                                           ImGuiTableFlags_Resizable;

        const auto &state = ProjectExplorer::state;

        ImGui::Begin(windowName.data(), nullptr, ImGuiWindowFlags_NoFocusOnAppearing);

        if (Widgets::ComboBox("Select Net",
                              selectedNetName,
                              state.netIdToNameMap | std::views::values)) {
            for (const auto &[netId, netName] : state.netIdToNameMap) {
                if (netName == selectedNetName) {
                    selectedNetId = netId;
                    isDirty = true;
                    break;
                }
            }
        }

        if (selectedNetId != UUID::null) {
            if (isDirty) {
                auto &simEngine = SimEngine::SimulationEngine::instance();
                currentTruthTable = simEngine.getTruthTableOfNet(selectedNetId);
                const auto &scene = Bess::Canvas::Scene::instance();
                // const auto &reg = scene->getEnttRegistry();

                // auto viewInp = reg.view<Canvas::Components::TagComponent,
                //                         Canvas::Components::SimulationComponent,
                //                         Canvas::Components::SimulationInputComponent>();
                //
                // viewInp.each([&](auto entity, const Canvas::Components::TagComponent &tagComp,
                //                  const Canvas::Components::SimulationComponent &simComp,
                //                  const Canvas::Components::SimulationInputComponent &simInputComp) {
                //     compIdToNameMap[simComp.simEngineEntity] = tagComp.name;
                // });
                //
                // auto viewOut = reg.view<Canvas::Components::TagComponent,
                //                         Canvas::Components::SimulationComponent,
                //                         Canvas::Components::SimulationOutputComponent>();
                //
                // viewOut.each([&](auto entity, const Canvas::Components::TagComponent &tagComp,
                //                  const Canvas::Components::SimulationComponent &simComp,
                //                  const Canvas::Components::SimulationOutputComponent &simOutputComp) {
                //     compIdToNameMap[simComp.simEngineEntity] = tagComp.name;
                // });

                isDirty = false;
            }

            if (!currentTruthTable.table.empty()) {
                if (ImGui::BeginTable("TruthTable", currentTruthTable.table[0].size(), tableFlags)) {
                    for (auto &compId : currentTruthTable.inputUuids) {
                        ImGui::TableSetupColumn(std::format("{}", compIdToNameMap.at(compId)).c_str());
                    }

                    for (auto &compId : currentTruthTable.outputUuids) {
                        ImGui::TableSetupColumn(std::format("{}", compIdToNameMap.at(compId)).c_str());
                    }

                    ImGui::TableHeadersRow();
                    for (int row = 0; row < currentTruthTable.table.size(); row++) {
                        for (int column = 0; column < currentTruthTable.table[0].size(); column++) {
                            ImGui::TableNextColumn();
                            const auto &state = currentTruthTable.table[row][column];
                            int value = state == SimEngine::LogicState::low
                                            ? 0
                                            : (state == SimEngine::LogicState::high
                                                   ? 1
                                                   : (state == SimEngine::LogicState::unknown ? 'X' : 'Z'));
                            ImGui::Text("%d", value);
                        }
                    }
                    ImGui::EndTable();
                }
            }
        } else {
            ImGui::Text("Please select a net to view its truth table.");
        }

        ImGui::End();
    }

    bool TruthTableWindow::isShown = false;
    bool TruthTableWindow::isfirstTimeDraw = true;
} // namespace Bess::UI
