#include "ui/ui_main/component_explorer.h"

#include "common/helpers.h"
#include "component_catalog.h"
#include "component_definition.h"
#include "imgui.h"
#include "imgui_internal.h"
#include "properties.h"
#include "scene/scene.h"
#include "simulation_engine.h"
#include "ui/icons/ComponentIcons.h"
#include "ui/icons/FontAwesomeIcons.h"
#include "ui/m_widgets.h"
#include <any>
#include <iostream>
#include <unordered_map>

namespace Bess::UI {

    bool ComponentExplorer::isfirstTimeDraw = false;
    std::string ComponentExplorer::m_searchQuery = "";
    std::string ComponentExplorer::windowName = (std::string(Icons::FontAwesomeIcons::FA_TOOLBOX) + "  Component Explorer");

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

    bool ButtonWithPopup(const std::string &label, const std::string &popupName, bool showMenuButton = true) {
        ImGuiContext &g = *ImGui::GetCurrentContext();
        ImGuiWindow *window = g.CurrentWindow;
        ImVec2 pos = window->DC.CursorPos;
        auto style = ImGui::GetStyle();
        ImRect bb(pos, ImVec2(pos.x + ImGui::GetContentRegionAvail().x, pos.y + g.FontSize + g.Style.FramePadding.y * 2));
        ImRect bbButton(pos, ImVec2(pos.x + ImGui::GetContentRegionAvail().x - style.FramePadding.x * 3 - g.Style.ItemInnerSpacing.x, pos.y + g.FontSize + g.Style.FramePadding.y * 2));
        float menuBtnX = bbButton.Max.x;
        float menuBtnSizeX = bb.Max.x - bbButton.Max.x;
        ImRect bbMenuButton(ImVec2(menuBtnX, pos.y + g.Style.FramePadding.y * 0.5f), ImVec2(menuBtnX + menuBtnSizeX, pos.y + g.FontSize + g.Style.FramePadding.y * 1.5));

        ImGuiID id = window->GetID(label.c_str());
        ImGuiID menuID = window->GetID((label + "##menu").c_str());

        bool hovered, held;
        bool clicked = ImGui::ButtonBehavior(bbButton, id, &hovered, &held, ImGuiButtonFlags_PressedOnClick);
        bool menuHovered, menuHeld;
        bool menuClicked = ImGui::ButtonBehavior(bbMenuButton, menuID, &menuHovered, &menuHeld, ImGuiButtonFlags_PressedOnClick);

        auto rounding = style.FrameRounding;

        auto bgColor = ImGui::GetColorU32(ImGuiCol_Button);
        if (menuHovered || hovered || held || ImGui::IsPopupOpen(popupName.c_str()))
            bgColor = ImGui::GetColorU32(held ? ImGuiCol_ButtonActive : ImGuiCol_ButtonHovered);

        window->DrawList->AddRectFilled(bb.Min, bb.Max, bgColor, rounding);
        ImGui::RenderText(ImVec2(pos.x + style.FramePadding.x + g.Style.ItemInnerSpacing.x, pos.y + g.Style.FramePadding.y), label.c_str());

        ImGui::ItemSize(bb, g.Style.FramePadding.y);
        ImGui::ItemAdd(bb, id);

        if (showMenuButton && (hovered || menuHovered)) {
            if (menuHovered)
                bgColor = ImGui::GetColorU32(ImGuiCol_TabActive);
            window->DrawList->AddRectFilled(bbMenuButton.Min, bbMenuButton.Max, bgColor, rounding);
            float x = bbMenuButton.Min.x + (bbMenuButton.Max.x - bbMenuButton.Min.x) / 2.f - 3.f;
            ImGui::RenderText(ImVec2(x, pos.y + g.Style.FramePadding.y), Icons::FontAwesomeIcons::FA_ELLIPSIS_V);
            if (menuClicked)
                ImGui::OpenPopup(popupName.c_str());
        }

        return clicked;
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

    void createComponent(const SimEngine::ComponentDefinition &def, int inputCount, int outputCount) {
        auto simEntt = SimEngine::SimulationEngine::instance().addComponent(def.type, inputCount, outputCount);
        auto &scene = Canvas::Scene::instance();
        scene.createSimEntity(simEntt, def, scene.getCameraPos());
        scene.setLastCreatedComp({&def, inputCount, outputCount});
    }

    typedef std::unordered_map<SimEngine::ComponentType, std::vector<std::pair<std::string, std::pair<SimEngine::Properties::ComponentProperty, std::any>>>> ModifiablePropertiesStr;

    ModifiablePropertiesStr generateModifiablePropertiesStr() {
        auto &components = SimEngine::ComponentCatalog::instance().getComponents();

        ModifiablePropertiesStr propertiesStr = {};

        for (auto &comp : components) {
            auto &p = comp.getModifiableProperties();
            if (p.empty())
                continue;

            propertiesStr[comp.type] = {};

            for (auto &mp : p) {
                std::string name;
                switch (mp.first) {
                case Bess::SimEngine::Properties::ComponentProperty::inputCount:
                    name = "Input Pins";
                    break;
                case Bess::SimEngine::Properties::ComponentProperty::outputCount:
                    name = "Output Pins";
                    break;
                default:
                    name = "Unknown Property";
                    break;
                }
                std::pair<std::string, std::pair<SimEngine::Properties::ComponentProperty, std::any>> v = {};
                for (auto &val : mp.second) {
                    v.first = std::to_string(std::any_cast<int>(val)) + " " + name;
                    v.second = {mp.first, val};
                    propertiesStr[comp.type].emplace_back(v);
                }
            }
        }

        return propertiesStr;
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
        static auto modifiableProperties = generateModifiablePropertiesStr();

        ImGuiTreeNodeFlags treeNodeFlags = ImGuiTreeNodeFlags_Framed;

        for (auto &ent : components) {
            if (MyTreeNode(ent.first.c_str())) {
                for (auto &comp : ent.second) {
                    auto name = comp.name;
                    if (m_searchQuery != "" && Common::Helpers::toLowerCase(name).find(m_searchQuery) == std::string::npos)
                        continue;

                    name = getIcon(comp.type) + "  " + name;
                    auto &properties = modifiableProperties[comp.type];

                    if (ButtonWithPopup(name, name + "OptionsMenu", !properties.empty())) {
                        createComponent(comp, -1, -1);
                    }

                    if (ImGui::BeginPopup((name + "OptionsMenu").c_str())) {
                        for (auto &p : properties) {
                            if (ImGui::MenuItem(p.first.c_str())) {
                                if (p.second.first == SimEngine::Properties::ComponentProperty::inputCount) {
                                    createComponent(comp, std::any_cast<int>(p.second.second), -1);
                                } else if (p.second.first == SimEngine::Properties::ComponentProperty::outputCount) {
                                    createComponent(comp, -1, std::any_cast<int>(p.second.second));
                                }
                            }
                        }
                        ImGui::EndPopup();
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
