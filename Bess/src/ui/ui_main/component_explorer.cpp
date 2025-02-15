#include "ui/ui_main/component_explorer.h"

#include "common/helpers.h"
#include "components_manager/component_bank.h"
#include "imgui.h"
#include "imgui_internal.h"
#include "pages/main_page/main_page.h"
#include "pages/main_page/main_page_state.h"
#include "ui/m_widgets.h"
#include "ui/ui_main/ui_main.h"

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
        auto &vault = Simulator::ComponentBank::getVault();

        ImGuiTreeNodeFlags treeNodeFlags = ImGuiTreeNodeFlags_Framed;

        for (auto &ent : vault) {
            if (ImGui::TreeNodeEx(ent.first.c_str(), treeNodeFlags)) {
                for (auto &comp : ent.second) {
                    auto &name = comp.getName();
                    if (m_searchQuery != "" && Common::Helpers::toLowerCase(name).find(m_searchQuery) == std::string::npos)
                        continue;

                    if (ImGui::Button(name.c_str(), {-1, 0})) {
                        // auto pos = Pages::MainPage::getTypedInstance()->getCameraPos();
                        std::any data = NULL;
                        if (comp.getType() == Simulator::ComponentType::jcomponent) {
                            data = comp.getJCompData();
                        } else if (comp.getType() == Simulator::ComponentType::flipFlop) {
                            data = name;
                        }
                        // Simulator::ComponentsManager::generateComponent(comp.getType(), data, {pos, 0.f});
                        Pages::MainPageState::getInstance()->setPrevGenBankElement(comp);
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
