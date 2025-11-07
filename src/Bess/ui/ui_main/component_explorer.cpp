#include "ui/ui_main/component_explorer.h"
#include "common/helpers.h"
#include "common/log.h"
#include "component_catalog.h"
#include "imgui.h"
#include "imgui_internal.h"
#include "scene/commands/add_command.h"
#include "scene/scene.h"
#include "scene/scene_pch.h"
#include "ui/icons/FontAwesomeIcons.h"
#include "ui/widgets/m_widgets.h"
#include <utility>

namespace Bess::UI {

    bool ComponentExplorer::isShown = false;
    bool ComponentExplorer::m_isfirstTimeDraw = true;
    std::string ComponentExplorer::m_searchQuery;

    bool MyTreeNode(const char *label, ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_None) {
        const ImGuiContext &g = *ImGui::GetCurrentContext();
        ImGuiWindow *window = g.CurrentWindow;

        const ImGuiID id = window->GetID(label);
        ImVec2 pos = window->DC.CursorPos;
        const ImRect bb(pos, ImVec2(pos.x + ImGui::GetContentRegionAvail().x, pos.y + g.FontSize + (g.Style.FramePadding.y * 2)));
        const bool opened = window->DC.StateStorage->GetInt(id, (flags & ImGuiTreeNodeFlags_DefaultOpen) ? 1 : 0) != 0;
        bool hovered = false, held = false;

        const auto style = ImGui::GetStyle();
        const auto rounding = style.FrameRounding;

        if (ImGui::ButtonBehavior(bb, id, &hovered, &held, ImGuiButtonFlags_PressedOnClick))
            window->DC.StateStorage->SetInt(id, opened ? 0 : 1);
        if (hovered || held)
            window->DrawList->AddRectFilled(bb.Min, bb.Max, ImGui::GetColorU32(held ? ImGuiCol_HeaderActive : ImGuiCol_HeaderHovered), rounding);

        // Icon, text
        const float button_sz = g.FontSize;
        pos.x += rounding / 2.f;
        const auto icon = opened ? Icons::FontAwesomeIcons::FA_CHEVRON_DOWN : Icons::FontAwesomeIcons::FA_CHEVRON_RIGHT;
        ImGui::RenderText(ImVec2(pos.x + g.Style.ItemInnerSpacing.x, pos.y + g.Style.FramePadding.y), icon);
        ImGui::RenderText(ImVec2(pos.x + button_sz + g.Style.ItemInnerSpacing.x, pos.y + g.Style.FramePadding.y), label);

        ImGui::ItemSize(bb, g.Style.FramePadding.y);
        ImGui::ItemAdd(bb, id);

        if (opened)
            ImGui::TreePush(label);
        return opened;
    }

    bool ButtonWithPopup(const std::string &label, const std::string &popupName, const bool showMenuButton = true) {
        const ImGuiContext &g = *ImGui::GetCurrentContext();
        ImGuiWindow *window = g.CurrentWindow;
        const ImVec2 pos = window->DC.CursorPos;
        const auto style = ImGui::GetStyle();

        const float btnContainerWidth = ImGui::GetContentRegionAvail().x;

        const ImRect bb(pos, ImVec2(pos.x + btnContainerWidth,
                                    pos.y + g.FontSize + (g.Style.FramePadding.y * 2)));
        // const ImRect bbButton(pos,
        //                       ImVec2(pos.x + ImGui::GetContentRegionAvail().x - (style.FramePadding.x * 3) - g.Style.ItemInnerSpacing.x,
        //                              pos.y + g.FontSize + (g.Style.FramePadding.y * 2)));
        const float menuBtnSizeY = showMenuButton
                                       ? g.FontSize + (g.Style.FramePadding.y * 1.5f)
                                       : 0;
        const float menuBtnSizeX = showMenuButton
                                       ? menuBtnSizeY
                                       : 0;
        const float menuBtnX = showMenuButton
                                   ? btnContainerWidth - menuBtnSizeX - g.Style.ItemInnerSpacing.x
                                   : btnContainerWidth; // extend to the end if no menu button

        const ImRect bbButton(pos,
                              ImVec2(pos.x + menuBtnX,
                                     pos.y + g.FontSize + (g.Style.FramePadding.y * 2)));

        const ImRect bbMenuButton(ImVec2(pos.x + menuBtnX,
                                         pos.y + (g.Style.FramePadding.y * 0.5f)),
                                  ImVec2(pos.x + menuBtnX + menuBtnSizeX,
                                         pos.y + menuBtnSizeY));

        const ImGuiID id = window->GetID(label.c_str());

        bool hovered = false, held = false;
        const bool clicked = ImGui::ButtonBehavior(bbButton, id, &hovered, &held, ImGuiButtonFlags_PressedOnClick);

        bool menuHovered = false, menuHeld = false;
        bool menuClicked = false;

        if (showMenuButton) {
            const ImGuiID menuID = window->GetID((label + "##menu").c_str());
            menuClicked = ImGui::ButtonBehavior(bbMenuButton, menuID, &menuHovered, &menuHeld, ImGuiButtonFlags_PressedOnClick);
        }

        auto bgColor = ImGui::GetColorU32(ImGuiCol_Button);
        if (menuHovered || hovered || held || ImGui::IsPopupOpen(popupName.c_str()))
            bgColor = ImGui::GetColorU32(held ? ImGuiCol_ButtonActive : ImGuiCol_ButtonHovered);

        const auto rounding = style.FrameRounding;
        window->DrawList->AddRectFilled(bb.Min, bb.Max, bgColor, rounding);
        ImGui::RenderText(ImVec2(pos.x + style.FramePadding.x + g.Style.ItemInnerSpacing.x, pos.y + g.Style.FramePadding.y), label.c_str());

        ImGui::ItemSize(bb, g.Style.FramePadding.y);
        ImGui::ItemAdd(bb, id);

        if (showMenuButton && (hovered || menuHovered)) {
            if (menuHovered)
                bgColor = ImGui::GetColorU32(ImGuiCol_TabActive);
            window->DrawList->AddRectFilled(bbMenuButton.Min, bbMenuButton.Max, bgColor, rounding);
            const float x = bbMenuButton.Min.x + ((bbMenuButton.Max.x - bbMenuButton.Min.x) / 2.f) - 3.f;
            ImGui::RenderText(ImVec2(x, pos.y + g.Style.FramePadding.y), Icons::FontAwesomeIcons::FA_ELLIPSIS_V);
            if (menuClicked)
                ImGui::OpenPopup(popupName.c_str());
        }

        return clicked;
    }

    void ComponentExplorer::createComponent(std::shared_ptr<const SimEngine::ComponentDefinition> def, const int inputCount, const int outputCount) {
        auto scene = Canvas::Scene::instance();
        Canvas::Commands::AddCommandData data;
        data.def = *def.get();
        data.pos = scene->getCameraPos();
        data.inputCount = inputCount;
        data.outputCount = outputCount;
        auto &cmdManager = scene->getCmdManager();
        const auto res = cmdManager.execute<Canvas::Commands::AddCommand, std::vector<UUID>>(std::vector{data});
        if (!res.has_value()) {
            BESS_ERROR("[ComponentExplorer][SimComp] Failed to execute AddCommand");
        }
        isShown = false;
    }

    void ComponentExplorer::createComponent(const Canvas::Components::NSComponent &comp) {
        auto scene = Canvas::Scene::instance();
        Canvas::Commands::AddCommandData data;
        data.nsComp = comp;
        data.pos = scene->getCameraPos();
        auto &cmdManager = scene->getCmdManager();
        const auto res = cmdManager.execute<Canvas::Commands::AddCommand, std::vector<UUID>>(std::vector{data});
        if (!res.has_value()) {
            BESS_ERROR("[ComponentExplorer][NonSimComp] Failed to execute AddCommand");
        }
        isShown = false;
    }

    void ComponentExplorer::draw() {
        if (!isShown)
            return;

        if (m_isfirstTimeDraw)
            firstTime();

        const ImVec2 windowSize = ImVec2(400, 400);
        ImGui::SetNextWindowSize(windowSize);

        const auto *viewport = ImGui::GetMainViewport();
        const ImGuiContext &g = *ImGui::GetCurrentContext();
        const auto style = g.Style;
        ImGui::SetNextWindowPos(
            ImVec2(viewport->Pos.x + (viewport->Size.x / 2) - (windowSize.x / 2),
                   viewport->Pos.y + (viewport->Size.y / 2) - (windowSize.y / 2)));

        constexpr auto flags = ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize |
                               ImGuiWindowFlags_NoMove | ImGuiWindowFlags_AlwaysAutoResize |
                               ImGuiWindowFlags_Modal;
        ImGui::Begin(windowName.data(), &isShown, flags);

        isShown = ImGui::IsWindowFocused();
        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 4);

        static bool focusRequested = false;
        {
            focusRequested = ImGui::IsWindowAppearing();

            if (focusRequested) {
                ImGui::SetKeyboardFocusHere();
                focusRequested = false;
            }

            ImGui::PushItemWidth(-1);
            if (UI::Widgets::TextBox("##Search", m_searchQuery, "Search")) {
                m_searchQuery = Common::Helpers::toLowerCase(m_searchQuery);
            }
            ImGui::PopItemWidth();
        }

        ImGui::PushStyleVar(ImGuiStyleVar_ButtonTextAlign, ImVec2(0.f, 0.5f));
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));
        ImGui::PushStyleColor(ImGuiCol_Header, ImVec4(0, 0, 0, 0)); // for tree node to have no bg normally

        const auto componentTree = SimEngine::ComponentCatalog::instance().getComponentsTree();
        // simulation components
        {
            for (auto &ent : *componentTree) {
                bool shouldCollectionShow = m_searchQuery.empty();

                if (!shouldCollectionShow) {
                    for (const auto &comp : ent.second) {
                        if (Common::Helpers::toLowerCase(comp->name).find(m_searchQuery) != std::string::npos) {
                            shouldCollectionShow = true;
                            break;
                        }
                    }
                }

                if (!shouldCollectionShow)
                    continue;

                if (MyTreeNode(ent.first.c_str(), ImGuiTreeNodeFlags_DefaultOpen)) {
                    for (const auto &comp : ent.second) {
                        auto name = comp->name;
                        if (m_searchQuery != "" && Common::Helpers::toLowerCase(name).find(m_searchQuery) == std::string::npos)
                            continue;

                        name = Common::Helpers::getComponentIcon(comp->getHash()) + "  " + name;

                        if (ButtonWithPopup(name, name + "OptionsMenu", !comp->getAltInputCounts().empty())) {
                            createComponent(comp, -1, -1);
                        }

                        if (ImGui::BeginPopup((name + "OptionsMenu").c_str())) {
                            for (auto &inpCount : comp->getAltInputCounts()) {
                                if (ImGui::MenuItem(std::format("{} Inputs", inpCount).c_str())) {
                                    createComponent(comp, inpCount, -1);
                                }
                            }
                            ImGui::EndPopup();
                        }
                    }
                    ImGui::TreePop();
                }
            }
        }

        // non simulation components
        if (MyTreeNode("Miscellaneous", ImGuiTreeNodeFlags_DefaultOpen)) {
            static auto nonSimComponents = Canvas::Components::getNSComponents();
            for (auto &comp : nonSimComponents) {
                if (m_searchQuery != "" && Common::Helpers::toLowerCase(comp.name).find(m_searchQuery) == std::string::npos)
                    continue;

                std::string name = comp.name;
                name = Common::Helpers::getComponentIcon(comp.type) + "  " + name;

                if (ButtonWithPopup(name, "", false)) {
                    createComponent(comp);
                }
            }

            ImGui::TreePop();
        }

        ImGui::PopStyleColor(2);
        ImGui::PopStyleVar(2);
        ImGui::End();
    }

    void ComponentExplorer::firstTime() {
        assert(m_isfirstTimeDraw);
        m_isfirstTimeDraw = false;
    }
} // namespace Bess::UI
