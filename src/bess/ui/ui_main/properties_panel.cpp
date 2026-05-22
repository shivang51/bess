#include "ui/ui_main/properties_panel.h"
#include "application/pages/main_page/main_page.h"
#include "common/helpers.h"
#include "gtc/type_ptr.hpp"
#include "pages/main_page/scene_components/connection_scene_component.h"
#include "pages/main_page/scene_components/scene_comp_types.h"
#include "pages/main_page/scene_components/sim_scene_component.h"
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
        auto &sceneState = Pages::MainPage::getInstance()
                               ->getState()
                               .getSceneDriver()
                               ->getState();
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
            auto &simEngine = SimEngine::SimulationEngine::instance();
            auto &def =
                simEngine.getComponentDefinition(simComp->getSimEngineId());

            // if (def->hasTrait<SimEngine::ClockTrait>()) {
            //     bool changed =
            //     drawClockTrait(def->getTrait<SimEngine::ClockTrait>(),
            //     compId); if (changed) {
            //         auto &def =
            //         simEngine.getComponentDefinition(simComp->getSimEngineId());
            //         const auto &trait =
            //         def->getTrait<SimEngine::ClockTrait>();
            //
            //         std::string frequencyUnitStr;
            //         switch (trait->frequencyUnit) {
            //         case SimEngine::FrequencyUnit::hz:
            //             frequencyUnitStr = "Hz";
            //             break;
            //         case SimEngine::FrequencyUnit::kHz:
            //             frequencyUnitStr = "kHz";
            //             break;
            //         case SimEngine::FrequencyUnit::MHz:
            //             frequencyUnitStr = "MHz";
            //             break;
            //         }
            //         def->getOutputSlotsInfo().names[0] =
            //         std::format("{:.2f}", trait->frequency) +
            //         frequencyUnitStr; const auto &slotComp =
            //         sceneState.getComponentByUuid<Canvas::SlotSceneComponent>(simComp->getOutputSlots()[0]);
            //         slotComp->setName(def->getOutputSlotsInfo().names[0]);
            //     }
            // }
        } else if (compType == Canvas::SceneComponentType::connection) {
            auto connComp = comp->cast<Canvas::ConnectionSceneComponent>();
            drawConnectionComponent(connComp);
        }
    }
} // namespace Bess::UI
