#include "ui/ui_main/project_explorer.h"
#include "common/helpers.h"
#include "imgui.h"
#include "imgui_internal.h"
#include "scene/scene.h"
#include "ui/icons/CodIcons.h"

namespace Bess::UI {
    std::string ProjectExplorer::windowName = std::string(Icons::CodIcons::TYPE_HIERARCHY) + "  Project Explorer";

    std::pair<bool, bool> ProjectExplorerNode(uint64_t id_, const char *label, bool selected, bool multiSelectMode) {
        ImGuiContext &g = *ImGui::GetCurrentContext();
        float rounding = g.Style.FrameRounding;
        float checkboxWidth = ImGui::CalcTextSize("W").x + g.Style.FramePadding.x + 2.f;
        ImGuiWindow *window = g.CurrentWindow;
        ImGuiID id = window->GetID(id_);
        ImGuiID idcb = window->GetID((std::to_string(id_) + "cb").c_str());
        ImVec2 pos = window->DC.CursorPos;
        ImRect bb(pos, ImVec2(pos.x + ImGui::GetContentRegionAvail().x - checkboxWidth, pos.y + g.FontSize + g.Style.FramePadding.y * 2));
        auto checkBoxPos = bb.Max;
        checkBoxPos.y = pos.y;
        ImRect bbCheckbox(checkBoxPos, ImVec2(checkBoxPos.x + checkboxWidth, checkBoxPos.y + g.FontSize + g.Style.FramePadding.y * 2));

        bool hovered, held;
        auto pressed = ImGui::ButtonBehavior(bb, id, &hovered, &held, ImGuiButtonFlags_PressedOnClick);
        bool cbHovered, cbHeld;
        auto cbPressed = ImGui::ButtonBehavior(bbCheckbox, idcb, &cbHovered, &cbHeld, ImGuiButtonFlags_PressedOnClick);

        auto colors = ImGui::GetStyle().Colors;

        ImVec4 bgColor = selected ? colors[ImGuiCol_Header] : ImVec4(0, 0, 0, 0);
        if (hovered || held)
            bgColor = colors[ImGuiCol_HeaderHovered];

        auto drawList = ImGui::GetWindowDrawList();
        auto size = bb.Max;
        size.x += checkboxWidth;
        drawList->AddRectFilled(bb.Min, size, IM_COL32(bgColor.x * 255, bgColor.y * 255, bgColor.z * 255, bgColor.w * 255), rounding);

        auto fgColor = colors[ImGuiCol_Text];

        auto textStart = bb.Min;
        textStart.y += g.Style.FramePadding.y;
        textStart.x += g.Style.FramePadding.x;
        drawList->AddText(textStart, IM_COL32(fgColor.x * 255, fgColor.y * 255, fgColor.z * 255, fgColor.w * 255), label);

        if (hovered || cbHovered || multiSelectMode) {
            ImGui::SetCursorPosX(bbCheckbox.Min.x - bb.Min.x + checkboxWidth / 2.f);
            float yPadding = ImGui::GetCursorPosY();
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
        ImGui::Begin(windowName.c_str());

        auto &scene = Bess::Canvas::Scene::instance();
        auto &registry = scene.getEnttRegistry();
        auto view = registry.view<Canvas::Components::TagComponent>();
        auto sel = registry.view<Canvas::Components::SelectedComponent>().size();
        bool isMultiSelected = sel > 1;

        auto size = view.size();
        if (size == 0) {
            ImGui::Text("No Components Added");
        } else if (size == 1) {
            ImGui::Text("%lu Component Added", view.size());
        } else {
            ImGui::Text("%lu Components Added", view.size());
        }

        if (sel > 1) {
            ImGui::SameLine();
            ImGui::Text("(%lu / %lu Selected)", sel, size);
        }

        for (auto &entity : view) {
            bool isSelected = registry.all_of<Canvas::Components::SelectedComponent>(entity);
            auto &tagComp = view.get<Canvas::Components::TagComponent>(entity);
            auto icon = Common::Helpers::getComponentIcon(tagComp.type);
            auto [pressed, cbPressed] = ProjectExplorerNode((uint64_t)entity, (icon + "  " + tagComp.name).c_str(), isSelected, isMultiSelected);
            if (pressed || cbPressed) {
                if (pressed) {
                    registry.clear<Canvas::Components::SelectedComponent>();
                }
                registry.emplace_or_replace<Canvas::Components::SelectedComponent>(entity);
            }
        }

        ImGui::End();
    }
} // namespace Bess::UI
