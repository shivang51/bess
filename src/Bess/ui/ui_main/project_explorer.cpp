#include "ui/ui_main/project_explorer.h"
#include "bess_uuid.h"
#include "event_dispatcher.h"
#include "imgui.h"
#include "imgui_internal.h"
#include "scene/commands/create_group_command.h"
#include "scene/commands/delete_node_command.h"
#include "scene/commands/reparent_node_command.h"
#include "scene/components/components.h"
#include "scene/scene.h"
#include "scene/scene_state/components/sim_scene_component.h"
#include "ui/ui_main/project_explorer_state.h"
#include "ui/widgets/m_widgets.h"
#include <cstdint>
#include <memory>
#include <ranges>

namespace Bess::UI {
    bool ProjectExplorer::isShown = true;
    bool ProjectExplorer::isfirstTimeDraw = true;
    ProjectExplorerState ProjectExplorer::state{};

    template <typename Func>
    void HandleNodeDropTarget(Func op) {
        if (ImGui::BeginDragDropTarget()) {
            if (const ImGuiPayload *payload = ImGui::AcceptDragDropPayload("TREE_NODE_PAYLOAD")) {
                const auto dataPtr = (uint64_t *)payload->Data;
                const auto size = (payload->DataSize / sizeof(uint64_t));
                const auto draggedNodeIDs = std::vector<uint64_t>(dataPtr, dataPtr + size);
                for (const auto &id : draggedNodeIDs) {
                    op(id);
                }
            }
            ImGui::EndDragDropTarget();
        }
    }

    void ProjectExplorer::init() {
        EventSystem::EventDispatcher::instance().sink<Bess::Events::ComponentCreatedEvent>().connect<&ProjectExplorer::onEntityCreated>();
        EventSystem::EventDispatcher::instance().sink<Bess::Events::EntityDestroyedEvent>().connect<&ProjectExplorer::onEntityDestroyed>();
    }

    void ProjectExplorer::onEntityCreated(const Bess::Events::ComponentCreatedEvent &e) {
        auto scene = Bess::Canvas::Scene::instance();
        const auto &sceneState = scene->getState();
        const auto &comp = sceneState.getComponentByUuid(e.uuid);

        auto node = std::make_shared<UI::ProjectExplorerNode>();
        node->sceneId = e.uuid;
        node->label = comp->getName();
        node->selected = false;
        node->multiSelectMode = false;

        state.addNode(node);
    }

    void ProjectExplorer::onEntityDestroyed(const Bess::Events::EntityDestroyedEvent &e) {
        auto node = state.getNodeOfSceneEntt(e.uuid);
        if (node) {
            state.removeNode(node);
        }
    }

    int ProjectExplorer::lastSelectedIndex = -1;

    void ProjectExplorer::clearAllSelections() {
        auto scene = Bess::Canvas::Scene::instance();
        auto &sceneState = scene->getState();

        sceneState.clearSelectedComponents();

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
        auto &sceneState = scene->getState();

        clearAllSelections();

        // Note (Shivang): Can't use auto here because of recursive lambda
        std::function<void(std::vector<std::shared_ptr<UI::ProjectExplorerNode>> &)> traverse =
            [&](std::vector<std::shared_ptr<UI::ProjectExplorerNode>> &nodes) {
                for (auto &node : nodes) {
                    if (node->visibleIndex >= start && node->visibleIndex <= end) {
                        node->selected = true;
                        if (!node->isGroup) {
                            sceneState.addSelectedComponent(node->sceneId);
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

        auto &sceneState = scene->getState();

        const auto selSize = sceneState.getSelectedComponents().size();
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

                HandleNodeDropTarget([&](uint64_t id) { pendingMoves.emplace_back(id, node->nodeId); });

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
                const auto &sceneComp = sceneState.getComponentByUuid(node->sceneId);
                if (sceneComp->getType() == Canvas::SceneComponentType::simulation) {
                    // icon = Common::Helpers::getComponentIcon(tagComp.type.simCompHash);
                } else {
                    // icon = Common::Helpers::getComponentIcon(tagComp.type.nsCompType);
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
                    for (auto &selId : sceneState.getSelectedComponents() | std::views::keys) {
                        const auto selectedNode = state.getNodeOfSceneEntt(selId);
                        if (selectedNode != nullptr) {
                            payloadData.emplace_back((uint64_t)selectedNode->nodeId);
                        }
                    }
                    ImGui::SetDragDropPayload("TREE_NODE_PAYLOAD", payloadData.data(),
                                              payloadData.size() * sizeof(uint64_t));
                    ImGui::Text("Dragging %lu nodes", payloadData.size());
                    ImGui::EndDragDropSource();
                }

                node->selected = sceneState.getSelectedComponents().contains(node->sceneId);

                if (cbPressed) {
                    if (node->selected) {
                        sceneState.removeSelectedComponent(node->sceneId);
                    } else {
                        sceneState.addSelectedComponent(node->sceneId);
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
                        if (node->selected) {
                            sceneState.removeSelectedComponent(node->sceneId);
                        } else {
                            sceneState.addSelectedComponent(node->sceneId);
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
                            sceneState.addSelectedComponent(node->sceneId);
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
                            sceneState.addSelectedComponent(node->sceneId);
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

        auto &sceneState = scene->getState();

        const auto size = sceneState.getRootComponents().size();
        const auto selSize = sceneState.getSelectedComponents().size();

        const bool isMultiSelected = selSize > 1;

        if (size == 0) {
            ImGui::Text("No Components Added");
        } else if (size == 1) {
            ImGui::Text("%lu Component Added", size);
        } else {
            ImGui::Text("%lu Components Added", size);
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

        HandleNodeDropTarget([&](uint64_t id) {
            state.moveNode(id, UUID::null);
        });

        if (ImGui::BeginPopupContextWindow()) {
            if (isMultiSelected) {
                if (ImGui::MenuItem("Group Selected", "Ctrl-G")) {
                    groupSelectedNodes();
                }
            } else {
                if (ImGui::MenuItem("Create Empty Group", "Ctrl-G")) {
                    auto scene = Bess::Canvas::Scene::instance();
                    auto _ = scene->getCmdManager().execute<Canvas::Commands::CreateGroupCommand, UUID>("New Group");
                }
            }

            if (ImGui::MenuItemEx("Regroup on nets", "", "")) {
                groupOnNets();
            }

            ImGui::EndPopup();
        }

        // Global shortcuts for Project Explorer window
        if (ImGui::IsWindowFocused(ImGuiFocusedFlags_ChildWindows)) {
            const bool ctrl = ImGui::GetIO().KeyCtrl;
            if (ctrl && ImGui::IsKeyPressed(ImGuiKey_G)) {
                groupSelectedNodes();
            }

            if (ImGui::IsKeyPressed(ImGuiKey_Delete)) {
                std::vector<std::shared_ptr<UI::ProjectExplorerNode>> nodesToDelete;

                for (const auto &selId : sceneState.getSelectedComponents() | std::views::keys) {
                    const auto node = state.getNodeOfSceneEntt(selId);
                    if (node != nullptr) {
                        nodesToDelete.emplace_back(node);
                    }
                }

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

        if (node->isGroup) {
            for (const auto &childNode : node->children) {
                selectNode(childNode);
            }
        } else {
            scene->getState().addSelectedComponent(node->sceneId);
        }
    }

    void ProjectExplorer::deleteNode(const std::shared_ptr<UI::ProjectExplorerNode> &node, bool firstCall) {
        static std::vector<UUID> entitesToDel;

        if (firstCall) {
            entitesToDel.clear();
        }

        // Collect entities and nodes recursively
        auto scene = Bess::Canvas::Scene::instance();

        // Always add the current node ID to be deleted (whether it's a group or entity)
        entitesToDel.emplace_back(node->nodeId);

        if (node->isGroup) {
            for (const auto &childNode : node->children) {
                if (!childNode)
                    continue;
                deleteNode(childNode, false);
            }
        }

        if (firstCall && !entitesToDel.empty()) {
            auto &cmdMgr = scene->getCmdManager();
            // Use generic DeleteNodeCommand which handles both entities and groups
            auto _ = cmdMgr.execute<Canvas::Commands::DeleteNodeCommand,
                                    std::any>(entitesToDel);
            entitesToDel.clear();
        }
    }

    void ProjectExplorer::groupSelectedNodes() {
        auto scene = Bess::Canvas::Scene::instance();

        const auto &selComponents = scene->getState().getSelectedComponents();

        if (selComponents.empty())
            return;

        auto res = scene->getCmdManager().execute<Canvas::Commands::CreateGroupCommand, UUID>("New Group");

        if (res.has_value()) {
            UUID groupUUID = res.value();
            // Reparent selected nodes to this group
            for (const auto &selId : selComponents | std::views::keys) {
                auto id = state.getNodeOfSceneEntt(selId)->nodeId;
                auto cmd = std::make_unique<Canvas::Commands::ReparentNodeCommand>(id, groupUUID);
                scene->getCmdManager().execute(std::move(cmd));
            }
        }
    }

    void ProjectExplorer::groupOnNets() {
        std::unordered_map<UUID, std::vector<UUID>> netGroups;
        auto scene = Bess::Canvas::Scene::instance();

        auto &sceneState = scene->getState();
        const auto &selComponents = sceneState.getSelectedComponents();

        for (auto &selId : selComponents | std::views::keys) {
            const auto &comp = sceneState.getComponentByUuid<Canvas::SimulationSceneComponent>(selId);
            const auto &netId = comp->getNetId();
            if (netId != UUID::null) {
                netGroups[netId].emplace_back(netId);
            }
        }

        state.netIdToNameMap.clear();
        int i = 1;
        for (const auto &[netId, entities] : netGroups) {
            auto res = scene->getCmdManager().execute<Canvas::Commands::CreateGroupCommand, UUID>(std::format("Net {}", i++));

            if (res.has_value()) {
                UUID groupUUID = res.value();
                state.netIdToNameMap[netId] = state.nodesLookup[groupUUID]->label;

                for (const auto &enttId : entities) {
                    auto cmd = std::make_unique<Canvas::Commands::ReparentNodeCommand>(enttId, groupUUID);
                    scene->getCmdManager().execute(std::move(cmd));
                }
            }
        }
    }
} // namespace Bess::UI
