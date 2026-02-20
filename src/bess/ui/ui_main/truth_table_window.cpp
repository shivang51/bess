#include "truth_table_window.h"
#include "imgui.h"
#include "pages/main_page/main_page.h"
#include "simulation_engine.h"
#include "types.h"
#include "ui/widgets/m_widgets.h"

namespace Bess::UI {

    void TruthTableWindow::draw() {
        if (!isShown)
            return;

        static constexpr auto tableFlags = ImGuiTableFlags_Borders |
                                           ImGuiTableFlags_RowBg |
                                           ImGuiTableFlags_SizingStretchProp |
                                           ImGuiTableFlags_Resizable |
                                           ImGuiTableFlags_Reorderable;

        ImGui::Begin(windowName.data(), nullptr, ImGuiWindowFlags_NoFocusOnAppearing);

        const auto &mainPageState = Pages::MainPage::getTypedInstance()->getState();

        if (Widgets::ComboBox("Select Net",
                              selectedNetName,
                              mainPageState.getNetIdToNameMap() | std::views::values)) {
            for (const auto &[netId, netName] : mainPageState.getNetIdToNameMap()) {
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

            if (!currentTruthTable.table.empty()) {
                char inp = 'A';
                char out = 'M';
                if (ImGui::BeginTable("TruthTable", (int)currentTruthTable.table[0].size(), tableFlags)) {
                    for (auto &compId : currentTruthTable.inputUuids) {
                        ImGui::TableSetupColumn(std::format("{}", inp++).c_str());
                    }

                    for (auto &compId : currentTruthTable.outputUuids) {
                        ImGui::TableSetupColumn(std::format("{}", out++).c_str());
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
    std::string *TruthTableWindow::selectedNetName = nullptr;
    UUID TruthTableWindow::selectedNetId = UUID::null;
    bool TruthTableWindow::isDirty = true;
    SimEngine::TruthTable TruthTableWindow::currentTruthTable;
    std::unordered_map<UUID, std::string> TruthTableWindow::compIdToNameMap;

} // namespace Bess::UI
