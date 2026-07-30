#include "ui/ui_main/project_explorer.h"
#include "bess_core/g_app_context.h"
#include "bess_core/settings/viewport_theme.h"
#include "common/bess_uuid.h"
#include "common/helpers.h"
#include "common/logger.h"
#include "common/types.h"
#include "dig_sim_driver.h"
#include "imgui.h"
#include "imgui_internal.h"
#include "pages/main_page/main_page.h"
#include "pages/main_page/scene_components/group_scene_component.h"
#include "pages/main_page/scene_components/scene_comp_types.h"
#include "pages/main_page/scene_components/sim_scene_component.h"
#include "project_session/project_session.h"
#include "simulation_engine.h"
#include "ui/icons/CodIcons_Remapped.h"
#include "ui/icons/FontAwesomeIcons_Remapped.h"
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
        bool matchesProjectExplorerIoFilter(const Canvas::SceneComponent *comp,
                                            bool filterInputs,
                                            bool filterOutputs) {
            if (!filterInputs && !filterOutputs) {
                return true;
            }

            const auto simComp =
                comp ? dynamic_cast<const Canvas::SimulationSceneComponent *>(
                           comp)
                     : nullptr;
            if (!simComp) {
                return false;
            }

            const auto simId = simComp->getSimEngineId();
            if (simId == UUID::null) {
                return false;
            }

            auto &appCtx = Bess::GAppContext::getInstance();
            auto projectCtx = appCtx.getSubSystem<Bess::ProjectSession>();
            auto &simEngine = projectCtx->sim();
            if (!simEngine.getComponent<SimEngine::Drivers::SimComponent>(
                    simId)) {
                return false;
            }

            const auto &def = simEngine.getComponentDefinition(simId);

            const auto &definition = std::dynamic_pointer_cast<
                SimEngine::Drivers::Digital::DigCompDef>(def);

            const auto behaviorType = definition->getBehaviorType();
            const bool isInput =
                behaviorType == SimEngine::ComponentBehaviorType::input;
            const bool isOutput =
                behaviorType == SimEngine::ComponentBehaviorType::output;

            return (filterInputs && isInput) || (filterOutputs && isOutput);
        }
    } // namespace

    bool ProjectExplorer::shouldDisplayEntity(const UUID &entityId) const {

        auto driver = GAppContext::getInstance()
                          .getSubSystem<Bess::ProjectSession>()
                          ->getSubSystem<SceneDriver>();

        if (driver->getIsPaused()) {
            return false;
        }

        auto &sceneState = driver->getActiveScene()->getState();
        const auto comp = sceneState.getComponentByUuid(entityId);
        if (!comp) {
            return false;
        }

        const bool passesIoFilter = matchesProjectExplorerIoFilter(
            comp, m_filterInputs, m_filterOutputs);

        bool passesSearch = m_searchQuery.empty();
        if (!passesSearch) {
            const auto query = Common::Helpers::toLowerCase(m_searchQuery);
            const auto name = Common::Helpers::toLowerCase(comp->getName());
            passesSearch = name.contains(query);
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

    std::vector<UUID> takeDropIds() {
        std::vector<UUID> ids;
        if (ImGui::BeginDragDropTarget()) {
            if (const ImGuiPayload *payload =
                    ImGui::AcceptDragDropPayload("TREE_NODE_PAYLOAD")) {
                const auto dataPtr =
                    static_cast<const std::uint64_t *>(payload->Data);
                const auto size = (payload->DataSize / sizeof(uint64_t));
                ids.reserve(static_cast<std::size_t>(size));
                for (int i = 0; i < size; ++i) {
                    ids.emplace_back(dataPtr[i]);
                }
            }
            ImGui::EndDragDropTarget();
        }
        return ids;
    }

    void parentComps(Canvas::SceneState &state,
                     const std::vector<std::pair<UUID, UUID>> &moves) {
        if (moves.empty()) {
            return;
        }

        auto &session =
            *GAppContext::getInstance().getSubSystem<ProjectSession>();
        auto tx = session.tx("Reparent components");
        for (const auto &[id, parent] : moves) {
            const auto comp = state.getComponentByUuid(id);
            if (!comp || comp->getParentComponent() == parent) {
                continue;
            }
            const auto status = tx.parentComp(id, parent, state.getSceneId());
            if (!status) {
                tx.cancel();
                BESS_WARN("Could not prepare component reparent: {}",
                          status.msg());
                return;
            }
        }

        if (tx.isEmpty()) {
            tx.cancel();
            return;
        }
        const auto result = tx.commit();
        if (!result) {
            BESS_WARN("Could not reparent components: {}", result.status.msg());
        }
    }

    void ProjectExplorer::onBeforeDraw() {
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.f, 2.f));
    }

    void ProjectExplorer::onAfterDraw() {
        ImGui::PopStyleVar();
    }

    void ProjectExplorer::onDraw() {
        const ImColor &itemAltBg =
            ImGui::GetStyle().Colors[ImGuiCol_TableRowBgAlt];
        const auto &style = ImGui::GetStyle();

        auto driver = GAppContext::getInstance()
                          .getSubSystem<Bess::ProjectSession>()
                          ->getSubSystem<SceneDriver>();
        auto &sceneState = driver->getActiveScene()->getState();

        const auto size = sceneState.getRootComponents().size();
        const auto selSize = sceneState.getSelectedComponents().size();

        m_isMultiSelected = selSize > 1;

        ImGui::PushItemWidth(ImGui::GetContentRegionAvail().x - 34.f);
        ImGui::SetCursorPosX(ImGui::GetCursorPosX() + 4.f);
        Widgets::TextBox("##ProjectExplorerSearch", m_searchQuery, "Search");
        ImGui::PopItemWidth();
        ImGui::SameLine();
        if (ImGui::Button(Icons::FontAwesomeIcons::FA_ELLIPSIS_VERTICAL)) {
            ImGui::OpenPopup("project_explorer_filters");
        }

        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(8.f, 4.f));
        if (ImGui::BeginPopup("project_explorer_filters")) {
            constexpr auto filterTitle = Common::Helpers::concat(
                Icons::FontAwesomeIcons::FA_FILTER, " Filters");
            if (ImGui::BeginMenu(filterTitle.data())) {
                ImGui::Selectable("Inputs",
                                  &m_filterInputs,
                                  ImGuiSelectableFlags_DontClosePopups);
                ImGui::Selectable("Outputs",
                                  &m_filterOutputs,
                                  ImGuiSelectableFlags_DontClosePopups);
                ImGui::EndMenu();
            }
            ImGui::EndPopup();
        }
        ImGui::PopStyleVar();

        const float footerHeight = ImGui::GetTextLineHeightWithSpacing() +
                                   (style.ItemSpacing.y * 2) +
                                   style.WindowPadding.y;
        if (ImGui::BeginChild(
                "project_explorer_list", ImVec2(0.f, -footerHeight), false)) {
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

            if (m_nodesKeyCounter &
                1) { // skipping if its not alternating color row
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
                if (ImGui::InvisibleButton("project_explorer_root_drop_target",
                                           remainingSpace)) {
                    sceneState.clearSelectedComponents();
                }

                std::vector<std::pair<UUID, UUID>> moves;
                for (const auto id : takeDropIds()) {
                    moves.emplace_back(id, UUID::null);
                }
                parentComps(sceneState, moves);
            }
            drawContextMenu();
        }
        ImGui::EndChild();

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::SetCursorPosX(ImGui::GetCursorPosX() + 4.f);
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
    }

    void ProjectExplorer::drawContextMenu() {
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(8.f, 4.f));
        if (ImGui::BeginPopupContextWindow()) {
            if (m_isMultiSelected) {
                if (ImGui::MenuItem("Group Selected", "Ctrl-G")) {
                    groupSelectedNodes();
                }
            } else {
                if (ImGui::MenuItem("Create Empty Group", "Ctrl-G")) {
                    const auto group =
                        Canvas::GroupSceneComponent::create("New Group");
                    auto &session = *GAppContext::getInstance()
                                         .getSubSystem<ProjectSession>();
                    const auto result = session.addComp(group);
                    if (!result) {
                        BESS_WARN("Could not add group: {}",
                                  result.status.msg());
                    }
                }
            }

            if (ImGui::MenuItemEx("Regroup on nets", "", "")) {
                groupOnNets();
            }

            ImGui::EndPopup();
        }
        ImGui::PopStyleVar();
    }

    bool ProjectExplorer::drawLeafNode(const size_t key,
                                       const uint64_t nodeId,
                                       const char *label,
                                       bool selected,
                                       const bool multiSelectMode) {
        const ImGuiContext &g = *ImGui::GetCurrentContext();
        const float rounding = g.Style.FrameRounding;
        ImGuiWindow *window = g.CurrentWindow;
        const ImGuiID id = window->GetID(std::to_string(nodeId).c_str());
        const ImVec2 pos = window->DC.CursorPos;
        const auto drawList = ImGui::GetWindowDrawList();

        const auto colors = ImGui::GetStyle().Colors;
        const float rowMinX = window->WorkRect.Min.x;
        const float rowMaxX = window->WorkRect.Max.x;

        if ((key & 1) == 0) {
            float x = window->Pos.x;
            float y = pos.y;
            ImVec2 avail = ImGui::GetContentRegionAvail();
            ImVec2 bgStart(x, y);
            ImVec2 bgEnd(x + window->Size.x,
                         y + g.FontSize + (g.Style.FramePadding.y * 2));
            drawList->AddRectFilled(
                bgStart, bgEnd, (ImColor)colors[ImGuiCol_TableRowBgAlt], 0);
        }

        const ImRect bb(
            pos,
            ImVec2(window->Pos.x + window->Size.x - g.Style.FramePadding.x,
                   pos.y + g.FontSize + (g.Style.FramePadding.y * 2)));

        bool hovered = false, held = false;
        const auto pressed = ImGui::ButtonBehavior(
            bb, id, &hovered, &held, ImGuiButtonFlags_PressedOnClick);

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
            ImVec2 bgEnd(x + window->Size.x,
                         y + g.FontSize + (g.Style.FramePadding.y * 2));

            bgColor.w = 200.f / 255.f;
            const auto color = ImGui::GetColorU32(bgColor);
            drawList->AddRectFilled(bgStart, bgEnd, color, 0);
        }

        const auto fgColor = colors[ImGuiCol_Text];

        auto textStart = bb.Min;
        textStart.y += g.Style.FramePadding.y;
        textStart.x += g.Style.FramePadding.x;
        drawList->AddText(textStart,
                          IM_COL32(fgColor.x * 255,
                                   fgColor.y * 255,
                                   fgColor.z * 255,
                                   fgColor.w * 255),
                          label);

        ImGui::ItemSize(bb, g.Style.FramePadding.y * 2);
        ImGui::ItemAdd(bb, id);

        return pressed;
    }

    void ProjectExplorer::groupSelectedNodes() {
        auto &session =
            *GAppContext::getInstance().getSubSystem<ProjectSession>();
        auto scene = session.getSubSystem<SceneDriver>();
        auto &sceneState = scene->getActiveScene()->getState();

        const auto &selComponents = sceneState.getSelectedComponents() |
                                    std::views::keys |
                                    std::ranges::to<std::vector>();

        if (selComponents.empty())
            return;

        auto groupComp = Canvas::GroupSceneComponent::create("New Group");
        auto tx = session.tx("Group components");
        auto status = tx.addComp(groupComp, {}, sceneState.getSceneId());
        for (const auto compId : selComponents) {
            if (status) {
                status = tx.parentComp(
                    compId, groupComp->getUuid(), sceneState.getSceneId());
            }
        }
        if (!status) {
            tx.cancel();
            BESS_WARN("Could not prepare group: {}", status.msg());
            return;
        }

        const auto result = tx.commit();
        if (!result) {
            BESS_WARN("Could not add group: {}", result.status.msg());
            return;
        }

        BESS_INFO(
            "[ProjectExplorer] Grouped {} selected components into new group.",
            selComponents.size());
    }

    void ProjectExplorer::groupOnNets() {

        auto &appCtx = Bess::GAppContext::getInstance();
        auto projectCtx = appCtx.getSubSystem<Bess::ProjectSession>();
        auto &simEngine = projectCtx->sim();
        if (!simEngine.isNetUpdated())
            return;

        const auto scene = GAppContext::getInstance()
                               .getSubSystem<Bess::ProjectSession>()
                               ->getSubSystem<SceneDriver>()
                               ->getActiveScene();

        std::unordered_map<UUID, std::vector<UUID>> netIdCompMap;

        auto &sceneState = scene->getState();
        std::unordered_map<UUID,
                           std::shared_ptr<Canvas::SimulationSceneComponent>>
            simIdToComp;

        for (const auto &[compId, comp] : sceneState.getAllComponents()) {
            if (comp->getType() == Canvas::SceneComponentType::simulation ||
                comp->getType() == Canvas::SceneComponentType::module) {
                const auto simComp =
                    comp->cast<Canvas::SimulationSceneComponent>();
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
                    BESS_WARN("[ProjectExplorer] Simulation component with "
                              "simId {} not found in scene for net grouping.",
                              (uint64_t)simId);
                }
            }
        }

        std::unordered_map<UUID, std::size_t> movedByParent;
        for (const auto &[_, components] : netIdCompMap) {
            for (const auto id : components) {
                const auto comp = sceneState.getComponentByUuid(id);
                if (comp && comp->getParentComponent() != UUID::null) {
                    ++movedByParent[comp->getParentComponent()];
                }
            }
        }

        std::vector<UUID> emptyGroups;
        for (const auto &[parent, count] : movedByParent) {
            const auto group =
                sceneState.getComponentByUuid<Canvas::GroupSceneComponent>(
                    parent);
            if (group && group->getChildComponents().size() == count) {
                emptyGroups.push_back(parent);
            }
        }

        auto &session = *projectCtx;
        auto tx = session.tx("Group components by net");
        int i = 1;
        for (const auto &[netId, components] : netIdCompMap) {
            std::shared_ptr<Canvas::GroupSceneComponent> group = nullptr;
            if (emptyGroups.empty()) {
                group = Canvas::GroupSceneComponent::create(
                    "Net " + std::to_string(i++));
                const auto status = tx.addComp(group, {}, scene->getSceneId());
                if (!status) {
                    tx.cancel();
                    BESS_WARN("Could not prepare net group: {}", status.msg());
                    return;
                }
            } else {
                group = sceneState
                            .getComponentByUuidSP<Canvas::GroupSceneComponent>(
                                emptyGroups.back());
                emptyGroups.pop_back();
                const auto status = tx.nameComp(group->getUuid(),
                                                "Net " + std::to_string(i++),
                                                scene->getSceneId());
                if (!status) {
                    tx.cancel();
                    BESS_WARN("Could not prepare net group rename: {}",
                              status.msg());
                    return;
                }
            }

            for (const auto &compId : components) {
                const auto comp = sceneState.getComponentByUuid(compId);
                if (!comp || comp->getParentComponent() == group->getUuid()) {
                    continue;
                }
                const auto status = tx.parentComp(
                    compId, group->getUuid(), scene->getSceneId());
                if (!status) {
                    tx.cancel();
                    BESS_WARN("Could not prepare net grouping: {}",
                              status.msg());
                    return;
                }
            }

        }

        if (!emptyGroups.empty()) {
            const auto status = tx.rmComp(emptyGroups, scene->getSceneId());
            if (!status) {
                tx.cancel();
                BESS_WARN("Could not prepare empty group removal: {}",
                          status.msg());
                return;
            }
        }
        if (!tx.isEmpty()) {
            const auto result = tx.commit();
            if (!result) {
                BESS_WARN("Could not group components by net: {}",
                          result.status.msg());
                return;
            }
        } else {
            tx.cancel();
        }
        BESS_INFO(
            "[ProjectExplorer] Grouped components on nets: created {} groups.",
            netIdCompMap.size());
    }

    size_t ProjectExplorer::drawEntites(const OrderedSet<UUID> &entities) {
        constexpr auto groupIcon = Icons::FontAwesomeIcons::FA_FOLDER;
        constexpr auto groupOpenIcon = Icons::FontAwesomeIcons::FA_FOLDER_OPEN;
        constexpr auto nodePopupName = "node_popup";
        constexpr auto treeFlags =
            ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_DrawLinesFull;

        auto sceneDriver = GAppContext::getInstance()
                               .getSubSystem<Bess::ProjectSession>()
                               ->getSubSystem<SceneDriver>();
        auto &sceneState = sceneDriver->getActiveScene()->getState();
        const auto selSize = sceneState.getSelectedComponents().size();

        size_t count = 0;

        std::vector<std::pair<UUID, UUID>> pendingMoves;

        for (const auto &compId : entities) {
            if (!shouldDisplayEntity(compId)) {
                continue;
            }

            const auto &comp = sceneState.getComponentByUuid(compId);
            if (!comp)
                continue;

            const auto type = comp->getType();
            if (!((int8_t)type &
                  (int8_t)
                      Canvas::SceneComponentTypeFlag::showInProjectExplorer))
                continue;

            bool clicked = false;
            if (comp->getType() == Canvas::SceneComponentType::group) {

                const auto key = m_nodesKeyCounter++;

                ImGui::PushID((int)key);

                const auto &win = ImGui::GetCurrentWindow();
                const auto &storage =
                    ImGui::GetCurrentWindow()->DC.StateStorage;
                const ImGuiID openId = win->GetID("open");
                bool opened = storage->GetInt(openId, 1) != 0;
                ImGui::PopID();

                const auto icon = opened ? groupOpenIcon : groupIcon;

                const auto oldName = comp->getName();
                const auto ret =
                    Widgets::EditableTreeNode(key,
                                              comp->getName(),
                                              comp->getIsSelected(),
                                              treeFlags,
                                              icon,
                                              ViewportTheme::colors.groupColor,
                                              nodePopupName,
                                              comp->getUuid());
                if (comp->getName() != oldName) {
                    auto name = comp->getName();
                    comp->setName(oldName);
                    const auto result = GAppContext::getInstance()
                                            .getSubSystem<ProjectSession>()
                                            ->nameComp(compId,
                                                       std::move(name),
                                                       sceneState.getSceneId());
                    if (!result) {
                        BESS_WARN("Could not rename group: {}",
                                  result.status.msg());
                    }
                }

                count++;
                opened = ret.first;
                clicked = ret.second;

                for (const auto id : takeDropIds()) {
                    pendingMoves.emplace_back(id, compId);
                }

                if (opened) {
                    count += drawEntites(comp->getChildComponents());
                    ImGui::TreePop();
                }

            } else {
                const bool isModule =
                    comp->getType() == Canvas::SceneComponentType::module;
                const bool isAtRoot = comp->getParentComponent() == UUID::null;
                std::string name;
                if (isAtRoot) {
                    name = std::format(
                        " {}   {}", comp->getIcon(), comp->getName());
                } else {
                    name = std::format(
                        "  {} {}", comp->getIcon(), comp->getName());
                }

                if (isModule) {
                    const auto &moduleColor = ViewportTheme::colors.moduleColor;
                    ImGui::PushStyleColor(ImGuiCol_Text, moduleColor.toHex());
                }

                const auto &pressed = drawLeafNode(m_nodesKeyCounter++,
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
                    for (auto &selId : sceneState.getSelectedComponents() |
                                           std::views::keys) {
                        const auto selectedComp =
                            sceneState.getComponentByUuid(selId);
                        if (selectedComp) {
                            payloadData.emplace_back(selId);
                        }
                    }
                    ImGui::SetDragDropPayload("TREE_NODE_PAYLOAD",
                                              payloadData.data(),
                                              payloadData.size() *
                                                  sizeof(uint64_t));
                    ImGui::Text("Dragging %lu nodes", payloadData.size());
                    ImGui::EndDragDropSource();
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
                    // selects the clicked component and deselects all other
                    // components
                    sceneState.clearSelectedComponents();
                    sceneState.addSelectedComponent(compId);
                    m_lastSelectedIndex = currentIndex;
                }
            }
        }

        parentComps(sceneState, pendingMoves);

        return count;
    }

} // namespace Bess::UI
