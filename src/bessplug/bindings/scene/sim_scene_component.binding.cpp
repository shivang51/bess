
#include "pages/main_page/scene_components/sim_scene_component.h"
#include "component_definition.h"
#include "pages/main_page/main_page.h"
#include "pages/main_page/scene_components/slot_scene_component.h"
#include "scene/scene_state/components/scene_component.h"
#include "scene/scene_state/scene_state.h" // included for pybind11
#include "settings/viewport_theme.h"
#include <pybind11/functional.h>
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>
#include <pybind11/stl_bind.h>

namespace py = pybind11;

class PySimSceneComponent : public Bess::Canvas::SimulationSceneComponent,
                            public py::trampoline_self_life_support {
  public:
    PySimSceneComponent() = default;

    void draw(Bess::Canvas::SceneState &state,
              std::shared_ptr<Bess::Renderer::MaterialRenderer> materialRenderer,
              std::shared_ptr<Bess::Canvas::PathRenderer> pathRenderer) override {
        PYBIND11_OVERRIDE(
            void,
            Bess::Canvas::SimulationSceneComponent,
            draw,
            std::ref(state),
            materialRenderer,
            pathRenderer);
    }

    void drawSchematic(Bess::Canvas::SceneState &state,
                       std::shared_ptr<Bess::Renderer::MaterialRenderer> materialRenderer,
                       std::shared_ptr<Bess::Canvas::PathRenderer> pathRenderer) override {
        PYBIND11_OVERRIDE_NAME(
            void,
            Bess::Canvas::SimulationSceneComponent,
            "draw_schematic",
            drawSchematic,
            std::ref(state),
            materialRenderer,
            pathRenderer);
    }

    void update(Bess::TimeMs timeStep, Bess::Canvas::SceneState &state) override {
        PYBIND11_OVERRIDE(
            void,
            Bess::Canvas::SimulationSceneComponent,
            update,
            timeStep,
            std::ref(state));
    }

    void onScaleChanged() override {
        PYBIND11_OVERRIDE_NAME(
            void,
            Bess::Canvas::SimulationSceneComponent,
            "on_scale_changed",
            onScaleChanged);
    }
};

void bind_sim_scene_component(py::module_ &m) {

    const auto setup = [](Bess::Canvas::SimulationSceneComponent &comp,
                          const std::shared_ptr<Bess::SimEngine::ComponentDefinition> &compDef) {
        comp.setCompDef(compDef);

        // STYLE
        auto &style = comp.getStyle();
        const auto &colors = Bess::ViewportTheme::colors;
        style.color = colors.componentBG;
        style.borderRadius = glm::vec4(6.f);
        style.headerColor = Bess::ViewportTheme::getCompHeaderColor(
            compDef->getGroupName());
        style.borderColor = colors.componentBorder;
        style.borderSize = glm::vec4(1.f);
        style.color = colors.componentBG;

        // SLOTS
        const auto &inpDetails = compDef->getInputSlotsInfo();
        const auto &outDetails = compDef->getOutputSlotsInfo();

        int inSlotIdx = 0, outSlotIdx = 0;
        char inpCh = 'A', outCh = 'a';

        const auto &slots = comp.createIOSlots(compDef->getInputSlotsInfo().count,
                                               compDef->getOutputSlotsInfo().count);

        std::vector<std::shared_ptr<Bess::Canvas::SlotSceneComponent>> createdSlots;
        for (const auto &slot : slots) {
            if (slot->getSlotType() == Bess::Canvas::SlotType::digitalInput) {
                if (inpDetails.names.size() > inSlotIdx)
                    slot->setName(inpDetails.names[inSlotIdx++]);
                else
                    slot->setName(std::string(1, inpCh++));
            } else {
                if (outDetails.names.size() > outSlotIdx)
                    slot->setName(outDetails.names[outSlotIdx++]);
                else
                    slot->setName(std::string(1, outCh++));
            }
            createdSlots.push_back(slot);
        }

        if (inpDetails.isResizeable) {
            auto slot = std::make_shared<Bess::Canvas::SlotSceneComponent>();
            slot->setSlotType(Bess::Canvas::SlotType::inputsResize);
            slot->setIndex(-1); // assign -1 for resize slots
            comp.addInputSlot(slot->getUuid(), false);
            createdSlots.push_back(slot);
        }

        if (outDetails.isResizeable) {
            auto slot = std::make_shared<Bess::Canvas::SlotSceneComponent>();
            slot->setSlotType(Bess::Canvas::SlotType::outputsResize);
            slot->setIndex(-1); // assign -1 for resize slots
            comp.addOutputSlot(slot->getUuid(), false);
            createdSlots.push_back(slot);
        }

        auto &scene = Bess::Pages::MainPage::getInstance()->getState().getSceneDriver();
        auto &sceneState = scene->getState();
        for (const auto &slot : createdSlots) {
            sceneState.addComponent(slot);
        }
    };

    py::class_<Bess::Canvas::SimulationSceneComponent,
               PySimSceneComponent,
               Bess::Canvas::SceneComponent,
               py::smart_holder>(m, "SimulationSceneComponent")
        .def(py::init<>())
        .def("draw", &Bess::Canvas::SimulationSceneComponent::draw,
             py::arg("scene_state"),
             py::arg("material_renderer"),
             py::arg("path_renderer"))
        .def("draw_schematic", &Bess::Canvas::SimulationSceneComponent::drawSchematic,
             py::arg("scene_state"),
             py::arg("material_renderer"),
             py::arg("path_renderer"))
        .def("update", &Bess::Canvas::SimulationSceneComponent::update,
             py::arg("time_step"),
             py::arg("scene_state"))
        .def("setup", setup, py::arg("comp_def"))
        .def("get_input_states", &Bess::Canvas::SimulationSceneComponent::getInputStates, py::arg("scene_state"))
        .def("get_output_states", &Bess::Canvas::SimulationSceneComponent::getOutputStates, py::arg("scene_state"))
        .def("draw_slots", &Bess::Canvas::SimulationSceneComponent::drawSlots, py::arg("scene_state"), py::arg("material_renderer"), py::arg("path_renderer"))
        .def("draw_background", &Bess::Canvas::SimulationSceneComponent::drawBackground, py::arg("scene_state"), py::arg("material_renderer"), py::arg("path_renderer"))
        .def("on_scale_changed", &Bess::Canvas::SimulationSceneComponent::onScaleChanged)
        .def_property("name",                                                                            //\n
                      [](const Bess::Canvas::SimulationSceneComponent &self) { return self.getName(); }, // \n
                      &Bess::Canvas::SimulationSceneComponent::setName)
        .def_property("position",                                                                                      //\n
                      [](const Bess::Canvas::SimulationSceneComponent &self) { return self.getTransform().position; }, // \n
                      &Bess::Canvas::SimulationSceneComponent::setPosition)
        .def_property("scale",                                                                                      //\n
                      [](const Bess::Canvas::SimulationSceneComponent &self) { return self.getTransform().scale; }, // \n
                      &Bess::Canvas::SimulationSceneComponent::setScale)
        .def_property_readonly("runtime_id", //\n
                               [](const Bess::Canvas::SimulationSceneComponent &self) { return self.getRuntimeId(); });
}
