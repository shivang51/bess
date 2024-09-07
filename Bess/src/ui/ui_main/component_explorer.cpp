#include "ui/ui_main/component_explorer.h"

#include "common/helpers.h"
#include "components_manager/component_bank.h"
#include "components_manager/components_manager.h"
#include "imgui.h"
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

        auto &vault = Simulator::ComponentBank::getVault();

        for (auto &ent : vault) {
            if (ImGui::TreeNode(ent.first.c_str())) {
                for (auto &comp : ent.second) {
                    auto &name = comp.getName();
                    if (m_searchQuery != "" && Common::Helpers::toLowerCase(name).find(m_searchQuery) == std::string::npos)
                        continue;

                    if (ImGui::Button(name.c_str(), {-1, 0})) {
                        auto pos = Pages::MainPage::getTypedInstance()->getCameraPos();
                        std::any data = NULL;
                        if (comp.getType() == Simulator::ComponentType::jcomponent) {
                            data = comp.getJCompData();
                        }
                        Simulator::ComponentsManager::generateComponent(comp.getType(), data, {pos, 0.f});
                        Pages::MainPageState::getInstance()->setPrevGenBankElement(comp);
                    }
                }
                ImGui::TreePop();
            }
        }

        ImGui::PopStyleVar();
        ImGui::End();
    }

    void ComponentExplorer::firstTime() {
        if (!isfirstTimeDraw)
            return;
        isfirstTimeDraw = false;
    }
} // namespace Bess::UI
