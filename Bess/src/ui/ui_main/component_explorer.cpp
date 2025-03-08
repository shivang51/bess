#include "ui/ui_main/component_explorer.h"

#include "common/helpers.h"
#include "component_catalog.h"
#include "imgui.h"
#include "imgui_internal.h"
#include "scene/scene.h"
#include "simulation_engine.h"
#include "ui/icons/ComponentIcons.h"
#include "ui/icons/FontAwesomeIcons.h"
#include "ui/icons/MaterialIcons.h"
#include "ui/m_widgets.h"

namespace Bess::UI {

    bool ComponentExplorer::isfirstTimeDraw = false;
    std::string ComponentExplorer::m_searchQuery = "";
    std::string ComponentExplorer::windowName = (std::string(Icons::MaterialIcons::APPS) + " Component Explorer");

    bool MyTreeNode(const char *label) {
        ImGuiContext &g = *ImGui::GetCurrentContext();
        ImGuiWindow *window = g.CurrentWindow;

        ImGuiID id = window->GetID(label);
        ImVec2 pos = window->DC.CursorPos;
        ImRect bb(pos, ImVec2(pos.x + ImGui::GetContentRegionAvail().x, pos.y + g.FontSize + g.Style.FramePadding.y * 2));
        bool opened = ImGui::TreeNodeBehaviorIsOpen(id, ImGuiTreeNodeFlags_None);
        bool hovered, held;

        auto style = ImGui::GetStyle();
        auto rounding = style.FrameRounding;

        if (ImGui::ButtonBehavior(bb, id, &hovered, &held, ImGuiButtonFlags_PressedOnClick))
            window->DC.StateStorage->SetInt(id, opened ? 0 : 1);
        if (hovered || held)
            window->DrawList->AddRectFilled(bb.Min, bb.Max, ImGui::GetColorU32(held ? ImGuiCol_HeaderActive : ImGuiCol_HeaderHovered), rounding);

        // Icon, text
        float button_sz = g.FontSize;
        pos.x += rounding / 2.f;
        auto icon = opened ? Icons::FontAwesomeIcons::FA_CHEVRON_DOWN : Icons::FontAwesomeIcons::FA_CHEVRON_RIGHT;
        ImGui::RenderText(ImVec2(pos.x + g.Style.ItemInnerSpacing.x, pos.y + g.Style.FramePadding.y), icon);
        ImGui::RenderText(ImVec2(pos.x + button_sz + g.Style.ItemInnerSpacing.x, pos.y + g.Style.FramePadding.y), label);

        ImGui::ItemSize(bb, g.Style.FramePadding.y);
        ImGui::ItemAdd(bb, id);

        if (opened)
            ImGui::TreePush(label);
        return opened;
    }

    std::string getIcon(SimEngine::ComponentType type) {
        switch (type) {
        case SimEngine::ComponentType::AND:
            return Icons::ComponentIcons::AND_GATE;
        case SimEngine::ComponentType::NAND:
            return Icons::ComponentIcons::NAND_GATE;
        case SimEngine::ComponentType::OR:
            return Icons::ComponentIcons::OR_GATE;
        case SimEngine::ComponentType::NOR:
            return Icons::ComponentIcons::NOR_GATE;
        case SimEngine::ComponentType::XOR:
            return Icons::ComponentIcons::XOR_GATE;
        case SimEngine::ComponentType::XNOR:
            return Icons::ComponentIcons::XNOR_GATE;
        default:
            break;
        }

        return std::string(" ") + Icons::FontAwesomeIcons::FA_CUBE + " ";
    }

    void ComponentExplorer::draw() {
        if (isfirstTimeDraw)
            firstTime();

        ImGui::Begin(windowName.c_str());
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
            if (MyTreeNode(ent.first.c_str())) {
                for (auto &comp : ent.second) {
                    auto name = comp.name;
                    if (m_searchQuery != "" && Common::Helpers::toLowerCase(name).find(m_searchQuery) == std::string::npos)
                        continue;

                    name = getIcon(comp.type) + "  " + name;
                    if (ImGui::Button(name.c_str(), {-1, 0})) {
                        auto simEntt = SimEngine::SimulationEngine::instance().addComponent(comp.type);
                        auto &scene = Canvas::Scene::instance();
                        scene.createSimEntity(simEntt, comp, scene.getCameraPos());
                        scene.setLastCreatedComp(&comp);
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
