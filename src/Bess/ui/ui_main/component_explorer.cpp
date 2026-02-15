#include "ui/ui_main/component_explorer.h"
#include "application/pages/main_page/cmds/add_comp_cmd.h"
#include "application/pages/main_page/main_page.h"
#include "application/pages/main_page/scene_components/non_sim_scene_component.h"
#include "common/helpers.h"
#include "component_catalog.h"
#include "imgui.h"
#include "imgui_internal.h"
#include "pages/main_page/scene_components/sim_scene_component.h"
#include "ui/widgets/m_widgets.h"
#include <utility>

namespace Bess::UI {

    bool ComponentExplorer::isShown = false;
    bool ComponentExplorer::m_isfirstTimeDraw = true;
    std::string ComponentExplorer::m_searchQuery;

    void ComponentExplorer::createComponent(const std::shared_ptr<SimEngine::ComponentDefinition> &def,
                                            const glm::vec2 &pos) {
        auto &cmdSystem = Pages::MainPage::getTypedInstance()->getState().getCommandSystem();
        auto &scene = Pages::MainPage::getTypedInstance()->getState().getSceneDriver();

        SimEngine::SimulationEngine &simEngine = SimEngine::SimulationEngine::instance();
        auto simEngineId = simEngine.addComponent(def);

        auto components = Canvas::SimulationSceneComponent::createNewAndRegister(def);

        auto sceneComp = components.front()->template cast<Canvas::SimulationSceneComponent>();
        components.erase(components.begin());
        sceneComp->setCompDef(def->clone());
        sceneComp->getTransform().position.x = pos.x;
        sceneComp->getTransform().position.y = pos.y;

        if (scene->hasPluginDrawHookForComponentHash(def->getHash())) {
            auto hook = scene->getPluginDrawHookForComponentHash(def->getHash());
            sceneComp->cast<Canvas::SimulationSceneComponent>()->setDrawHook(hook);
        }

        cmdSystem.execute(std::make_unique<Cmd::AddCompCmd<Canvas::SimulationSceneComponent>>(sceneComp, components));
        isShown = false;
    }

    void ComponentExplorer::createComponent(std::type_index tIdx, const glm::vec2 &pos) {
        auto &cmdSystem = Pages::MainPage::getTypedInstance()->getState().getCommandSystem();
        auto inst = Canvas::NonSimSceneComponent::getInstance(tIdx);
        inst->getTransform().position.x = pos.x;
        inst->getTransform().position.y = pos.y;
        cmdSystem.execute(std::make_unique<Cmd::AddCompCmd<Canvas::NonSimSceneComponent>>(inst));
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
        int key = 0;
        // simulation components
        {
            for (auto &ent : *componentTree) {
                bool shouldCollectionShow = m_searchQuery.empty();

                if (!shouldCollectionShow) {
                    for (const auto &comp : ent.second) {
                        if (Common::Helpers::toLowerCase(comp->getName()).find(m_searchQuery) != std::string::npos) {
                            shouldCollectionShow = true;
                            break;
                        }
                    }
                }

                if (!shouldCollectionShow)
                    continue;

                if (Widgets::TreeNode(key, ent.first, ImGuiTreeNodeFlags_DefaultOpen)) {
                    for (const auto &comp : ent.second) {
                        if (m_searchQuery != "" &&
                            Common::Helpers::toLowerCase(comp->getName())
                                    .find(m_searchQuery) == std::string::npos)
                            continue;

                        const std::string &name = comp->getName();

                        if (Widgets::ButtonWithPopup(name, name + "OptionsMenu", false)) {
                            createComponent(comp);
                        }

                        if (ImGui::BeginPopup((name + "OptionsMenu").c_str())) {
                            ImGui::EndPopup();
                        }
                    }
                    ImGui::TreePop();
                }
            }
        }

        // non simulation components
        if (Widgets::TreeNode(++key, "Miscellaneous", ImGuiTreeNodeFlags_DefaultOpen)) {
            static auto nonSimComponents = Canvas::NonSimSceneComponent::registry;
            for (auto &comp : nonSimComponents) {
                if (m_searchQuery != "" && Common::Helpers::toLowerCase(comp.second).find(m_searchQuery) == std::string::npos)
                    continue;

                if (Widgets::ButtonWithPopup(comp.second, "", false)) {
                    createComponent(comp.first);
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
