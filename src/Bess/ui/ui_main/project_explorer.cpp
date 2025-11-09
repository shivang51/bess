#include "ui/ui_main/project_explorer.h"
#include "common/helpers.h"
#include "imgui.h"
#include "imgui_internal.h"
#include "scene/components/components.h"
#include "scene/scene.h"

namespace Bess::UI {

    bool ProjectExplorer::isShown = true;

    std::pair<bool, bool> ProjectExplorerNode(const uint64_t id_, const char *label, bool selected, const bool multiSelectMode) {
        const ImGuiContext &g = *ImGui::GetCurrentContext();
        const float rounding = g.Style.FrameRounding;
        const float checkboxWidth = ImGui::CalcTextSize("W").x + g.Style.FramePadding.x + 2.f;
        ImGuiWindow *window = g.CurrentWindow;
        const ImGuiID id = window->GetID(std::to_string(id_).c_str());
        const ImGuiID idcb = window->GetID((std::to_string(id_) + "cb").c_str());
        const ImVec2 pos = window->DC.CursorPos;
        const ImRect bb(pos, ImVec2(pos.x + ImGui::GetContentRegionAvail().x - checkboxWidth, pos.y + g.FontSize + (g.Style.FramePadding.y * 2)));
        auto checkBoxPos = bb.Max;
        checkBoxPos.y = pos.y;
        const ImRect bbCheckbox(checkBoxPos, ImVec2(checkBoxPos.x + checkboxWidth, checkBoxPos.y + g.FontSize + (g.Style.FramePadding.y * 2)));

        bool hovered = false, held = false;
        const auto pressed = ImGui::ButtonBehavior(bb, id, &hovered, &held, ImGuiButtonFlags_PressedOnClick);
        bool cbHovered = false, cbHeld = false;
        const auto cbPressed = ImGui::ButtonBehavior(bbCheckbox, idcb, &cbHovered, &cbHeld, ImGuiButtonFlags_PressedOnClick);

        const auto colors = ImGui::GetStyle().Colors;

        ImVec4 bgColor = selected ? colors[ImGuiCol_Header] : ImVec4(0, 0, 0, 0);
        if (hovered || held)
            bgColor = colors[ImGuiCol_HeaderHovered];

        const auto drawList = ImGui::GetWindowDrawList();
        auto size = bb.Max;
        size.x += checkboxWidth;
        drawList->AddRectFilled(bb.Min, size, IM_COL32(bgColor.x * 255, bgColor.y * 255, bgColor.z * 255, bgColor.w * 255), rounding);

        const auto fgColor = colors[ImGuiCol_Text];

        auto textStart = bb.Min;
        textStart.y += g.Style.FramePadding.y;
        textStart.x += g.Style.FramePadding.x;
        drawList->AddText(textStart, IM_COL32(fgColor.x * 255, fgColor.y * 255, fgColor.z * 255, fgColor.w * 255), label);

        if (hovered || cbHovered || multiSelectMode) {
            ImGui::SetCursorPosX(bbCheckbox.Min.x - bb.Min.x + (checkboxWidth / 2.f));
            const float yPadding = ImGui::GetCursorPosY();
            ImGui::SetCursorPosY(yPadding + g.Style.FramePadding.y);
            ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(0, 0));

            ImGui::Checkbox(("##CheckBox" + std::to_string(id_)).c_str(), &selected);
            ImGui::PopStyleVar();
            ImGui::SetCursorPosY(yPadding);
        }

        ImGui::ItemSize(bb, g.Style.FramePadding.y * 2);
        ImGui::ItemAdd(bb, id);

        return {pressed, cbPressed};
    }

    void ProjectExplorer::draw() {
        if (!isShown)
            return;

        ImGui::Begin(windowName.data(), nullptr, ImGuiWindowFlags_NoFocusOnAppearing);

        auto scene = Bess::Canvas::Scene::instance();
        auto &registry = scene->getEnttRegistry();

        const auto view = registry.view<Canvas::Components::TagComponent>();
        const auto size = view.size();

        const auto selTagGroup = registry.group<>(entt::get<Canvas::Components::SelectedComponent,
                                                            Canvas::Components::TagComponent>);
        const auto selSize = selTagGroup.size();

        const bool isMultiSelected = selSize > 1;

        if (size == 0) {
            ImGui::Text("No Components Added");
        } else if (size == 1) {
            ImGui::Text("%lu Component Added", view.size());
        } else {
            ImGui::Text("%lu Components Added", view.size());
        }

        if (selSize > 1) {
            ImGui::SameLine();
            ImGui::Text("(%lu / %lu Selected)", selSize, size);
        }

        std::string icon;
        for (const auto &entity : view) {
            const bool isSelected = selTagGroup.contains(entity);

            const auto &tagComp = view.get<Canvas::Components::TagComponent>(entity);
            if (tagComp.isSimComponent) {
                icon = Common::Helpers::getComponentIcon(tagComp.type.simCompHash);
            } else {
                icon = Common::Helpers::getComponentIcon(tagComp.type.nsCompType);
            }

            const auto [pressed, cbPressed] = ProjectExplorerNode((uint64_t)entity,
                                                                  (icon + "  " + tagComp.name).c_str(),
                                                                  isSelected, isMultiSelected);

            if (pressed) {
                registry.clear<Canvas::Components::SelectedComponent>();
                registry.emplace_or_replace<Canvas::Components::SelectedComponent>(entity);
            } else if (cbPressed) {
                if (isSelected) {
                    registry.remove<Canvas::Components::SelectedComponent>(entity);
                } else {
                    registry.emplace_or_replace<Canvas::Components::SelectedComponent>(entity);
                }
            }
        }

        ImGui::End();
    }
} // namespace Bess::UI
