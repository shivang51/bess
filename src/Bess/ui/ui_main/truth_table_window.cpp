#include "truth_table_window.h"
#include "imgui.h"
#include "ui/ui_main/project_explorer.h"
#include "ui/widgets/m_widgets.h"

#include "simulation_engine.h"

namespace Bess::UI {

    std::string TruthTableWindow::selectedNetName;
    UUID TruthTableWindow::selectedNetId = UUID::null;
    bool TruthTableWindow::isDirty = true;
    std::vector<std::vector<Bess::SimEngine::LogicState>> TruthTableWindow::currentTruthTable;

    void TruthTableWindow::draw() {
        if (!isShown)
            return;

        static constexpr auto tableFlags = ImGuiTableFlags_Borders |
                                           ImGuiTableFlags_RowBg |
                                           ImGuiTableFlags_SizingStretchProp |
                                           ImGuiTableFlags_Resizable;

        const auto &state = ProjectExplorer::state;

        ImGui::Begin(windowName, nullptr, ImGuiWindowFlags_NoFocusOnAppearing);

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
                isDirty = false;
            }

            if (!currentTruthTable.empty()) {
                if (ImGui::BeginTable("table1", currentTruthTable[0].size(), tableFlags)) {
                    for (int i = 0; i < currentTruthTable[0].size(); i++) {
                        ImGui::TableSetupColumn(("Col " + std::to_string(i)).c_str());
                    }
                    ImGui::TableHeadersRow();
                    for (int row = 0; row < currentTruthTable.size(); row++) {
                        for (int column = 0; column < currentTruthTable[0].size(); column++) {
                            ImGui::TableNextColumn();
                            const auto &state = currentTruthTable[row][column];
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
