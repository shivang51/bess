#include "debug_panel.h"
#include "icons/CodIcons.h"
#include "imgui.h"
#include "imgui_internal.h"
#include "pages/main_page/main_page.h"
#include "widgets/m_widgets.h"

namespace Bess::UI {
    DebugPanel::DebugPanel() : Panel("debug_panel", "Debug Panel") {
        m_title = Icons::CodIcons::DEBUG_ALT + std::string(" Debug Panel");
    }

    void DebugPanel::render() {
        auto &mainPageState = Pages::MainPage::getInstance()->getState();

        auto &sceneDriver = mainPageState.getSceneDriver();
        const auto &sceneState = sceneDriver->getState();

        ImGui::Begin("Debug Window");

        ImGui::Text("Active Scene: %lu", sceneDriver.getActiveSceneIdx());

        ImGui::SameLine();
        ImGui::SeparatorEx(ImGuiSeparatorFlags_Vertical);
        ImGui::SameLine();

        ImGui::Text("Scene Count: %lu", sceneDriver.getSceneCount());

        ImGui::SameLine();
        if (ImGui::Button("Prev-Scene")) {
            size_t activeScene = sceneDriver.getActiveSceneIdx();
            if (activeScene > 0) {
                sceneDriver.setActiveScene(activeScene - 1);
            }
        }

        ImGui::SameLine();

        if (ImGui::Button("Next-Scene")) {
            size_t activeScene = sceneDriver.getActiveSceneIdx();
            if (activeScene < sceneDriver.getSceneCount() - 1) {
                sceneDriver.setActiveScene(activeScene + 1);
            }
        }

        if (Widgets::TreeNode(0, "Selected components")) {
            const auto &selComps = sceneState.getSelectedComponents();
            ImGui::Indent();
            for (const auto &[compId, selected] : selComps) {
                ImGui::BulletText("%lu", (uint64_t)compId);
            }
            ImGui::Unindent();
            ImGui::TreePop();
        }

        if (Widgets::TreeNode(1, "Connections")) {
            ImGui::Indent();
            const auto &connections = sceneState.getAllComponentConnections();
            for (const auto &[compId, connIds] : connections) {

                ImGui::Text("Component %lu", (uint64_t)compId);
                ImGui::Indent();
                for (const auto &connId : connIds) {
                    ImGui::BulletText("%lu", (uint64_t)connId);
                }
                ImGui::Unindent();
            }
            ImGui::Unindent();
            ImGui::TreePop();
        }

        if (Widgets::TreeNode(2, "First Sim Component Serilaized")) {
            const auto &selComps = sceneState.getSelectedComponents();
            if (!selComps.empty()) {
                const auto &compId = selComps.begin()->first;
                const auto &comp = sceneState.getComponentByUuid(compId);

                const auto &j = comp->toJson();
                ImGui::TextWrapped("%s", j.toStyledString().c_str());
            }
            ImGui::TreePop();
        }

        ImGui::End();
    }
} // namespace Bess::UI
