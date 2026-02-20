#include "ui/widgets/m_widgets.h"
#include "imgui.h"
#include "imgui_internal.h"
#include "ui/icons/FontAwesomeIcons.h"

namespace Bess::UI::Widgets {
    static int InputTextCallback(ImGuiInputTextCallbackData *data) {
        if (data->EventFlag == ImGuiInputTextFlags_CallbackResize) {
            std::string *str = (std::string *)data->UserData;
            str->resize(data->BufTextLen);
            data->Buf = (char *)str->c_str();
        }
        return 0;
    }

    bool TextBox(const std::string &label, std::string &value, const std::string &hintText) {
        ImGui::AlignTextToFramePadding();
        if (!label.empty() && label[0] != '#' && label[1] != '#') {
            ImGui::Text("%s", label.c_str());
            ImGui::SameLine();
        }
        if (ImGui::InputTextWithHint(("##Tb" + label).c_str(), hintText.c_str(),
                                     value.data(), value.capacity() + 1, ImGuiInputTextFlags_CallbackResize,
                                     InputTextCallback, (void *)&value)) {
            return true;
        }

        return false;
    }

    bool CheckboxWithLabel(const char *label, bool *value) {
        ImGui::Text("%s", label);
        auto style = ImGui::GetStyle();
        float availWidth = ImGui::GetContentRegionAvail().x;
        ImGui::SameLine();
        float checkboxWidth = ImGui::CalcTextSize("X").x + style.FramePadding.x;
        ImGui::SetCursorPosX(availWidth - checkboxWidth);
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(0, 0));
        bool changed = ImGui::Checkbox(("##" + std::string(label)).c_str(), value);
        ImGui::PopStyleVar();
        return changed;
    }

    bool TreeNode(int key,
                  const std::string &name,
                  ImGuiTreeNodeFlags flags,
                  const std::string &icon,
                  glm::vec4 iconColor) {
        const ImGuiContext &g = *ImGui::GetCurrentContext();
        ImGuiWindow *window = g.CurrentWindow;

        const ImGuiID id = window->GetID(std::format("@{}@_{}", key, name).c_str());
        ImGui::PushID(id);
        ImVec2 pos = window->DC.CursorPos;
        const ImRect bb(pos, ImVec2(pos.x + ImGui::GetContentRegionAvail().x, pos.y + g.FontSize + (g.Style.FramePadding.y * 2)));
        const bool opened = window->DC.StateStorage->GetInt(id, (flags & ImGuiTreeNodeFlags_DefaultOpen) ? 1 : 0) != 0;
        bool hovered = false, held = false;

        const auto style = ImGui::GetStyle();
        const auto rounding = style.FrameRounding;

        if (ImGui::ButtonBehavior(bb, id, &hovered, &held, ImGuiButtonFlags_PressedOnClick))
            window->DC.StateStorage->SetInt(id, opened ? 0 : 1);
        if (hovered || held)
            window->DrawList->AddRectFilled(bb.Min, bb.Max,
                                            ImGui::GetColorU32(held
                                                                   ? ImGuiCol_ButtonActive
                                                                   : ImGuiCol_ButtonHovered),
                                            rounding);

        // Icon, text
        const float buttonSize = g.FontSize;
        pos.x += rounding / 2.f;
        const auto stateIcon = opened
                                   ? Icons::FontAwesomeIcons::FA_CHEVRON_DOWN
                                   : Icons::FontAwesomeIcons::FA_CHEVRON_RIGHT;

        ImGui::PushStyleColor(ImGuiCol_Text, g.Style.Colors[ImGuiCol_TextDisabled]);
        ImGui::RenderText(ImVec2(pos.x + g.Style.ItemInnerSpacing.x,
                                 pos.y + g.Style.FramePadding.y),
                          stateIcon);
        ImGui::PopStyleColor();

        float posIconOffsetX = 0.f;

        if (!icon.empty()) {
            if (iconColor.x >= 0.0f) {
                ImU32 col = ImGui::GetColorU32(ImVec4(iconColor.x,
                                                      iconColor.y,
                                                      iconColor.z,
                                                      iconColor.w));
                ImGui::PushStyleColor(ImGuiCol_Text, col);
            }

            ImGui::RenderText(ImVec2(pos.x + buttonSize +
                                         (g.Style.ItemInnerSpacing.x * 2),
                                     pos.y + g.Style.FramePadding.y),
                              icon.c_str());

            posIconOffsetX = ImGui::CalcTextSize(icon.c_str()).x + g.Style.ItemInnerSpacing.x;

            if (iconColor.x >= 0.0f) {
                ImGui::PopStyleColor();
            }
        }

        const float posX = pos.x + buttonSize + posIconOffsetX + g.Style.ItemInnerSpacing.x;
        ImGui::RenderText(ImVec2(posX, pos.y + g.Style.FramePadding.y),
                          name.c_str());

        ImGui::ItemSize(bb, g.Style.FramePadding.y);
        ImGui::ItemAdd(bb, id);

        if (opened)
            ImGui::TreePush(name.c_str());

        ImGui::PopID();
        return opened;
    }

    bool ButtonWithPopup(const std::string &label, const std::string &popupName, const bool showMenuButton) {
        const ImGuiContext &g = *ImGui::GetCurrentContext();
        ImGuiWindow *window = g.CurrentWindow;
        const ImVec2 pos = window->DC.CursorPos;
        const auto style = ImGui::GetStyle();

        const float btnContainerWidth = ImGui::GetContentRegionAvail().x;

        const ImRect bb(pos, ImVec2(pos.x + btnContainerWidth,
                                    pos.y + g.FontSize + (g.Style.FramePadding.y * 2)));

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

    std::pair<bool, bool> EditableTreeNode(
        uint64_t key,
        std::string &name,
        const bool selected,
        ImGuiTreeNodeFlags treeFlags,
        const std::string &icon,
        glm::vec4 iconColor,
        const std::string &popupName,
        uint64_t payloadId) {

        ImGuiContext &g = *ImGui::GetCurrentContext();
        ImGuiWindow *window = g.CurrentWindow;

        ImGui::PushID(key);

        const float rowHeight = g.FontSize + (g.Style.FramePadding.y * 2.0f);
        const ImVec2 pos = window->DC.CursorPos;
        const ImVec2 avail = ImGui::GetContentRegionAvail();

        ImDrawList *drawList = ImGui::GetWindowDrawList();

        if ((key & 1) == 0) {
            ImVec2 bgStart(window->Pos.x, pos.y);
            ImVec2 bgEnd(window->Pos.x + window->Size.x, pos.y + rowHeight);
            drawList->AddRectFilled(bgStart, bgEnd,
                                    (ImColor)g.Style.Colors[ImGuiCol_TableRowBgAlt], 0);
        }

        const ImRect rowBB(pos, ImVec2(pos.x + avail.x, pos.y + rowHeight));
        ImGui::ItemSize(rowBB, g.Style.FramePadding.y);

        const ImGuiID nodeId = window->GetID("node");
        const ImGuiID toggleId = window->GetID("toggle");
        const ImGuiID labelId = window->GetID("label");
        const ImGuiID openId = window->GetID("open");
        const ImGuiID editingId = window->GetID("editing");
        const ImGuiID editBufId = window->GetID("edit_buf");
        const ImGuiID prevNameId = window->GetID("prev_name");

        ImGuiStorage *st = window->DC.StateStorage;
        bool opened = st->GetInt(openId, (treeFlags & ImGuiTreeNodeFlags_DefaultOpen) ? 1 : 0) != 0;
        bool editing = st->GetInt(editingId, 0) != 0;

        bool rowHovered = ImGui::IsMouseHoveringRect(rowBB.Min, rowBB.Max);

        if (rowHovered) {
            const auto &bg = ImGui::GetColorU32(ImGuiCol_ButtonHovered);
            ImVec2 bgStart(window->Pos.x, pos.y);
            ImVec2 bgEnd(window->Pos.x + window->Size.x, pos.y + rowHeight);
            window->DrawList->AddRectFilled(bgStart, bgEnd, bg, g.Style.FrameRounding);
        } else if (selected) {
            const ImU32 &bg = ImGui::GetColorU32(ImGuiCol_ButtonHovered);
            window->DrawList->AddRectFilled(rowBB.Min, rowBB.Max, bg, g.Style.FrameRounding);
        }

        float toggleWidth = g.FontSize + (g.Style.ItemInnerSpacing.x * 2.0f);
        ImRect toggleRect(rowBB.Min, ImVec2(rowBB.Min.x + toggleWidth, rowBB.Max.y));
        bool toggleHovered = false, toggleHeld = false;

        if (ImGui::ButtonBehavior(toggleRect, toggleId, &toggleHovered, &toggleHeld)) {
            opened = !opened;
            st->SetInt(openId, opened ? 1 : 0);
        }

        ImGui::ItemAdd(toggleRect, toggleId);

        const auto stateIcon = opened
                                   ? Icons::FontAwesomeIcons::FA_CHEVRON_DOWN
                                   : Icons::FontAwesomeIcons::FA_CHEVRON_RIGHT;
        ImGui::PushStyleColor(ImGuiCol_Text, g.Style.Colors[ImGuiCol_TextDisabled]);
        ImGui::RenderText(ImVec2(toggleRect.Min.x + g.Style.ItemInnerSpacing.x, toggleRect.Min.y + g.Style.FramePadding.y), stateIcon);
        ImGui::PopStyleColor();

        float iconOffsetX = 0.0f;
        if (!icon.empty()) {
            float iconW = ImGui::CalcTextSize(icon.c_str()).x;
            iconOffsetX = iconW + g.Style.ItemInnerSpacing.x;
            if (iconColor.x >= 0)
                ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetColorU32(ImVec4(iconColor.x, iconColor.y, iconColor.z, iconColor.w)));
            ImGui::RenderText(ImVec2(toggleRect.Max.x, rowBB.Min.y + g.Style.FramePadding.y), icon.c_str());
            if (iconColor.x >= 0)
                ImGui::PopStyleColor();
        }

        // 3-dot Menu Button
        // Drawn before Label/Input but after background/toggle.
        // Needs to be interactive, so drawn before row behavior runs.
        // NOTE: Drawing button advances cursor, so we must restore it later to avoid layout glitches (shrinking height).
        ImVec2 backupCursorPos = window->DC.CursorPos;
        if (rowHovered) {
            ImGui::SetCursorScreenPos(ImVec2(rowBB.Max.x - g.FontSize - g.Style.FramePadding.x, rowBB.Min.y + g.Style.FramePadding.y));
            if (ImGui::SmallButton(Icons::FontAwesomeIcons::FA_ELLIPSIS_V)) {
                ImGui::OpenPopup(popupName.c_str());
            }
        }
        window->DC.CursorPos = backupCursorPos;

        ImVec2 labelPos(toggleRect.Max.x + iconOffsetX + g.Style.ItemInnerSpacing.x, rowBB.Min.y + g.Style.FramePadding.y);
        ImVec2 labelSize = ImGui::CalcTextSize(name.c_str());
        ImRect labelRect(labelPos, ImVec2(labelPos.x + labelSize.x + 10.0f, rowBB.Max.y));

        ImGui::ItemAdd(labelRect, labelId);

        bool beginEdit = false;
        if (!editing) {
            if (rowHovered && ImGui::IsMouseDoubleClicked(0))
                beginEdit = true;
            if (selected && ImGui::IsKeyPressed(ImGuiKey_F2))
                beginEdit = true;

            if (beginEdit) {
                st->SetInt(editingId, 1);
                editing = true;

                std::string *prevName = new std::string(name);
                st->SetVoidPtr(prevNameId, (void *)prevName);

                std::string *editBuf = new std::string(name);
                if (editBuf->empty())
                    editBuf->resize(1);
                st->SetVoidPtr(editBufId, (void *)editBuf);

                ImGui::SetKeyboardFocusHere();
            }
        }

        bool clicked = false;

        if (editing) {
            std::string *editBuf = (std::string *)st->GetVoidPtr(editBufId);
            if (!editBuf) {
                editing = false;
                st->SetInt(editingId, 0);
                ImGui::RenderText(labelPos, name.c_str());
            } else {
                if (editBuf->empty())
                    editBuf->resize(1);

                ImGui::SetCursorScreenPos(labelPos);
                ImGuiInputTextFlags flags = ImGuiInputTextFlags_AutoSelectAll |
                                            ImGuiInputTextFlags_EnterReturnsTrue |
                                            ImGuiInputTextFlags_CallbackResize;

                char *buf = editBuf->data();
                ImGui::SetCursorPosY(ImGui::GetCursorPosY() - g.Style.FramePadding.y);
                bool accepted = ImGui::InputText("##edit", buf, (size_t)editBuf->capacity() + 1,
                                                 flags, InputTextCallback, (void *)editBuf);

                bool commit = false;
                bool cancel = false;
                if (ImGui::IsKeyPressed(ImGuiKey_Escape))
                    cancel = true;
                if (ImGui::IsKeyPressed(ImGuiKey_Enter))
                    commit = true;
                // this is true when the item was active and then deactivated after edit (e.g., user clicked away)
                if (ImGui::IsItemDeactivatedAfterEdit() || ImGui::IsItemDeactivated())
                    commit = true;

                if (commit || cancel) {
                    if (cancel) {
                        std::string *prevName = (std::string *)st->GetVoidPtr(prevNameId);
                        if (prevName) {
                            name = *prevName;
                        }
                        delete prevName;
                    } else {
                        name = *editBuf;
                        std::string *prevName = (std::string *)st->GetVoidPtr(prevNameId);
                        delete prevName;
                    }

                    std::string *editBufPtr = (std::string *)st->GetVoidPtr(editBufId);
                    delete editBufPtr;
                    st->SetVoidPtr(editBufId, nullptr);
                    st->SetVoidPtr(prevNameId, nullptr);
                    st->SetInt(editingId, 0);
                    editing = false;

                    ImGui::ClearActiveID();
                }
            }
        } else {
            ImGui::RenderText(labelPos, name.c_str());

            bool rowHeld = false;
            bool rowPressed = false;
            rowPressed = ImGui::ButtonBehavior(rowBB, nodeId, &rowHovered,
                                               &rowHeld,
                                               ImGuiButtonFlags_PressedOnClick | ImGuiButtonFlags_AllowOverlap);

            if (rowPressed) {
                ImGui::SetItemDefaultFocus();
                clicked = true;
            }
        }

        // Restore cursor again just to be safe after InputText/RenderText which might move it
        window->DC.CursorPos = backupCursorPos;

        if (!editing) {
            ImGui::ItemAdd(rowBB, nodeId);

            if (ImGui::BeginDragDropSource()) {
                ImGui::SetDragDropPayload("TREE_NODE_PAYLOAD", (void *)(&payloadId), sizeof(payloadId));
                ImGui::Text("Dragging %s with id %lu", name.c_str(), payloadId);
                ImGui::EndDragDropSource();
            }
        }

        if (opened) {
            ImGui::TreePush(std::to_string(key).c_str());
        }

        ImGui::PopID();
        return {opened, clicked};
    }
} // namespace Bess::UI::Widgets
