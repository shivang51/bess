#include "ui/ui_main/component_explorer.h"

#include "common/helpers.h"
#include "component_catalog.h"
#include "imgui.h"
#include "imgui_internal.h"
#include "scene/scene.h"
#include "simulation_engine.h"
#include "ui/m_widgets.h"

namespace Bess::UI {

    bool ComponentExplorer::isfirstTimeDraw = false;
    std::string ComponentExplorer::m_searchQuery = "";

    void ComponentExplorer::draw() {
        if (isfirstTimeDraw)
            firstTime();

        ImGui::Begin("Component Explorer");
        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 4);

        {
            ImGui::PushItemWidth(-1);
            if (UI::MWidgets::TextBox("##Search", m_searchQuery, "Search")) {
                m_searchQuery = Common::Helpers::toLowerCase(m_searchQuery);
            }
            ImGui::PopItemWidth();
        }

        ImGui::PushStyleVar(ImGuiStyleVar_ButtonTextAlign, ImVec2(0.f, 0.5f));
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));
        ImGui::PushStyleColor(ImGuiCol_Header, ImVec4(0, 0, 0, 0)); // for tree node to have no bg normally
        static auto components = SimEngine::ComponentCatalog::instance().getComponentsTree();

        ImGuiTreeNodeFlags treeNodeFlags = ImGuiTreeNodeFlags_Framed;

        for (auto &ent : components) {
            if (ImGui::TreeNodeEx(ent.first.c_str(), treeNodeFlags)) {
                for (auto &comp : ent.second) {
                    auto &name = comp.name;
                    if (m_searchQuery != "" && Common::Helpers::toLowerCase(name).find(m_searchQuery) == std::string::npos)
                        continue;

                    if (ImGui::Button(name.c_str(), {-1, 0})) {
                        auto simEntt = SimEngine::SimulationEngine::instance().addComponent(comp.type);
                        Canvas::Scene::instance().createSimEntity(simEntt, comp.name, comp.inputCount, comp.outputCount);
                    }
                }
                ImGui::TreePop();
            }
        }

        ImGui::PopStyleColor(2);
        ImGui::PopStyleVar(2);
        ImGui::End();
    }

    void ComponentExplorer::firstTime() {
        if (!isfirstTimeDraw)
            return;
        isfirstTimeDraw = false;
    }
} // namespace Bess::UI
