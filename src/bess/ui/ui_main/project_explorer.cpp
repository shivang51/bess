#include "ui/ui_main/project_explorer.h"
#include "application/pages/main_page/main_page.h"
#include "common.hpp"
#include "common/bess_uuid.h"
#include "common/helpers.h"
#include "common/logger.h"
#include "imgui.h"
#include "imgui_internal.h"
#include "pages/main_page/cmds/add_comp_cmd.h"
#include "pages/main_page/cmds/delete_comp_cmd.h"
#include "pages/main_page/scene_components/group_scene_component.h"
#include "pages/main_page/scene_components/scene_comp_types.h"
#include "pages/main_page/scene_components/sim_scene_component.h"
#include "simulation_engine.h"
#include "settings/viewport_theme.h"
#include "ui/icons/CodIcons.h"
#include "ui/widgets/m_widgets.h"
#include <cstdint>
#include <memory>
#include <ranges>

namespace Bess::UI {
    static constexpr auto windowName = Common::Helpers::concat(
        Icons::CodIcons::SYMBOL_CLASS, "  Project Explorer");

    ProjectExplorer::ProjectExplorer() : Panel(std::string(windowName.data())) {
        m_defaultDock = Dock::left;
        m_visible = true;
    }

    namespace {
        bool matchesProjectExplorerIoFilter(const std::shared_ptr<Canvas::SceneComponent> &comp,
                                            bool filterInputs,
                                            bool filterOutputs) {
            if (!filterInputs && !filterOutputs) {
                return true;
            }

            const auto simComp = comp ? comp->cast<Canvas::SimulationSceneComponent>() : nullptr;
            if (!simComp) {
                return false;
            }

            const auto simId = simComp->getSimEngineId();
            if (simId == UUID::null) {
                return false;
            }

            auto &simEngine = SimEngine::SimulationEngine::instance();
            if (!simEngine.getSimEngineState().isComponentValid(simId)) {
                return false;
            }

            const auto &definition = simEngine.getComponentDefinition(simId);
            const auto behaviorType = definition->getBehaviorType();
            const bool isInput = behaviorType == SimEngine::ComponentBehaviorType::input;
            const bool isOutput = behaviorType == SimEngine::ComponentBehaviorType::output;

            return (filterInputs && isInput) || (filterOutputs && isOutput);
        }
    } // namespace

    bool ProjectExplorer::shouldDisplayEntity(const UUID &entityId) const {
        auto &sceneState = Pages::MainPage::getInstance()->getState().getSceneDriver()->getState();
        const auto comp = sceneState.getComponentByUuid(entityId);
        if (!comp) {
            return false;
        }

        const bool passesIoFilter = matchesProjectExplorerIoFilter(comp, m_filterInputs, m_filterOutputs);

        bool passesSearch = m_searchQuery.empty();
        if (!passesSearch) {
            const auto query = Common::Helpers::toLowerCase(m_searchQuery);
            const auto name = Common::Helpers::toLowerCase(comp->getName());
            passesSearch = name.find(query) != std::string::npos;
        }

        if (passesIoFilter && passesSearch) {
            return true;
        }

        for (const auto &childId : comp->getChildComponents()) {
            if (shouldDisplayEntity(childId)) {
                return true;
            }
        }

        return false;
    }

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

    void ProjectExplorer::onDraw() {
        const ImColor &itemAltBg = ImGui::GetStyle().Colors[ImGuiCol_TableRowBgAlt];
        const auto &style = ImGui::GetStyle();

        auto &sceneState = Pages::MainPage::getInstance()->getState().getSceneDriver()->getState();

        const auto size = sceneState.getRootComponents().size();
        const auto selSize = sceneState.getSelectedComponents().size();

        const bool isMultiSelected = selSize > 1;

        ImGui::PushItemWidth(ImGui::GetContentRegionAvail().x - 34.f);
        Widgets::TextBox("##ProjectExplorerSearch", m_searchQuery, "Search");
        ImGui::PopItemWidth();
        ImGui::SameLine();
        if (ImGui::Button(Icons::CodIcons::ELLIPSIS)) {
            ImGui::OpenPopup("project_explorer_filters");
        }

        if (ImGui::BeginPopup("project_explorer_filters")) {
            if (ImGui::BeginMenu("Filters")) {
                ImGui::Selectable("Inputs", &m_filterInputs);
                ImGui::Selectable("Outputs", &m_filterOutputs);
                ImGui::EndMenu();
            }
            ImGui::EndPopup();
        }

        const float footerHeight = ImGui::GetTextLineHeightWithSpacing() + style.ItemSpacing.y + style.WindowPadding.y;
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.f, 0.f));
        if (ImGui::BeginChild("project_explorer_list", ImVec2(0.f, -footerHeight), false)) {
            ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0, 0));
            ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(2, 2));

            m_nodesKeyCounter = 0;
            drawEntites(sceneState.getRootComponents());

            const ImGuiContext &g = *ImGui::GetCurrentContext();
            const ImGuiWindow *window = g.CurrentWindow;
            const auto drawList = ImGui::GetWindowDrawList();
            const float height = g.FontSize + (g.Style.FramePadding.y * 2);
            const float x = window->Pos.x;
            float y = window->DC.CursorPos.y;
            ImVec2 bgStart, bgEnd;

            if (m_nodesKeyCounter & 1) { // skipping if its not alternating color row
                y += height;
            }

            while (y < window->Pos.y + window->Size.y) {
                bgStart = ImVec2(x, y);
                bgEnd = ImVec2(x + window->Size.x, y + height);
                drawList->AddRectFilled(bgStart, bgEnd, itemAltBg, 0);
                y += height * 2;
            }

            ImGui::PopStyleVar(2);

            const ImVec2 remainingSpace = ImGui::GetContentRegionAvail();
            if (remainingSpace.x > 0.f && remainingSpace.y > 0.f) {
                if (ImGui::InvisibleButton("project_explorer_root_drop_target", remainingSpace)) {
                    sceneState.clearSelectedComponents();
                }

                HandleNodeDropTarget([&](uint64_t id) {
                    sceneState.detachChild(id);
                });
            }
        }
        ImGui::EndChild();
        ImGui::PopStyleVar();

        ImGui::Spacing();
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

        if (ImGui::BeginPopupContextWindow()) {
            if (isMultiSelected) {
                if (ImGui::MenuItem("Group Selected", "Ctrl-G")) {
                    groupSelectedNodes();
                }
            } else {
                if (ImGui::MenuItem("Create Empty Group", "Ctrl-G")) {
                    const auto group = Canvas::GroupSceneComponent::create("New Group");
                    auto cmd = std::make_unique<Cmd::AddCompCmd<Canvas::GroupSceneComponent>>(group);
                    Pages::MainPage::getInstance()->getState().getCommandSystem().execute(std::move(cmd));
                }
            }

            if (ImGui::MenuItemEx("Regroup on nets", "", "")) {
                groupOnNets();
            }

            ImGui::EndPopup();
        }
    }

    std::pair<bool, bool> ProjectExplorer::drawLeafNode(const size_t key, const uint64_t nodeId,
                                                        const char *label, bool selected,
                                                        const bool multiSelectMode) {
        const ImGuiContext &g = *ImGui::GetCurrentContext();
        const float rounding = g.Style.FrameRounding;
        const float checkboxWidth = ImGui::CalcTextSize("W").x + g.Style.FramePadding.x + 2.f;
        const float rowHeight = g.FontSize + (g.Style.FramePadding.y * 2);
        ImGuiWindow *window = g.CurrentWindow;
        const ImGuiID id = window->GetID(std::to_string(nodeId).c_str());
        const ImGuiID idcb = window->GetID((std::to_string(nodeId) + "cb").c_str());
        const ImVec2 pos = ImGui::GetCursorScreenPos();
        const auto drawList = ImGui::GetWindowDrawList();

        const auto colors = ImGui::GetStyle().Colors;
        const float rowMinX = window->WorkRect.Min.x;
        const float rowMaxX = window->WorkRect.Max.x;

        if ((key & 1) == 0) {
            ImVec2 bgStart(rowMinX, pos.y);
            ImVec2 bgEnd(rowMaxX, pos.y + rowHeight);
            drawList->AddRectFilled(bgStart, bgEnd, (ImColor)colors[ImGuiCol_TableRowBgAlt], 0);
        }

        const float checkboxAreaWidth = checkboxWidth + g.Style.FramePadding.x;
        const ImRect bb(ImVec2(rowMinX, pos.y),
                        ImVec2(rowMaxX - checkboxAreaWidth, pos.y + rowHeight));
        const ImVec2 checkBoxPos(rowMaxX - checkboxWidth, pos.y + g.Style.FramePadding.y);
        const ImRect bbCheckbox(ImVec2(rowMaxX - checkboxAreaWidth, pos.y),
                                ImVec2(rowMaxX, pos.y + rowHeight));

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
            ImVec2 bgStart(rowMinX, pos.y);
            ImVec2 bgEnd(rowMaxX, pos.y + rowHeight);
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
            const ImVec2 cursorPos = ImGui::GetCursorPos();
            ImGui::SetCursorScreenPos(checkBoxPos);
            ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(0, 0));
            ImGui::PushStyleColor(ImGuiCol_FrameBg, colors[ImGuiCol_ButtonHovered]);

            ImGui::Checkbox(("##CheckBox" + std::to_string(nodeId)).c_str(), &selected);
            ImGui::PopStyleVar();
            ImGui::PopStyleColor();
            ImGui::SetCursorPos(cursorPos);
        }

        ImGui::ItemSize(bb, g.Style.FramePadding.y * 2);
        ImGui::ItemAdd(bb, id);

        return {pressed, cbPressed};
    }

    void ProjectExplorer::groupSelectedNodes() {
        auto &mainPageState = Pages::MainPage::getInstance()->getState();
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

            if (!comp)
                continue;

            groupComp->addChildComponent(compId);
        }

        auto &cmdSystem = mainPageState.getCommandSystem();
        cmdSystem.execute(std::make_unique<Cmd::AddCompCmd<Canvas::GroupSceneComponent>>(groupComp));

        BESS_INFO("[ProjectExplorer] Grouped {} selected components into new group.", selComponents.size());
    }

    void ProjectExplorer::groupOnNets() {

        auto &simEngine = SimEngine::SimulationEngine::instance();
        if (!simEngine.isNetUpdated())
            return;

        auto &mainPageState = Pages::MainPage::getInstance()->getState();
        const auto &scene = mainPageState.getSceneDriver().getActiveScene();

        auto &netIdToNameMap = mainPageState.getNetIdToNameMap();
        auto &netIdCompMap = mainPageState.getNetIdToCompMap(scene->getState().getSceneId());

        auto &sceneState = scene->getState();
        std::unordered_map<UUID, std::shared_ptr<Canvas::SimulationSceneComponent>> simIdToComp;

        for (const auto &[compId, comp] : sceneState.getAllComponents()) {
            if (comp->getType() == Canvas::SceneComponentType::simulation ||
                comp->getType() == Canvas::SceneComponentType::module) {
                const auto simComp = comp->cast<Canvas::SimulationSceneComponent>();
                simIdToComp[simComp->getSimEngineId()] = simComp;
            }
        }

        netIdCompMap.clear();
        const auto &nets = simEngine.getNetsMap();
        for (const auto &[netId, net] : nets) {
            for (const auto &simId : net.getComponents()) {
                if (simIdToComp.contains(simId)) {
                    const auto &comp = simIdToComp[simId];
                    netIdCompMap[netId].emplace_back(comp->getUuid());
                    comp->setNetId(netId);
                } else {
                    BESS_WARN("[ProjectExplorer] Simulation component with simId {} not found in scene for net grouping.",
                              (uint64_t)simId);
                }
            }
        }

        // Use groups which get empty instead of creating new ones
        std::vector<UUID> emptyGroups;

        auto &cmdSystem = Pages::MainPage::getInstance()->getState().getCommandSystem();

        int i = 1;
        // move nodes to new groups and ones which get empty
        for (const auto &[netId, components] : netIdCompMap) {
            std::shared_ptr<Canvas::GroupSceneComponent> group = nullptr;
            const bool newGroup = emptyGroups.empty();

            if (emptyGroups.empty()) {
                group = Canvas::GroupSceneComponent::create("Net " + std::to_string(i++));
            } else {
                group = sceneState.getComponentByUuid<Canvas::GroupSceneComponent>(emptyGroups.back());
                emptyGroups.pop_back();
                group->setName("Net " + std::to_string(i++));
            }

            for (const auto &compId : components) {
                group->addChildComponent(compId);
                auto prevParent = sceneState.getComponentByUuid(compId)->getParentComponent();
                if (prevParent != UUID::null && sceneState.isComponentValid(prevParent)) {
                    auto prevParentComp = sceneState.getComponentByUuid(prevParent);
                    prevParentComp->removeChildComponent(compId);
                    if (prevParentComp->getChildComponents().empty()) {
                        emptyGroups.emplace_back(prevParent);
                    }
                }
            }

            netIdToNameMap[netId] = &group->getName();

            if (newGroup) {
                cmdSystem.execute(std::make_unique<Cmd::AddCompCmd<Canvas::GroupSceneComponent>>(group));
            }
        }

        if (!emptyGroups.empty()) {
            cmdSystem.execute(std::make_unique<Cmd::DeleteCompCmd>(emptyGroups));
        }
        BESS_INFO("[ProjectExplorer] Grouped components on nets: created {} groups.", netIdCompMap.size());
    }

    size_t ProjectExplorer::drawEntites(const std::unordered_set<UUID> &entities) {
        constexpr auto treeIcon = Icons::CodIcons::FOLDER;
        constexpr auto moduleIcon = Icons::CodIcons::SYMBOL_MODULE;
        constexpr auto nodePopupName = "node_popup";

        auto &sceneState = Pages::MainPage::getInstance()->getState().getSceneDriver()->getState();
        const auto selSize = sceneState.getSelectedComponents().size();

        size_t count = 0;

        std::vector<std::pair<UUID, UUID>> pendingMoves;

        for (const auto &compId : entities) {
            if (!shouldDisplayEntity(compId)) {
                continue;
            }

            const auto &comp = sceneState.getComponentByUuid(compId);
            const auto type = comp->getType();
            if (!((int8_t)type &
                  (int8_t)Canvas::SceneComponentTypeFlag::showInProjectExplorer))
                continue;

            if (!comp)
                continue;

            bool clicked = false;
            if (comp->getType() == Canvas::SceneComponentType::group) {
                const auto ret = Widgets::EditableTreeNode(m_nodesKeyCounter++,
                                                           comp->getName(),
                                                           comp->getIsSelected(),
                                                           ImGuiTreeNodeFlags_DefaultOpen |
                                                               ImGuiTreeNodeFlags_DrawLinesFull,
                                                           treeIcon,
                                                           ViewportTheme::colors.selectedWire,
                                                           nodePopupName,
                                                           comp->getUuid());

                count++;
                const auto opened = ret.first;
                clicked = ret.second;

                HandleNodeDropTarget([&](uint64_t id) { //\n (just for formatting)
                    pendingMoves.emplace_back(id,
                                              compId //\n
                    );
                });

                if (opened) {
                    count += drawEntites(comp->getChildComponents());
                    ImGui::TreePop();
                }

            } else {
                const bool isModule = comp->getType() == Canvas::SceneComponentType::module;

                const auto name = std::format("{} {}", comp->getIcon(), comp->getName());

                if (isModule) {
                    const auto &moduleColor = ViewportTheme::colors.moduleColor;
                    ImGui::PushStyleColor(ImGuiCol_Text,
                                          ImVec4(moduleColor.x, moduleColor.y, moduleColor.z, moduleColor.w));
                }

                const auto &[pressed, cbPressed] = drawLeafNode(m_nodesKeyCounter++,
                                                                compId,
                                                                name.c_str(),
                                                                comp->getIsSelected(),
                                                                selSize > 1);
                if (isModule) {
                    ImGui::PopStyleColor();
                }

                count++;
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
                        sceneState.removeSelectedComponent(compId);
                    } else {
                        sceneState.addSelectedComponent(compId);
                    }
                    m_lastSelectedIndex = (int32_t)m_nodesKeyCounter - 1;
                }
            }

            if (clicked) {
                int32_t currentIndex = (int32_t)m_nodesKeyCounter - 1;
                const bool ctrl = ImGui::GetIO().KeyCtrl;
                const bool shift = ImGui::GetIO().KeyShift;

                if (shift && m_lastSelectedIndex != -1) {
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
                            sceneState.removeSelectedComponent(compId);
                        } else {
                            sceneState.addSelectedComponent(compId);
                        }
                    }
                    m_lastSelectedIndex = currentIndex;
                } else {
                    // selects the clicked component and deselects all other components
                    sceneState.clearSelectedComponents();
                    sceneState.addSelectedComponent(compId);
                    m_lastSelectedIndex = currentIndex;
                }
            }
        }

        for (const auto &[draggedNodeID, newParentID] : pendingMoves) {
            sceneState.attachChild(newParentID, draggedNodeID);
        }

        return count;
    }
} // namespace Bess::UI
