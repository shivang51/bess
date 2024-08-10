#include "ui/component_explorer.h"

#include "imgui.h"
#include "components_manager/component_bank.h"
#include "components_manager/components_manager.h"

namespace Bess::UI {
    
    bool ComponentExplorer::isfirstTimeDraw = false;
    std::string ComponentExplorer::m_searchQuery = "";

    int InputTextCallback(ImGuiInputTextCallbackData* data)
    {
        if (data->EventFlag == ImGuiInputTextFlags_CallbackResize)
        {
            std::string* str = (std::string*)data->UserData;
            str->resize(data->BufTextLen);
            data->Buf = (char*)str->c_str();
        }
        return 0;
    }

    static std::string toLowerCase(const std::string& str) {
        std::string data = str;
        std::transform(data.begin(), data.end(), data.begin(),
            [](unsigned char c) { return std::tolower(c); });
        return data;
    }

    void ComponentExplorer::draw()
    {
        if (isfirstTimeDraw) firstTime();

        ImGui::Begin("Component Explorer");
        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 4);
        
        {
            ImGui::PushItemWidth(-1);
            if (ImGui::InputTextWithHint("##Search", "Search", m_searchQuery.data(), m_searchQuery.capacity() + 1, ImGuiInputTextFlags_CallbackResize, InputTextCallback, (void*)&m_searchQuery)) {
                m_searchQuery = toLowerCase(m_searchQuery);
            }
            ImGui::PopItemWidth();
        }

        auto& vault = Simulator::ComponentBank::getVault();

        for (auto& ent : vault) {
            if (ImGui::TreeNode(ent.first.c_str())) {
                for (auto& comp : ent.second) {
                    auto& name = comp.getName();
                    if (m_searchQuery != "" && toLowerCase(name).find(m_searchQuery) == std::string::npos) continue;

                    if (ImGui::Button(name.c_str(), { -1, 0 })) {
                        auto pos = glm::vec3(0.f);
                        std::any data = NULL;
                        if (comp.getType() == Simulator::ComponentType::jcomponent) {
                            data = comp.getJCompData();
                        }
                        Simulator::ComponentsManager::generateComponent(comp.getType(), data, pos);
                    }
                }
                ImGui::TreePop();
            }
        }

        ImGui::PopStyleVar();
        ImGui::End();
    }

    void ComponentExplorer::firstTime()
    {
        if (!isfirstTimeDraw) return;
        isfirstTimeDraw = false;
    }
}