#include "ui/ui_main/project_explorer.h"
#include "bess_uuid.h"
#include "imgui.h"
#include "imgui_internal.h"
#include "scene/commands/delete_comp_command.h"
#include "scene/components/components.h"
#include "scene/scene.h"
#include "ui/ui_main/project_explorer_state.h"
#include "ui/widgets/m_widgets.h"
#include <cstdint>
#include <memory>

namespace Bess::UI {
    bool ProjectExplorer::isShown = true;
    bool ProjectExplorer::isfirstTimeDraw = true;
    ProjectExplorerState ProjectExplorer::state{};

#define DEF_NODE_DROP_TARGET(op)                                                               \
    if (ImGui::BeginDragDropTarget()) {                                                        \
        if (const ImGuiPayload *payload = ImGui::AcceptDragDropPayload("TREE_NODE_PAYLOAD")) { \
            const auto dataPtr = (uint64_t *)payload->Data;                                    \
            const auto size = (payload->DataSize / sizeof(uint64_t));                          \
            const auto draggedNodeIDs = std::vector<uint64_t>(dataPtr, dataPtr + size);        \
            for (const auto &id : draggedNodeIDs) {                                            \
                op;                                                                            \
            }                                                                                  \
        }                                                                                      \
        ImGui::EndDragDropTarget();                                                            \
    }

    int ProjectExplorer::lastSelectedIndex = -1;

    void ProjectExplorer::clearAllSelections() {
        auto scene = Bess::Canvas::Scene::instance();
        auto &registry = scene->getEnttRegistry();
        registry.clear<Canvas::Components::SelectedComponent>();

        // Note (Shivang): Can't use auto here because of recursive lambda
        std::function<void(std::vector<std::shared_ptr<UI::ProjectExplorerNode>> &)> clearNodes =
            [&](std::vector<std::shared_ptr<UI::ProjectExplorerNode>> &nodes) {
                for (auto &node : nodes) {
                    node->selected = false;
                    if (!node->children.empty()) {
                        clearNodes(node->children);
                    }
                }
            };
        clearNodes(state.nodes);
    }

    void ProjectExplorer::selectRange(int start, int end) {
        auto scene = Bess::Canvas::Scene::instance();
        auto &registry = scene->getEnttRegistry();

        clearAllSelections();

        // Note (Shivang): Can't use auto here because of recursive lambda
        std::function<void(std::vector<std::shared_ptr<UI::ProjectExplorerNode>> &)> traverse =
            [&](std::vector<std::shared_ptr<UI::ProjectExplorerNode>> &nodes) {
                for (auto &node : nodes) {
                    if (node->visibleIndex >= start && node->visibleIndex <= end) {
                        node->selected = true;
                        if (!node->isGroup) {
                            registry.emplace_or_replace<Canvas::Components::SelectedComponent>(node->sceneEntity);
                        }
                    }
                    if (node->isGroup && !node->children.empty()) {
                        traverse(node->children);
                    }
                }
            };
        traverse(state.nodes);
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
        std::vector<std::shared_ptr<UI::ProjectExplorerNode>> pendingDeletes;
        for (auto &node : nodes) {
            count++;
            node->visibleIndex = i; // Track visible index for shift-selection
            bool clicked = false;
            bool opened = false;

            if (node->isGroup) {
                const auto ret = Widgets::EditableTreeNode(i++,
                                                           node->label,
                                                           node->selected,
                                                           ImGuiTreeNodeFlags_DefaultOpen |
                                                               ImGuiTreeNodeFlags_DrawLinesFull,
                                                           treeIcon,
                                                           ViewportTheme::colors.selectedWire,
                                                           nodePopupName, node->nodeId);
                opened = ret.first;
                clicked = ret.second;

                DEF_NODE_DROP_TARGET(pendingMoves.emplace_back(id, node->nodeId));

                if (ImGui::BeginPopup(nodePopupName)) {
                    if (ImGui::MenuItemEx("Select All", "", "", false, true)) {
                        selectNode(node);
                    }

                    if (ImGui::MenuItemEx("Delete", "", "", false, true)) {
                        pendingDeletes.emplace_back(node);
                    }

                    ImGui::EndPopup();
                }

                if (opened) {
                    if (!node->children.empty()) {
                        count += drawNodes(node->children);
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
                const auto [pressed, cbPressed] = drawLeafNode(
                    i++,
                    node->nodeId,
                    (icon + " " + node->label).c_str(),
                    node->selected,
                    isMultiSelected);
                clicked = pressed;

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

                if (cbPressed) {
                    if (node->selected) {
                        registry.remove<Canvas::Components::SelectedComponent>(entity);
                    } else {
                        registry.emplace_or_replace<Canvas::Components::SelectedComponent>(entity);
                    }
                    // Checkbox interaction counts as selecting
                    lastSelectedIndex = i - 1;
                }
            }

            if (clicked) {
                int currentIndex = i - 1; // i was incremented
                const bool ctrl = ImGui::GetIO().KeyCtrl;
                const bool shift = ImGui::GetIO().KeyShift;

                if (shift && lastSelectedIndex != -1) {
                    int start = std::min(lastSelectedIndex, currentIndex);
                    int end = std::max(lastSelectedIndex, currentIndex);
                    selectRange(start, end);
                } else if (ctrl) {
                    // Toggle selection
                    // FIXME (Shivang): For now ignoring group nodes
                    // if (node->isGroup) {
                    //     node->selected = !node->selected;
                    // } else {
                    if (!node->isGroup) {
                        const auto &entity = node->sceneEntity;
                        if (node->selected) {
                            registry.remove<Canvas::Components::SelectedComponent>(entity);
                        } else {
                            registry.emplace_or_replace<Canvas::Components::SelectedComponent>(entity);
                        }
                    }
                    lastSelectedIndex = currentIndex;
                } else {
                    // Exclusive selection - but handle multi-select drag
                    if (!node->selected) {
                        clearAllSelections();
                        if (node->isGroup) {
                            node->selected = true;
                        } else {
                            registry.emplace_or_replace<Canvas::Components::SelectedComponent>(node->sceneEntity);
                        }
                    }
                    lastSelectedIndex = currentIndex;
                }
            } else {
                bool released = false;

                if (ImGui::IsMouseReleased(0) &&
                    ImGui::IsItemHovered(ImGuiHoveredFlags_None) &&
                    !ImGui::IsMouseDragging(0)) {
                    released = true;
                }

                if (released && !ImGui::GetIO().KeyCtrl && !ImGui::GetIO().KeyShift) {
                    if (node->selected) {
                        clearAllSelections();
                        if (node->isGroup) {
                            node->selected = true;
                        } else {
                            registry.emplace_or_replace<Canvas::Components::SelectedComponent>(node->sceneEntity);
                        }
                        lastSelectedIndex = i - 1;
                    }
                }
            }
        }

        for (const auto &[draggedNodeID, newParentID] : pendingMoves) {
            state.moveNode(draggedNodeID, newParentID);
        }

        for (const auto &node : pendingDeletes) {
            deleteNode(node);
        }

        return count;
    }

    void ProjectExplorer::draw() {
        if (!isShown)
            return;

        if (isfirstTimeDraw) {
            firstTime();
        }

        const ImColor &itemAltBg = ImGui::GetStyle().Colors[ImGuiCol_TableRowBgAlt];
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

        if (ImGui::InvisibleButton("project_explorer_root_drop_target",
                                   ImVec2(window->Size.x, window->Size.y - startY))) {
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

        // Global shortcuts for Project Explorer window
        if (ImGui::IsWindowFocused(ImGuiFocusedFlags_ChildWindows)) {
            const bool ctrl = ImGui::GetIO().KeyCtrl;
            if (ctrl && ImGui::IsKeyPressed(ImGuiKey_G)) {
                if (isMultiSelected) {
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
                } else {
                    auto groupNode = std::make_shared<UI::ProjectExplorerNode>();
                    groupNode->isGroup = true;
                    groupNode->label = "New Group Node";
                    state.addNode(groupNode);
                }
            }

            if (ImGui::IsKeyPressed(ImGuiKey_Delete)) {
                std::vector<std::shared_ptr<UI::ProjectExplorerNode>> nodesToDelete;

                // Add selected entities from registry
                selTagGroup.each([&](const entt::entity entity,
                                     const Canvas::Components::SelectedComponent &,
                                     const Canvas::Components::TagComponent &) {
                    const auto node = state.getNodeFromSceneEntity(entity);
                    if (node != nullptr) {
                        nodesToDelete.emplace_back(node);
                    }
                });

                // Add selected group nodes (requires traversal since they aren't in registry)
                std::function<void(const std::vector<std::shared_ptr<UI::ProjectExplorerNode>> &)> collectGroups =
                    [&](const std::vector<std::shared_ptr<UI::ProjectExplorerNode>> &nodes) {
                        for (const auto &node : nodes) {
                            if (node->isGroup && node->selected) {
                                nodesToDelete.emplace_back(node);
                            }
                            if (!node->children.empty()) {
                                collectGroups(node->children);
                            }
                        }
                    };
                collectGroups(state.nodes);

                for (const auto &node : nodesToDelete) {
                    deleteNode(node);
                }
            }
        }

        ImGui::End();
    }

    std::pair<bool, bool> ProjectExplorer::drawLeafNode(const int key, const uint64_t nodeId,
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
            drawList->AddRectFilled(bgStart, bgEnd, (ImColor)colors[ImGuiCol_TableRowBgAlt], 0);
        }

        const ImRect bb(pos, ImVec2(window->Pos.x + window->Size.x - checkboxWidth - g.Style.FramePadding.x,
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
        drawList->AddText(textStart,
                          IM_COL32(fgColor.x * 255, fgColor.y * 255, fgColor.z * 255, fgColor.w * 255),
                          label);

        if (hovered || cbHovered || multiSelectMode) {
            ImGui::SetCursorPosX(checkBoxPos.x);
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

    void ProjectExplorer::firstTime() {
        isfirstTimeDraw = false;
        state.reset();
    }

    void ProjectExplorer::selectNode(const std::shared_ptr<UI::ProjectExplorerNode> &node) {
        auto scene = Bess::Canvas::Scene::instance();
        auto &registry = scene->getEnttRegistry();

        if (node->isGroup) {
            for (const auto &childNode : node->children) {
                selectNode(childNode);
            }
        } else {
            registry.emplace_or_replace<Canvas::Components::SelectedComponent>(node->sceneEntity);
        }
    }

    void ProjectExplorer::deleteNode(const std::shared_ptr<UI::ProjectExplorerNode> &node, bool firstCall) {
        static std::vector<UUID> entitesToDel;

        if (firstCall) {
            entitesToDel.clear();
        }

        if (node->isGroup) {
            for (const auto &childNode : node->children) {
                if (!childNode)
                    continue;
                deleteNode(childNode, false);
            }
        } else {
            auto scene = Bess::Canvas::Scene::instance();
            auto &registry = scene->getEnttRegistry();
            entitesToDel.emplace_back(scene->getUuidOfEntity(node->sceneEntity));
        }

        if (firstCall) {
            auto scene = Bess::Canvas::Scene::instance();
            auto &registry = scene->getEnttRegistry();
            auto &cmdMgr = scene->getCmdManager();
            auto _ = cmdMgr.execute<Canvas::Commands::DeleteCompCommand,
                                    std::string>(entitesToDel);
            entitesToDel.clear();
            state.removeNode(node);
        }
    }
} // namespace Bess::UI
