#include "truth_table_window.h"
#include "imgui.h"

namespace Bess::UI {
    void TruthTableWindow::draw() {
        if (!isShown)
            return;

        ImGui::Begin(windowName, nullptr, ImGuiWindowFlags_NoFocusOnAppearing);

        ImGui::Text("Truth Table Viewer");

        ImGui::End();
    }

    bool TruthTableWindow::isShown = false;
    bool TruthTableWindow::isfirstTimeDraw = true;
} // namespace Bess::UI
