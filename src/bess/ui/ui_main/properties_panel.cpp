#include "ui/ui_main/properties_panel.h"
#include "application/pages/main_page/main_page.h"
#include "common/helpers.h"
#include "gtc/type_ptr.hpp"
#include "pages/main_page/scene_components/connection_scene_component.h"
#include "pages/main_page/scene_components/scene_comp_types.h"
#include "pages/main_page/scene_components/sim_scene_component.h"
#include "bess_core/g_app_context.h"
#include "bess_core/project_context.h"
#include "simulation_engine.h"
#include "ui/icons/CodIcons.h"
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
            ImGui::ColorEdit4("Color", glm::value_ptr(style.color));
        }
    }

    void PropertiesPanel::onDraw() {
        auto sceneDriver = GAppContext::getInstance().getSubSystem<Bess::ProjectContext>()->getSubSystem<SceneDriver>();
        auto &sceneState = sceneDriver->getActiveScene()->getState();
        if (sceneState.getSelectedComponents().empty()) {
            ImGui::TextUnformatted("No component selected.");
            return;
        }

        // for now only showing first selected component's properties
        const UUID &compId = sceneState.getSelectedComponents().begin()->first;
        auto comp = sceneState.getComponentByUuid(compId);
        const auto compType = comp->getType();

        if (Widgets::TextBox("Name", comp->getName())) {
            comp->setName(comp->getName());
        }

        comp->drawPropertiesUI(sceneState);

        if (compType == Canvas::SceneComponentType::simulation) {
            auto simComp = comp->cast<Canvas::SimulationSceneComponent>();
            auto &appCtx = Bess::GAppContext::getInstance();
            auto projectCtx = appCtx.getSubSystem<Bess::ProjectContext>();
            auto &simEngine = projectCtx->getSimEngine();
            auto &def =
                simEngine.getComponentDefinition(simComp->getSimEngineId());
        } else if (compType == Canvas::SceneComponentType::connection) {
            auto connComp = comp->cast<Canvas::ConnectionSceneComponent>();
            drawConnectionComponent(connComp);
        }
    }
} // namespace Bess::UI
