
#include "pages/main_page/scene_components/sim_scene_component.h"
#include "common/bess_uuid.h"
#include "component_definition.h"
#include "pages/main_page/main_page.h"
#include "pages/main_page/scene_components/slot_scene_component.h"
#include "scene/scene_state/components/scene_component.h"
#include "scene/scene_state/scene_state.h" // included for pybind11
#include "scene_draw_context.h"
#include "settings/viewport_theme.h"
#include "json/value.h"
#include <memory>
#include <pybind11/functional.h>
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>
#include <pybind11/stl_bind.h>

namespace py = pybind11;

class PySimSceneComponent : public Bess::Canvas::SimulationSceneComponent,
                            public py::trampoline_self_life_support {
  public:
    PySimSceneComponent() = default;
    ~PySimSceneComponent() override = default;

    void draw(Bess::SceneDrawContext &context) override {
        PYBIND11_OVERRIDE(
            void,
            Bess::Canvas::SimulationSceneComponent,
            draw,
            std::ref(context));
    }

    std::vector<Bess::UUID> cleanup(Bess::Canvas::SceneState &state,
                                    Bess::UUID caller) override {
        PYBIND11_OVERRIDE(
            std::vector<Bess::UUID>,
            Bess::Canvas::SimulationSceneComponent,
            cleanup,
            std::ref(state),
            caller);
    }

    std::vector<std::shared_ptr<SceneComponent>> clone(const Bess::Canvas::SceneState &sceneState) const override {
        PYBIND11_OVERRIDE(
            std::vector<std::shared_ptr<SceneComponent>>,
            Bess::Canvas::SimulationSceneComponent,
            clone,
            std::cref(sceneState));
    }

    void drawSchematic(Bess::SceneDrawContext &context) override {
        PYBIND11_OVERRIDE_NAME(
            void,
            Bess::Canvas::SimulationSceneComponent,
            "draw_schematic",
            drawSchematic,
            std::ref(context));
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

    std::string getTypeName() override {
        PYBIND11_OVERRIDE_NAME(
            std::string,
            Bess::Canvas::SimulationSceneComponent,
            "get_type_name",
            getTypeName);
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
        .def(py::pickle(
            [](const Bess::Canvas::SimulationSceneComponent &self) {
                Json::StreamWriterBuilder builder;
                builder["indentation"] = "";
                const std::string output = Json::writeString(builder, self.toJson());
                return py::make_tuple(output);
            },
            [](py::tuple &t) {
                const auto &self = t[0].cast<std::string>();

                Json::CharReaderBuilder builder;
                Json::CharReader *reader = builder.newCharReader();

                Json::Value json;
                std::string errors;

                bool parsingSuccessful = reader->parse(
                    self.c_str(),
                    self.c_str() + self.size(),
                    &json,
                    &errors);
                delete reader;

                if (!parsingSuccessful) {
                    throw std::runtime_error("Failed to parse SimulationSceneComponent from JSON: " + errors);
                }

                auto newComp = PySimSceneComponent();
                auto sharedComp = std::shared_ptr<Bess::Canvas::SimulationSceneComponent>(&newComp,
                                                                                          [](Bess::Canvas::SimulationSceneComponent *) {});
                Bess::Canvas::SimulationSceneComponent::fromJson(json, sharedComp);
                return newComp;
            }))
        .def("cleanup", &Bess::Canvas::SimulationSceneComponent::cleanup, py::arg("state"), py::arg("caller") = Bess::UUID::null)
        .def("draw", &Bess::Canvas::SimulationSceneComponent::draw, py::arg("context"))
        .def("draw_schematic", &Bess::Canvas::SimulationSceneComponent::drawSchematic, py::arg("context"))
        .def("update", &Bess::Canvas::SimulationSceneComponent::update, py::arg("time_step"), py::arg("scene_state"))
        .def("clone", &Bess::Canvas::SimulationSceneComponent::clone, py::arg("scene_state"))
        .def("clone_sim_comp", &Bess::Canvas::SimulationSceneComponent::cloneSimulationComponent, py::arg("scene_state"), py::arg("cloned_component"))
        .def("setup", setup, py::arg("comp_def"))
        .def("get_input_states", &Bess::Canvas::SimulationSceneComponent::getInputStates, py::arg("scene_state"))
        .def("get_output_states", &Bess::Canvas::SimulationSceneComponent::getOutputStates, py::arg("scene_state"))
        .def("draw_slots", &Bess::Canvas::SimulationSceneComponent::drawSlots, py::arg("context"))
        .def("draw_background", &Bess::Canvas::SimulationSceneComponent::drawBackground, py::arg("context"))
        .def("on_scale_changed", &Bess::Canvas::SimulationSceneComponent::onScaleChanged)
        .def("get_type_name", &Bess::Canvas::SimulationSceneComponent::getTypeName)
        .def("set_scale_dirty", &Bess::Canvas::SimulationSceneComponent::setScaleDirty, py::arg("val") = true)
        .def("set_schematic_scale_dirty", &Bess::Canvas::SimulationSceneComponent::setSchematicScaleDirty, py::arg("val") = true)
        .def_property_readonly("comp_def", [](const Bess::Canvas::SimulationSceneComponent &self) { return self.getCompDef(); })
        .def_property("transform",                                                                            //\n
                      [](const Bess::Canvas::SimulationSceneComponent &self) { return self.getTransform(); }, // \n
                      &Bess::Canvas::SimulationSceneComponent::setTransform)
        .def_property("schematic_transform",                                                                           //\n
                      [](const Bess::Canvas::SimulationSceneComponent &self) { return self.getSchematicTransform(); }, // \n
                      &Bess::Canvas::SimulationSceneComponent::setSchematicTransform)
        .def_property("name",                                                                            //\n
                      [](const Bess::Canvas::SimulationSceneComponent &self) { return self.getName(); }, // \n
                      &Bess::Canvas::SimulationSceneComponent::setName)
        .def_property("position",                                                                                      //\n
                      [](const Bess::Canvas::SimulationSceneComponent &self) { return self.getTransform().position; }, // \n
                      &Bess::Canvas::SimulationSceneComponent::setPosition)
        .def_property("scale",                                                                                      //\n
                      [](const Bess::Canvas::SimulationSceneComponent &self) { return self.getTransform().scale; }, // \n
                      &Bess::Canvas::SimulationSceneComponent::setScale)
        .def_property("schematic_scale",                                                                                     //\n
                      [](const Bess::Canvas::SimulationSceneComponent &self) { return self.getSchematicTransform().scale; }, // \n
                      [](Bess::Canvas::SimulationSceneComponent &self, const glm::vec2 &scale) {
        self.getSchematicTransform().scale = scale;
        self.setSchSlotsPosDirty(); })
        .def_property_readonly("runtime_id", //\n
                               [](const Bess::Canvas::SimulationSceneComponent &self) { return self.getRuntimeId(); });
}
