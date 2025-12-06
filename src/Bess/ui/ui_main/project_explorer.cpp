#include "ui/ui_main/project_explorer.h"
#include "bess_uuid.h"
#include "common/log.h"
#include "imgui.h"
#include "imgui_internal.h"
#include "scene/components/components.h"
#include "scene/scene.h"
#include "ui/icons/FontAwesomeIcons.h"
#include "ui/ui_main/project_explorer_state.h"
#include <cstdint>
#include <memory>

namespace Bess::UI {
    bool ProjectExplorer::isShown = true;
    bool ProjectExplorer::isfirstTimeDraw = true;
    ProjectExplorerState ProjectExplorer::state{};
    ImColor ProjectExplorer::itemAltBg = ImColor(0, 0, 0);

#define DEF_NODE_DROP_TARGET(op)                                                                                                \
    if (ImGui::BeginDragDropTarget()) {                                                                                         \
        if (const ImGuiPayload *payload = ImGui::AcceptDragDropPayload("TREE_NODE_PAYLOAD")) {                                  \
            auto draggedNodeIDs = std::vector<uint64_t>((uint64_t *)payload->Data, ((uint64_t *)payload->Data) +                \
                                                                                       (payload->DataSize / sizeof(uint64_t))); \
            for (const auto &id : draggedNodeIDs) {                                                                             \
                op;                                                                                                             \
            }                                                                                                                   \
        }                                                                                                                       \
        ImGui::EndDragDropTarget();                                                                                             \
    }

    static int ImGuiStringCallback(ImGuiInputTextCallbackData *data) {
        if (data->EventFlag == ImGuiInputTextFlags_CallbackResize) {
            std::string *str = (std::string *)data->UserData;
            IM_ASSERT(str != nullptr);
            str->resize(data->BufTextLen);
            data->Buf = (char *)str->c_str();
        }
        return 0;
    }

    size_t ProjectExplorer::drawNodes(std::vector<std::shared_ptr<UI::ProjectExplorerNode>> &nodes, bool isRoot) {
        static int i = 0;
        constexpr auto treeIcon = Icons::CodIcons::FOLDER;
        constexpr auto nodePopupName = "node_popup";

        auto scene = Bess::Canvas::Scene::instance();
        auto &registry = scene->getEnttRegistry();

        const auto view = registry.view<Canvas::Components::TagComponent>();
        const auto selTagGroup = registry.group<>(entt::get<Canvas::Components::SelectedComponent,
                                                            Canvas::Components::TagComponent>);
        const auto selSize = selTagGroup.size();

        const bool isMultiSelected = selSize > 1;

        size_t count = 0;
        if (isRoot) {
            i = 0;
        }

        std::string icon;
        std::vector<std::pair<UUID, UUID>> pendingMoves;
        for (auto &node : nodes) {
            count++;
            if (node->isGroup) {
                if (EditableTreeNode(i++,
                                     node->label,
                                     node->selected,
                                     ImGuiTreeNodeFlags_DefaultOpen |
                                         ImGuiTreeNodeFlags_DrawLinesFull,
                                     treeIcon, ViewportTheme::colors.selectedWire, nodePopupName)) {
                    if (ImGui::BeginDragDropSource()) {
                        ImGui::SetDragDropPayload("TREE_NODE_PAYLOAD", (void *)(&node->nodeId), sizeof(node->nodeId));
                        ImGui::Text("Dragging %s with id %lu", node->label.c_str(), (uint64_t)node->nodeId);
                        ImGui::EndDragDropSource();
                    }

                    DEF_NODE_DROP_TARGET(pendingMoves.emplace_back(id, node->nodeId));

                    if (!node->children.empty()) {
                        count += drawNodes(node->children);
                    }

                    if (ImGui::BeginPopup(nodePopupName)) {
                        if (ImGui::MenuItemEx("Select All", "", "", false, true)) {
                            for (const auto &childNode : node->children) {
                                registry.emplace_or_replace<Canvas::Components::SelectedComponent>(
                                    childNode->sceneEntity);
                            }
                        }
                        ImGui::EndPopup();
                    }

                    ImGui::TreePop();
                }

            } else {
                const auto &tagComp = view.get<Canvas::Components::TagComponent>(node->sceneEntity);
                if (tagComp.isSimComponent) {
                    icon = Common::Helpers::getComponentIcon(tagComp.type.simCompHash);
                } else {
                    icon = Common::Helpers::getComponentIcon(tagComp.type.nsCompType);
                }
                const auto [pressed, cbPressed] = ProjectExplorerNode(
                    i++,
                    node->nodeId,
                    (icon + " " + node->label).c_str(),
                    node->selected,
                    isMultiSelected);

                if (ImGui::BeginDragDropSource()) {
                    std::vector<uint64_t> payloadData;
                    selTagGroup.each([&](const entt::entity entity,
                                         const Canvas::Components::SelectedComponent &,
                                         const Canvas::Components::TagComponent &) {
                        const auto selectedNode = state.getNodeFromSceneEntity(entity);
                        if (selectedNode != nullptr) {
                            payloadData.emplace_back((uint64_t)selectedNode->nodeId);
                        }
                    });
                    ImGui::SetDragDropPayload("TREE_NODE_PAYLOAD", payloadData.data(),
                                              payloadData.size() * sizeof(uint64_t));
                    ImGui::Text("Dragging %lu nodes", payloadData.size());
                    ImGui::EndDragDropSource();
                }

                const auto &entity = node->sceneEntity;
                node->selected = selTagGroup.contains(entity);

                if (pressed) {
                    if (selSize == 0) {
                        registry.clear<Canvas::Components::SelectedComponent>();
                        registry.emplace_or_replace<Canvas::Components::SelectedComponent>(entity);
                    }
                } else if (cbPressed) {
                    if (node->selected) {
                        registry.remove<Canvas::Components::SelectedComponent>(entity);
                    } else {
                        registry.emplace_or_replace<Canvas::Components::SelectedComponent>(entity);
                    }
                }
            }
        }

        for (const auto &[draggedNodeID, newParentID] : pendingMoves) {
            state.moveNode(draggedNodeID, newParentID);
        }

        return count;
    }

    void ProjectExplorer::draw() {
        if (!isShown)
            return;

        if (isfirstTimeDraw) {
            firstTime();
        }

        itemAltBg = ImGui::GetStyle().Colors[ImGuiCol_TableRowBgAlt];
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

        std::unordered_map<UUID, std::vector<entt::entity>> netGroups;
        for (const auto &entity : view) {
            const bool isSelected = selTagGroup.contains(entity);
            const auto &tagComp = view.get<Canvas::Components::TagComponent>(entity);
            netGroups[tagComp.netId].emplace_back(entity);
        }

        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0, 0));
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(2, 2));
        const auto nodesCount = drawNodes(state.nodes, true);

        const ImGuiContext &g = *ImGui::GetCurrentContext();
        const ImGuiWindow *window = g.CurrentWindow;
        const auto drawList = ImGui::GetWindowDrawList();
        const auto colors = ImGui::GetStyle().Colors;
        const float height = g.FontSize + (g.Style.FramePadding.y * 2);
        const float x = window->Pos.x;
        float y = window->DC.CursorPos.y;
        float startY = y;
        ImVec2 bgStart, bgEnd;

        if (nodesCount & 1) { // skipping if its not alternating color row
            y += height;
        }

        while (y < window->Pos.y + window->Size.y) {
            bgStart = ImVec2(x, y);
            bgEnd = ImVec2(x + window->Size.x, y + height);
            drawList->AddRectFilled(bgStart, bgEnd, itemAltBg, 0);
            y += height * 2;
        }

        ImGui::PopStyleVar(2);

        if (ImGui::InvisibleButton("project_explorer_root_drop_target", ImVec2(window->Size.x, window->Size.y - startY))) {
        }

        DEF_NODE_DROP_TARGET(state.moveNode(id, UUID::null));

        if (ImGui::BeginPopupContextWindow()) {
            if (isMultiSelected) {
                if (ImGui::MenuItem("Group Selected", "Ctrl-G")) {
                    auto groupNode = std::make_shared<UI::ProjectExplorerNode>();
                    groupNode->isGroup = true;
                    groupNode->label = "New Group Node";
                    state.addNode(groupNode);

                    for (const auto &entity : selTagGroup) {
                        const auto node = state.getNodeFromSceneEntity(entity);
                        if (node != nullptr) {
                            state.moveNode(node, groupNode);
                        }
                    }
                }
            } else {
                if (ImGui::MenuItem("Create Empty Group", "Ctrl-G")) {
                    auto groupNode = std::make_shared<UI::ProjectExplorerNode>();
                    groupNode->isGroup = true;
                    groupNode->label = "New Group Node";
                    state.addNode(groupNode);
                }
            }
            ImGui::EndPopup();
        }
        ImGui::End();
    }

    std::pair<bool, bool> ProjectExplorer::ProjectExplorerNode(const int key, const uint64_t nodeId,
                                                               const char *label, bool selected,
                                                               const bool multiSelectMode) {
        const ImGuiContext &g = *ImGui::GetCurrentContext();
        const float rounding = g.Style.FrameRounding;
        const float checkboxWidth = ImGui::CalcTextSize("W").x + g.Style.FramePadding.x + 2.f;
        ImGuiWindow *window = g.CurrentWindow;
        const ImGuiID id = window->GetID(std::to_string(nodeId).c_str());
        const ImGuiID idcb = window->GetID((std::to_string(nodeId) + "cb").c_str());
        const ImVec2 pos = window->DC.CursorPos;
        const auto drawList = ImGui::GetWindowDrawList();

        const auto colors = ImGui::GetStyle().Colors;

        if ((key & 1) == 0) {
            float x = window->Pos.x;
            float y = pos.y;
            ImVec2 avail = ImGui::GetContentRegionAvail();
            ImVec2 bgStart(x, y);
            ImVec2 bgEnd(x + window->Size.x, y + g.FontSize + (g.Style.FramePadding.y * 2));
            drawList->AddRectFilled(bgStart, bgEnd, itemAltBg, 0);
        }

        const ImRect bb(pos, ImVec2(pos.x + ImGui::GetContentRegionAvail().x - checkboxWidth,
                                    pos.y + g.FontSize + (g.Style.FramePadding.y * 2)));
        auto checkBoxPos = bb.Max;
        checkBoxPos.y = pos.y;
        const ImRect bbCheckbox(checkBoxPos,
                                ImVec2(checkBoxPos.x + checkboxWidth,
                                       checkBoxPos.y + g.FontSize + (g.Style.FramePadding.y * 2)));

        bool hovered = false, held = false;
        const auto pressed = ImGui::ButtonBehavior(bb, id, &hovered, &held, ImGuiButtonFlags_PressedOnClick);
        bool cbHovered = false, cbHeld = false;
        const auto cbPressed = ImGui::ButtonBehavior(bbCheckbox, idcb, &cbHovered,
                                                     &cbHeld, ImGuiButtonFlags_PressedOnClick);

        ImVec4 bgColor = ImVec4(0, 0, 0, 0);
        if (selected) {
            bgColor = colors[ImGuiCol_HeaderActive];
        } else if (hovered || held) {
            bgColor = colors[ImGuiCol_HeaderHovered];
        }

        if (bgColor.w > 0.0f) {
            float x = window->Pos.x;
            float y = pos.y;
            ImVec2 avail = ImGui::GetContentRegionAvail();
            ImVec2 bgStart(x, y);
            ImVec2 bgEnd(x + window->Size.x, y + g.FontSize + (g.Style.FramePadding.y * 2));
            auto color = IM_COL32(bgColor.x * 255, bgColor.y * 255, bgColor.z * 255, 200);
            drawList->AddRectFilled(bgStart, bgEnd, color, 0);
        }

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

            ImGui::Checkbox(("##CheckBox" + std::to_string(nodeId)).c_str(), &selected);
            ImGui::PopStyleVar();
            ImGui::SetCursorPosY(yPadding);
        }

        ImGui::ItemSize(bb, g.Style.FramePadding.y * 2);
        ImGui::ItemAdd(bb, id);

        return {pressed, cbPressed};
    }

    bool ProjectExplorer::EditableTreeNode(
        uint64_t key,
        std::string &name,
        bool &selected,
        ImGuiTreeNodeFlags treeFlags,
        const std::string &icon,
        glm::vec4 iconColor,
        const std::string &popupName) {

        ImGuiContext &g = *ImGui::GetCurrentContext();
        ImGuiWindow *window = g.CurrentWindow;
        if (!window)
            return false;

        ImGui::PushID(key);

        const float rowHeight = g.FontSize + (g.Style.FramePadding.y * 2.0f);
        const ImVec2 pos = window->DC.CursorPos;
        const ImVec2 avail = ImGui::GetContentRegionAvail();

        ImDrawList *drawList = ImGui::GetWindowDrawList();

        // alternating background
        if ((key & 1) == 0) {
            ImVec2 bgStart(window->Pos.x, pos.y);
            ImVec2 bgEnd(window->Pos.x + window->Size.x, pos.y + rowHeight);
            drawList->AddRectFilled(bgStart, bgEnd, itemAltBg, 0);
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

        // read persistent states from window storage
        ImGuiStorage *st = window->DC.StateStorage;
        bool opened = st->GetInt(openId, (treeFlags & ImGuiTreeNodeFlags_DefaultOpen) ? 1 : 0) != 0;
        bool editing = st->GetInt(editingId, 0) != 0;

        // Hover state
        bool rowHovered = ImGui::IsMouseHoveringRect(rowBB.Min, rowBB.Max);

        // Draw selected / hovered background
        if (selected || rowHovered) {
            ImU32 bg = selected
                           ? ImGui::GetColorU32(ImGuiCol_HeaderActive)
                           : ImGui::GetColorU32(ImGuiCol_HeaderHovered);
            window->DrawList->AddRectFilled(rowBB.Min, rowBB.Max, bg, g.Style.FrameRounding);
        }

        // Toggle (chevron) area
        float toggleWidth = g.FontSize + (g.Style.ItemInnerSpacing.x * 2.0f);
        ImRect toggleRect(rowBB.Min, ImVec2(rowBB.Min.x + toggleWidth, rowBB.Max.y));
        bool toggleHovered = false, toggleHeld = false;

        if (ImGui::ButtonBehavior(toggleRect, toggleId, &toggleHovered, &toggleHeld)) {
            opened = !opened;
            st->SetInt(openId, opened ? 1 : 0);
        }

        ImGui::ItemAdd(toggleRect, toggleId);

        // Render chevron
        const auto stateIcon = opened
                                   ? Icons::FontAwesomeIcons::FA_CHEVRON_DOWN
                                   : Icons::FontAwesomeIcons::FA_CHEVRON_RIGHT;
        ImGui::PushStyleColor(ImGuiCol_Text, g.Style.Colors[ImGuiCol_TextDisabled]);
        ImGui::RenderText(ImVec2(toggleRect.Min.x + g.Style.ItemInnerSpacing.x, toggleRect.Min.y + g.Style.FramePadding.y), stateIcon);
        ImGui::PopStyleColor();

        // Icon (if any)
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

        // Label area
        ImVec2 labelPos(toggleRect.Max.x + iconOffsetX + g.Style.ItemInnerSpacing.x, rowBB.Min.y + g.Style.FramePadding.y);
        ImVec2 labelSize = ImGui::CalcTextSize(name.c_str());
        ImRect labelRect(labelPos, ImVec2(labelPos.x + labelSize.x + 10.0f, rowBB.Max.y));

        // Invisible item for event capture
        ImGui::ItemAdd(labelRect, labelId);
        bool labelHovered = ImGui::ItemHoverable(labelRect, labelId, 0);

        // Selection logic: click on label selects this node; clicking anywhere else clears it.
        if (!editing) {
            if (labelHovered && ImGui::IsMouseClicked(0)) {
                selected = true;
                // focus this item so keyboard navigation works as expected
                ImGui::SetItemDefaultFocus();
            } else if (ImGui::IsMouseClicked(0) && !labelHovered && !toggleHovered) {
                // somebody clicked elsewhere -> clear selection for this node
                selected = false;
            }
        }

        // Begin rename logic: double-click on label OR press F2 while selected.
        bool beginEdit = false;
        if (!editing) {
            if (labelHovered && ImGui::IsMouseDoubleClicked(0))
                beginEdit = true;
            if (selected && ImGui::IsKeyPressed(ImGuiKey_F2))
                beginEdit = true;

            if (beginEdit) {
                // mark editing state
                st->SetInt(editingId, 1);
                editing = true;

                // store previous name so we can restore on cancel
                std::string *prevName = new std::string(name);
                st->SetVoidPtr(prevNameId, (void *)prevName);

                // create edit buffer and store it as well
                std::string *editBuf = new std::string(name);
                // ensure there's at least one char so &(*editBuf)[0] is valid
                if (editBuf->empty())
                    editBuf->resize(1);
                st->SetVoidPtr(editBufId, (void *)editBuf);

                // Focus next widget (the InputText will be next)
                ImGui::SetKeyboardFocusHere();
            }
        }

        // Rendering or editing
        if (editing) {
            // retrieve edit buffer
            std::string *editBuf = (std::string *)st->GetVoidPtr(editBufId);
            if (!editBuf) {
                // safety: if something went wrong, fall back to non-edit mode
                editing = false;
                st->SetInt(editingId, 0);
                ImGui::RenderText(labelPos, name.c_str());
            } else {
                // Make sure buffer has at least one char for ImGui to write into
                if (editBuf->empty())
                    editBuf->resize(1);

                // place cursor and call InputText. Use CallbackResize so the std::string can grow safely.
                ImGui::SetCursorScreenPos(labelPos);
                ImGuiInputTextFlags flags = ImGuiInputTextFlags_AutoSelectAll |
                                            ImGuiInputTextFlags_EnterReturnsTrue |
                                            ImGuiInputTextFlags_CallbackResize;

                char *buf = editBuf->data();
                ImGui::SetCursorPosY(ImGui::GetCursorPosY() - g.Style.FramePadding.y);
                bool accepted = ImGui::InputText("##edit", buf, (size_t)editBuf->capacity() + 1,
                                                 flags, ImGuiStringCallback, (void *)editBuf);

                bool commit = false;
                bool cancel = false;
                if (ImGui::IsKeyPressed(ImGuiKey_Escape))
                    cancel = true;
                if (ImGui::IsKeyPressed(ImGuiKey_Enter))
                    commit = true;
                // this is true when the item was active and then deactivated after edit (e.g., user clicked away)
                if (ImGui::IsItemDeactivatedAfterEdit())
                    commit = true;

                if (commit || cancel) {
                    if (cancel) {
                        // restore previous name
                        std::string *prevName = (std::string *)st->GetVoidPtr(prevNameId);
                        if (prevName) {
                            name = *prevName;
                        }
                        delete prevName;
                    } else {
                        // commit changes: copy editBuf into name
                        // editBuf may contain no trailing null; ensure string size is correct
                        // Note: InputText + CallbackResize guarantees editBuf->size() == BufTextLen
                        name = *editBuf;
                        // delete prev name
                        std::string *prevName = (std::string *)st->GetVoidPtr(prevNameId);
                        delete prevName;
                    }

                    // cleanup edit buffer and editing flag
                    std::string *editBufPtr = (std::string *)st->GetVoidPtr(editBufId);
                    delete editBufPtr;
                    st->SetVoidPtr(editBufId, nullptr);
                    st->SetVoidPtr(prevNameId, nullptr);
                    st->SetInt(editingId, 0);
                    editing = false;

                    // clear keyboard focus so subsequent clicks behave normally
                    ImGui::ClearActiveID();
                }
            }
        } else { // Normal label rendering
            ImGui::RenderText(labelPos, name.c_str());
        }

        if (rowHovered) {
            ImGui::SameLine();
            ImGui::SetCursorPosX(rowBB.Max.x - g.FontSize - g.Style.FramePadding.x);
            if (ImGui::SmallButton(Icons::FontAwesomeIcons::FA_ELLIPSIS_V)) {
                ImGui::OpenPopup(popupName.c_str());
            }
        }

        if (opened) {
            ImGui::TreePush(std::to_string(key).c_str());
        }

        ImGui::ItemAdd(rowBB, nodeId);

        ImGui::PopID();
        return opened;
    }

    void ProjectExplorer::firstTime() {
        isfirstTimeDraw = false;
        state.reset();
    }
} // namespace Bess::UI
