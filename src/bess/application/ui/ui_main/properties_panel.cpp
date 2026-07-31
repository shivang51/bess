#include "ui/ui_main/properties_panel.h"
#include "bess_core/g_app_context.h"
#include "common/helpers.h"
#include "dig_sim_driver.h"
#include "gtc/type_ptr.hpp"
#include "pages/main_page/comp_edit.h"
#include "pages/main_page/main_page.h"
#include "pages/main_page/scene_components/connection_scene_component.h"
#include "pages/main_page/scene_components/scene_comp_types.h"
#include "pages/main_page/scene_components/sim_scene_component.h"
#include "project_session/project_session.h"
#include "simulation_engine.h"
#include "ui/icons/CodIcons_Remapped.h"
#include "ui/widgets/m_widgets.h"
#include <imgui.h>

namespace Bess::UI {
    static constexpr auto windowName = Common::Helpers::concat(
        Icons::CodIcons::SYMBOL_PROPERTY, "  Properties");

    PropertiesPanel::PropertiesPanel() : Panel(std::string(windowName.data())) {
        m_defaultDock = Dock::right;
        m_visible = true;
    }

    void drawConnectionComponent(
        const std::shared_ptr<Canvas::ConnectionSceneComponent> &comp) {
        Widgets::CheckboxWithLabel("Use Custom Color",
                                   &comp->getUseCustomColor());
        if (comp->getUseCustomColor()) {
            auto &style = comp->getStyle();
            ImGui::ColorEdit4("Color", style.color.data());
        }
    }

    void PropertiesPanel::onDraw() {
        auto &session =
            *GAppContext::getInstance().getSubSystem<ProjectSession>();
        auto sceneDriver = session.getSubSystem<SceneDriver>();
        auto &sceneState = sceneDriver->getActiveScene()->getState();
        if (sceneState.getSelectedComponents().empty()) {
            ImGui::TextUnformatted("No component selected.");
            return;
        }

        // for now only showing first selected component's properties
        const UUID &compId = sceneState.getSelectedComponents().begin()->first;
        auto comp = sceneState.getComponentByUuid(compId);
        const auto compType = comp->getType();

        const auto oldName = comp->getName();
        if (Widgets::TextBox("Name", comp->getName())) {
            auto name = comp->getName();
            comp->setName(oldName);
            const auto result = session.nameComp(
                compId, std::move(name), sceneState.getSceneId());
            if (!result) {
                BESS_WARN("Could not rename component: {}",
                          result.status.msg());
            }
        }

        auto before = comp->toJson();
        comp->drawPropertiesUI(sceneState);

        if (compType == Canvas::SceneComponentType::simulation) {
            auto simComp = comp->cast<Canvas::SimulationSceneComponent>();
            auto &appCtx = Bess::GAppContext::getInstance();
            auto projectCtx = appCtx.getSubSystem<Bess::ProjectSession>();
            auto &simEngine = projectCtx->sim();
            auto &def =
                simEngine.getComponentDefinition(simComp->getSimEngineId());
        } else if (compType == Canvas::SceneComponentType::connection) {
            auto connComp = comp->cast<Canvas::ConnectionSceneComponent>();
            drawConnectionComponent(connComp);
        }
        (void)Edit::trackComp(*comp, std::move(before), "properties");
    }
} // namespace Bess::UI
