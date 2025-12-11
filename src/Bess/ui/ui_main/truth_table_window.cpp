#include "truth_table_window.h"
#include "imgui.h"
#include "ui/ui_main/project_explorer.h"
#include "ui/widgets/m_widgets.h"

namespace Bess::UI {

    std::string TruthTableWindow::selectedNetName;
    UUID TruthTableWindow::selectedNetId = UUID::null;

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
                    break;
                }
            }
        }

        if (selectedNetId != UUID::null) {
            if (ImGui::BeginTable("table1", 3, tableFlags)) {
                ImGui::TableSetupColumn("Column 0");
                ImGui::TableSetupColumn("Column 1");
                ImGui::TableSetupColumn("Column 2");
                ImGui::TableHeadersRow();
                for (int row = 0; row < 4; row++) {
                    for (int column = 0; column < 3; column++) {
                        ImGui::TableNextColumn();
                        ImGui::Text("Row %d Column %d", row, column);
                    }
                }
                ImGui::EndTable();
            }
        } else {
            ImGui::Text("Please select a net to view its truth table.");
        }

        ImGui::End();
    }

    bool TruthTableWindow::isShown = false;
    bool TruthTableWindow::isfirstTimeDraw = true;
} // namespace Bess::UI
