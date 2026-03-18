#include "debug_panel.h"
#include "common/bess_uuid.h"
#include "icons/FontAwesomeIcons.h"
#include "imgui.h"
#include "imgui_internal.h"
#include "pages/main_page/main_page.h"
#include "pages/main_page/scene_components/scene_comp_types.h"
#include "pages/main_page/scene_components/sim_scene_component.h"
#include "widgets/m_widgets.h"

namespace Bess::UI {
    DebugPanel::DebugPanel() : Panel("Debug Panel") {
        m_name = Icons::FontAwesomeIcons::FA_USER_NURSE + std::string(" Debug Panel");
#ifdef DEBUG
        m_visible = true;
#endif
    }

    void DebugPanel::onDraw() {
        auto &mainPageState = Pages::MainPage::getInstance()->getState();

        auto &sceneDriver = mainPageState.getSceneDriver();
        const auto &sceneState = sceneDriver->getState();

        if (ImGui::BeginTabBar("MyTabBar")) {
            if (ImGui::BeginTabItem("Tab 1")) {
                ImGui::Text("This is the content of Tab 1");
                ImGui::EndTabItem();
            }
            if (ImGui::BeginTabItem("Tab 2")) {
                ImGui::Text("This is the content of Tab 2");
                ImGui::EndTabItem();
            }
            if (ImGui::BeginTabItem("Tab 3")) {
                ImGui::Text("This is the content of Tab 3");
                ImGui::EndTabItem();
            }
            ImGui::EndTabBar();
        }

        if (sceneDriver.getSceneCount() > 1) {

            ImGui::AlignTextToFramePadding();
            ImGui::Text("Active Scene: %lu", sceneDriver.getActiveSceneIdx());

            ImGui::SameLine();
            ImGui::AlignTextToFramePadding();
            ImGui::SeparatorEx(ImGuiSeparatorFlags_Vertical);
            ImGui::SameLine();

            ImGui::AlignTextToFramePadding();
            ImGui::Text("Scene Count: %lu", sceneDriver.getSceneCount());

            ImGui::SameLine();
            if (ImGui::Button("Prev-Scene")) {
                size_t activeScene = sceneDriver.getActiveSceneIdx();
                if (activeScene > 0) {
                    sceneDriver.setActiveScene(activeScene - 1);
                }
            }

            ImGui::SameLine();

            ImGui::AlignTextToFramePadding();
            if (ImGui::Button("Next-Scene")) {
                size_t activeScene = sceneDriver.getActiveSceneIdx();
                if (activeScene < sceneDriver.getSceneCount() - 1) {
                    sceneDriver.setActiveScene(activeScene + 1);
                }
            }
        }

        const auto &hoverId = sceneDriver->getHoveredEntity();
        ImGui::Text("Hovered  Runtime Id: %u | Info: %u", hoverId.runtimeId, hoverId.info);

        if (sceneState.getSelectedComponents().size() >= 1) {
            const auto &selectedCompId = sceneState.getSelectedComponents().begin()->first;
            const auto &selectedComp = sceneState.getComponentByUuid(selectedCompId);
            ImGui::Text("Selected Id: %lu | Runtime Id of component: %u",
                        (uint64_t)selectedCompId,
                        selectedComp ? selectedComp->getRuntimeId() : 0);

            drawDependencyGraph(selectedCompId);
        }

        if (Widgets::TreeNode(0, "Selected components")) {
            const auto &selComps = sceneState.getSelectedComponents();
            ImGui::Indent();
            for (const auto &[compId, selected] : selComps) {
                ImGui::BulletText("%lu", (uint64_t)compId);
            }
            ImGui::Unindent();
            ImGui::TreePop();
        }

        if (Widgets::TreeNode(2, "First Component Serilaized")) {
            const auto &selComps = sceneState.getSelectedComponents();
            if (!selComps.empty()) {
                const auto &compId = selComps.begin()->first;
                const auto &comp = sceneState.getComponentByUuid(compId);

                const auto &j = comp->toJson();
                ImGui::TextWrapped("%s", j.toStyledString().c_str());

                if (comp->getType() == Canvas::SceneComponentType::module ||
                    comp->getType() == Canvas::SceneComponentType::simulation) {
                    const auto &simComp = comp->cast<Canvas::SimulationSceneComponent>();
                    const auto &def = simComp->getCompDef();
                    Json::Value defJson;
                    JsonConvert::toJsonValue(def, defJson);
                    ImGui::Separator();
                    ImGui::TextWrapped("%s", defJson.toStyledString().c_str());
                }
            }
            ImGui::TreePop();
        }
    }

    void DebugPanel::drawDependencyGraph(const UUID &compId) {
        const auto &mainPageState = Pages::MainPage::getInstance()->getState();
        const auto &sceneDriver = mainPageState.getSceneDriver();
        const auto &sceneState = sceneDriver->getState();

        const auto &comp = sceneState.getComponentByUuid(compId);
        if (!comp) {
            return;
        }

        const auto &dependants = comp->getDependants(sceneState);

        if (Widgets::TreeNode(1,
                              std::format("Dependants of component {} ({}):",
                                          comp->getName(),
                                          (uint64_t)compId))) {
            ImGui::Indent();
            for (const auto &depId : dependants) {
                const auto &depComp = sceneState.getComponentByUuid(depId);
                ImGui::BulletText("%s (%lu) - %s",
                                  depComp->getName().c_str(),
                                  (uint64_t)depId,
                                  typeid(depComp->getType()).name());
            }
            ImGui::Unindent();
            ImGui::TreePop();
        }
    }
} // namespace Bess::UI
