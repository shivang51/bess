#include "debug_panel.h"
#include "bess_core/g_app_context.h"
#include "bess_core/project_context.h"
#include "bess_core/scene_driver.h"
#include "common/bess_uuid.h"
#include "dig_sim_driver.h"
#include "icons/CodIcons_Remapped.h"
#include "imgui.h"
#include "imgui_internal.h"
#include "pages/main_page/main_page.h"
#include "pages/main_page/scene_components/scene_comp_types.h"
#include "pages/main_page/scene_components/sim_scene_component.h"
#include "scene_ser_reg.h"
#include "services/plugin_service/plugin_service.h"
#include "simulation_engine.h"
#include "ui_main/ui_main.h"
#include "widgets/m_widgets.h"

namespace Bess::UI {
    DebugPanel::DebugPanel() : Panel("Debug Panel") {
        m_name = Icons::CodIcons::COPILOT + std::string(" Debug Panel");
#ifdef DEBUG
        m_visible = true;
#endif
    }

    void DebugPanel::onDraw() {
        auto &mainPageState = Pages::MainPage::getInstance()->getState();

        auto sceneDriver = GAppContext::getInstance()
                               .getSubSystem<Bess::ProjectContext>()
                               ->getSubSystem<SceneDriver>();
        const auto &sceneState = sceneDriver->getActiveScene()->getState();

        if (!sceneDriver->getIsPaused()) {
            const auto &hoverVp = UI::UIMain::getHoveredSceneViewportPanel();

            const auto &pickingId =
                hoverVp ? hoverVp->getPickingId() : PickingId::invalid();
            ImGui::Text("Hovered  Runtime Id: %u | Info: %u",
                        pickingId.runtimeId,
                        pickingId.info);
        }

        if (ImGui::BeginTabBar("MyTabBar")) {
            if (ImGui::BeginTabItem("Scene Controls")) {
                if (sceneDriver->getSceneCount() > 1) {

                    ImGui::AlignTextToFramePadding();
                    ImGui::Text("Active Scene: %lu",
                                sceneDriver->getActiveSceneIdx());

                    ImGui::SameLine();
                    ImGui::AlignTextToFramePadding();
                    ImGui::SeparatorEx(ImGuiSeparatorFlags_Vertical);
                    ImGui::SameLine();

                    ImGui::AlignTextToFramePadding();
                    ImGui::Text("Scene Count: %lu",
                                sceneDriver->getSceneCount());

                    ImGui::SameLine();
                    if (ImGui::Button("Prev-Scene")) {
                        size_t activeScene = sceneDriver->getActiveSceneIdx();
                        if (activeScene > 0) {
                            sceneDriver->setActiveScene(activeScene - 1);
                        }
                    }

                    ImGui::SameLine();

                    ImGui::AlignTextToFramePadding();
                    if (ImGui::Button("Next-Scene")) {
                        size_t activeScene = sceneDriver->getActiveSceneIdx();
                        if (activeScene < sceneDriver->getSceneCount() - 1) {
                            sceneDriver->setActiveScene(activeScene + 1);
                        }
                    }
                } else {
                    ImGui::Text("Only one scene, no controls to show");
                }
                ImGui::EndTabItem();
            }
            if (ImGui::BeginTabItem("Selected Comps Info")) {
                if (sceneState.getSelectedComponents().size() >= 1) {
                    const auto &selectedCompId =
                        sceneState.getSelectedComponents().begin()->first;
                    const auto &selectedComp =
                        sceneState.getComponentByUuid(selectedCompId);
                    ImGui::Text(
                        "Selected Id: %lu | Runtime Id of component: %u",
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
                ImGui::EndTabItem();
            }
            if (ImGui::BeginTabItem("Comp Ser Info")) {
                const auto &selComps = sceneState.getSelectedComponents();
                const auto &appCtx = GAppContext::getInstance();

                if (!selComps.empty()) {
                    const auto &compId = selComps.begin()->first;
                    const auto &comp = sceneState.getComponentByUuid(compId);

                    auto pluginService =
                        appCtx.hasSubSystem<Svc::PluginService>()
                            ? appCtx.getSubSystem<Svc::PluginService>()
                            : nullptr;

                    if (Canvas::SceneSerReg::hasComponent(
                            comp->getTypeName())) {
                        ImGui::Text(
                            "Component type %s supports deserialization",
                            comp->getTypeName().c_str());
                    } else if (pluginService && pluginService->hasSceneComp(
                                                    comp->getTypeName())) {
                        ImGui::Text("Component type %s is provided by a plugin",
                                    comp->getTypeName().c_str());
                        if (pluginService->canDerserialize(
                                comp->getTypeName())) {
                            ImGui::Text(
                                "Plugin can deserialize component type %s",
                                comp->getTypeName().c_str());
                        } else {
                            ImGui::Text(
                                "Plugin CANNOT deserialize component type %s",
                                comp->getTypeName().c_str());
                        }
                    } else {
                        ImGui::Text("Component type %s NOT found anywhere, "
                                    "cannot be deserialized",
                                    comp->getTypeName().c_str());
                    }

                    if (Widgets::TreeNode(2,
                                          "First Sel Component Serilaized")) {
                        if (!selComps.empty()) {

                            const auto &j = comp->toJson();
                            Widgets::SelectableText(compId.toString(),
                                                    j.toStyledString());

                            if (comp->getType() ==
                                    Canvas::SceneComponentType::module ||
                                comp->getType() ==
                                    Canvas::SceneComponentType::simulation) {
                                const auto &simComp = comp->cast<
                                    Canvas::SimulationSceneComponent>();
                                BESS_ASSERT(simComp->getCompDef(),
                                            "[DEBUGPANEL] def not set");
                                const auto &digComp =
                                    Bess::GAppContext::getInstance()
                                        .getSubSystem<Bess::ProjectContext>()
                                        ->getSimEngine()
                                        .getComponent<SimEngine::Drivers::
                                                          Digital::DigSimComp>(
                                            simComp->getSimEngineId());
                                Json::Value digCompJson = digComp->toJson();
                                ImGui::Separator();
                                Widgets::SelectableText(
                                    std::to_string(simComp->getSimEngineId()),
                                    digCompJson.toStyledString());
                            }
                        }
                        ImGui::TreePop();
                    }
                }

                ImGui::EndTabItem();
            }

            if (ImGui::BeginTabItem("Project File Ser")) {
                const auto &projectFile = mainPageState.getCurrentProjectFile();
                Widgets::SelectableText("Sim-Engine",
                                        projectFile->toJson().toStyledString());
                ImGui::EndTabItem();
            }

            ImGui::EndTabBar();
        }
    }

    void DebugPanel::drawDependencyGraph(const UUID &compId) {
        const auto &mainPageState = Pages::MainPage::getInstance()->getState();
        auto sceneDriver = GAppContext::getInstance()
                               .getSubSystem<Bess::ProjectContext>()
                               ->getSubSystem<SceneDriver>();
        const auto &sceneState = sceneDriver->getActiveScene()->getState();

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
