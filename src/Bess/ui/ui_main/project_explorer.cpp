#include "ui/ui_main/project_explorer.h"
#include "application/pages/main_page/main_page.h"
#include "bess_uuid.h"
#include "event_dispatcher.h"
#include "imgui.h"
#include "imgui_internal.h"
#include "pages/main_page/cmds/add_comp_cmd.h"
#include "pages/main_page/scene_components/group_scene_component.h"
#include "pages/main_page/scene_components/scene_comp_types.h"
#include "pages/main_page/scene_components/sim_scene_component.h"
#include "scene/commands/create_group_command.h"
#include "scene/commands/delete_node_command.h"
#include "settings/viewport_theme.h"
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
        EventSystem::EventDispatcher::instance().sink<Bess::Canvas::Events::ComponentAddedEvent>().connect<&ProjectExplorer::onEntityCreated>();
        EventSystem::EventDispatcher::instance().sink<Bess::Canvas::Events::ComponentRemovedEvent>().connect<&ProjectExplorer::onEntityDestroyed>();
    }

    void ProjectExplorer::onEntityCreated(const Bess::Canvas::Events::ComponentAddedEvent &e) {
        if (!((uint8_t)e.type & (uint8_t)Canvas::SceneComponentTypeFlag::showInProjectExplorer)) {
            return;
        }

        auto &sceneState = Pages::MainPage::getTypedInstance()->getState().getSceneDriver()->getState();
        const auto &comp = sceneState.getComponentByUuid(e.uuid);

        auto node = std::make_shared<UI::ProjectExplorerNode>();
        node->sceneId = e.uuid;
        node->label = comp->getName();
        node->selected = false;
        node->multiSelectMode = false;

        state.addNode(node);
    }

    void ProjectExplorer::onEntityDestroyed(const Bess::Canvas::Events::ComponentRemovedEvent &e) {
        if (!((uint8_t)e.type & (uint8_t)Canvas::SceneComponentTypeFlag::showInProjectExplorer)) {
            return;
        }

        auto node = state.getNodeOfSceneEntt(e.uuid);
        if (node) {
            state.removeNode(node);
        }
    }

    int32_t ProjectExplorer::lastSelectedIndex = -1;
    size_t ProjectExplorer::nodesKeyCounter = 0;

    void ProjectExplorer::clearAllSelections() {
        auto &sceneState = Pages::MainPage::getTypedInstance()->getState().getSceneDriver()->getState();

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
        auto &sceneState = Pages::MainPage::getTypedInstance()->getState().getSceneDriver()->getState();

        sceneState.clearSelectedComponents();

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

        auto &sceneState = Pages::MainPage::getTypedInstance()->getState().getSceneDriver()->getState();

        const auto selSize = sceneState.getSelectedComponents().size();
        const bool isMultiSelected = selSize > 1;

        size_t count = 0;
        if (isRoot) {
            i = 0;
        }

        std::vector<std::pair<UUID, UUID>> pendingMoves;
        for (auto &node : nodes) {
            count++;
            node->visibleIndex = i; // Track visible index for shift-selection
            bool clicked = false;
            bool opened = false;

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

        auto &sceneState = Pages::MainPage::getTypedInstance()->getState().getSceneDriver()->getState();

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

        nodesKeyCounter = 0;
        drawEntites(sceneState.getRootComponents());

        const ImGuiContext &g = *ImGui::GetCurrentContext();
        const ImGuiWindow *window = g.CurrentWindow;
        const auto drawList = ImGui::GetWindowDrawList();
        const auto colors = ImGui::GetStyle().Colors;
        const float height = g.FontSize + (g.Style.FramePadding.y * 2);
        const float x = window->Pos.x;
        float y = window->DC.CursorPos.y;
        float startY = y;
        ImVec2 bgStart, bgEnd;

        if (nodesKeyCounter & 1) { // skipping if its not alternating color row
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
            sceneState.clearSelectedComponents();
            for (auto &node : state.nodes) {
                node->selected = false;
            }
        }

        HandleNodeDropTarget([&](uint64_t id) {
            sceneState.detachChild(id);
        });

        if (ImGui::BeginPopupContextWindow()) {
            if (isMultiSelected) {
                if (ImGui::MenuItem("Group Selected", "Ctrl-G")) {
                    groupSelectedNodes();
                }
            } else {
                if (ImGui::MenuItem("Create Empty Group", "Ctrl-G")) {
                    auto &scene = Pages::MainPage::getTypedInstance()->getState().getSceneDriver();
                    auto _ = scene->getCmdManager().execute<Canvas::Commands::CreateGroupCommand, UUID>("New Group");
                }
            }

            if (ImGui::MenuItemEx("Regroup on nets", "", "")) {
                groupOnNets();
            }

            ImGui::EndPopup();
        }

        ImGui::End();
    }

    std::pair<bool, bool> ProjectExplorer::drawLeafNode(const size_t key, const uint64_t nodeId,
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
            bgColor = colors[ImGuiCol_ButtonHovered];
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
            ImGui::PushStyleColor(ImGuiCol_FrameBg, colors[ImGuiCol_ButtonHovered]);

            ImGui::Checkbox(("##CheckBox" + std::to_string(nodeId)).c_str(), &selected);
            ImGui::PopStyleVar();
            ImGui::PopStyleColor();
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
        auto &scene = Pages::MainPage::getTypedInstance()->getState().getSceneDriver();

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
        auto &scene = Pages::MainPage::getTypedInstance()->getState().getSceneDriver();

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
                                    std::string>(entitesToDel);
            entitesToDel.clear();
        }
    }

    void ProjectExplorer::groupSelectedNodes() {
        auto &mainPageState = Pages::MainPage::getTypedInstance()->getState();
        auto &scene = mainPageState.getSceneDriver();
        auto &sceneState = scene->getState();

        const auto &selComponents = sceneState.getSelectedComponents() |
                                    std::views::keys |
                                    std::ranges::to<std::vector>();

        if (selComponents.empty())
            return;

        auto groupComp = Canvas::GroupSceneComponent::create("New Group");

        for (const auto &compId : selComponents) {
            auto comp = scene->getState().getComponentByUuid(compId);

            // Only root components can be grouped
            if (!comp || !sceneState.isRootComponent(compId))
                continue;

            groupComp->addChildComponent(compId);
        }

        auto &cmdSystem = mainPageState.getCommandSystem();
        cmdSystem.execute(std::make_unique<Cmd::AddCompCmd<Canvas::GroupSceneComponent>>(groupComp));

        BESS_INFO("[ProjectExplorer] Grouped {} selected components into new group.", selComponents.size());
    }

    void ProjectExplorer::groupOnNets() {
        std::unordered_map<UUID, std::vector<UUID>> netGroups;

        auto &scene = Pages::MainPage::getTypedInstance()->getState().getSceneDriver();
        auto &sceneState = scene->getState();

        // preparing new groups for nodes
        for (const auto &node : state.nodesLookup | std::views::values) {
            if (node->isGroup)
                continue;
            const auto &comp = sceneState.getComponentByUuid<Canvas::SimulationSceneComponent>(node->sceneId);
            if (!comp)
                continue;
            const auto &netId = comp->getNetId();
            if (netId != UUID::null) {
                netGroups[netId].emplace_back(node->nodeId);
            }
        }

        state.netIdToNameMap.clear();
        std::vector<UUID> emptyGroups;

        // move nodes to new groups
        int i = 1;
        for (const auto &[netId, nodes] : netGroups) {
            auto res = scene->getCmdManager().execute<Canvas::Commands::CreateGroupCommand, UUID>(std::format("Net {}", i++));

            if (res.has_value()) {
                UUID newGroupId = res.value();
                state.netIdToNameMap[netId] = state.nodesLookup[newGroupId]->label;

                for (const auto &nodeId : nodes) {
                    const auto &node = state.nodesLookup[nodeId];
                    if (node->parentId != UUID::null) {
                        const auto &parentNode = state.nodesLookup[node->parentId];
                        BESS_ASSERT(parentNode->isGroup, "Parent node is expected to be a group");
                        // if the parent group will be empty after moving this node, mark it for deletion
                        if (parentNode->children.size() == 1) {
                            emptyGroups.emplace_back(parentNode->nodeId);
                        }
                    }

                    state.moveNode(nodeId, newGroupId);
                }
            }
        }

        // delete empty groups
        for (const auto &groupId : emptyGroups) {
            const auto &groupNode = state.nodesLookup[groupId];
            if (groupNode->children.empty()) {
                deleteNode(groupNode);
            }
        }
        BESS_INFO("[ProjectExplorer] Grouped components on nets: created {} groups.", netGroups.size());
    }

    size_t ProjectExplorer::drawEntites(const std::unordered_set<UUID> &entities) {
        constexpr auto treeIcon = Icons::CodIcons::FOLDER;

        auto &sceneState = Pages::MainPage::getTypedInstance()->getState().getSceneDriver()->getState();
        const auto selSize = sceneState.getSelectedComponents().size();

        size_t count = 0;

        std::vector<std::pair<UUID, UUID>> pendingMoves;

        for (const auto &entt : entities) {
            const auto &comp = sceneState.getComponentByUuid(entt);
            const auto type = comp->getType();
            if (!((int8_t)type &
                  (int8_t)Canvas::SceneComponentTypeFlag::showInProjectExplorer))
                continue;

            if (!comp)
                continue;

            bool clicked = false;
            if (comp->getType() == Canvas::SceneComponentType::group) {
                const auto ret = Widgets::EditableTreeNode(nodesKeyCounter++,
                                                           comp->getName(),
                                                           comp->getIsSelected(),
                                                           ImGuiTreeNodeFlags_DefaultOpen |
                                                               ImGuiTreeNodeFlags_DrawLinesFull,
                                                           treeIcon,
                                                           ViewportTheme::colors.selectedWire,
                                                           "",
                                                           comp->getUuid());

                const auto opened = ret.first;
                clicked = ret.second;

                HandleNodeDropTarget([&](uint64_t id) { //\n (just for formatting)
                    pendingMoves.emplace_back(id,
                                              entt //\n
                    );
                });

                count++;
                if (opened) {
                    count += drawEntites(comp->getChildComponents());
                    ImGui::TreePop();
                }

            } else {
                const auto &[pressed, cbPressed] = drawLeafNode(nodesKeyCounter++,
                                                                entt,
                                                                comp->getName().c_str(),
                                                                comp->getIsSelected(),
                                                                selSize > 1);

                clicked = pressed;
                if (ImGui::BeginDragDropSource()) {
                    std::vector<uint64_t> payloadData;
                    for (auto &selId : sceneState.getSelectedComponents() | std::views::keys) {
                        const auto selectedComp = sceneState.getComponentByUuid(selId);
                        if (selectedComp) {
                            payloadData.emplace_back(selId);
                        }
                    }
                    ImGui::SetDragDropPayload("TREE_NODE_PAYLOAD", payloadData.data(),
                                              payloadData.size() * sizeof(uint64_t));
                    ImGui::Text("Dragging %lu nodes", payloadData.size());
                    ImGui::EndDragDropSource();
                }

                if (cbPressed) {
                    if (comp->getIsSelected()) {
                        sceneState.removeSelectedComponent(entt);
                    } else {
                        sceneState.addSelectedComponent(entt);
                    }
                    lastSelectedIndex = (int32_t)nodesKeyCounter - 1;
                }

                count++;
            }

            if (clicked) {
                int32_t currentIndex = (int32_t)nodesKeyCounter - 1;
                const bool ctrl = ImGui::GetIO().KeyCtrl;
                const bool shift = ImGui::GetIO().KeyShift;

                if (shift && lastSelectedIndex != -1) {
                    // TODO (Shivang): Fix range selection
                    // int start = std::min(lastSelectedIndex, currentIndex);
                    // int end = std::max(lastSelectedIndex, currentIndex);
                    // selectRange(start, end);
                } else if (ctrl) {
                    // Toggle selection
                    // FIXME (Shivang): For now ignoring group nodes
                    // if (node->isGroup) {
                    //     node->selected = !node->selected;
                    // } else {
                    if (comp->getType() != Canvas::SceneComponentType::group) {
                        if (comp->getIsSelected()) {
                            sceneState.removeSelectedComponent(entt);
                        } else {
                            sceneState.addSelectedComponent(entt);
                        }
                    }
                    lastSelectedIndex = currentIndex;
                } else {
                    // Exclusive selection - but handle multi-select drag
                    if (!comp->getIsSelected()) {
                        sceneState.clearSelectedComponents();
                        if (comp->getType() == Canvas::SceneComponentType::group) {
                            comp->setIsSelected(true);
                        } else {
                            sceneState.addSelectedComponent(entt);
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
                    if (comp->getIsSelected()) {
                        sceneState.clearSelectedComponents();
                        if (comp->getType() == Canvas::SceneComponentType::group) {
                            comp->setIsSelected(true);
                        } else {
                            sceneState.addSelectedComponent(entt);
                        }
                        lastSelectedIndex = (int32_t)nodesKeyCounter - 1;
                    }
                }
            }
        }

        for (const auto &[draggedNodeID, newParentID] : pendingMoves) {
            sceneState.attachChild(newParentID, draggedNodeID);
        }

        return count;
    }
} // namespace Bess::UI
