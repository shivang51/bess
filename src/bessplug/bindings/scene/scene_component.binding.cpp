
#include "scene/scene_state/components/scene_component.h"
#include "pages/main_page/scene_components/sim_scene_component.h"
#include "scene/scene_state/scene_state.h" // included for pybind11
#include <pybind11/functional.h>
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>
#include <pybind11/stl_bind.h>

namespace py = pybind11;

class PySceneComponent : public Bess::Canvas::SceneComponent,
                         public py::trampoline_self_life_support {
  public:
    PySceneComponent() = default;

    void draw(Bess::Canvas::SceneState &state,
              std::shared_ptr<Bess::Renderer::MaterialRenderer> materialRenderer,
              std::shared_ptr<Bess::Canvas::PathRenderer> pathRenderer) override {
        PYBIND11_OVERRIDE(
            void,
            Bess::Canvas::SceneComponent,
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
            Bess::Canvas::SceneComponent,
            "draw_schematic",
            drawSchematic,
            std::ref(state),
            materialRenderer,
            pathRenderer);
    }

    void update(Bess::TimeMs timeStep, Bess::Canvas::SceneState &state) override {
        PYBIND11_OVERRIDE(
            void,
            Bess::Canvas::SceneComponent,
            update,
            timeStep,
            std::ref(state));
    }
};

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
};

void bind_scene_component(py::module_ &m) {
    py::class_<Bess::Canvas::SceneComponent,
               PySceneComponent,
               py::smart_holder>(m, "SceneComponent")
        .def(py::init<>())
        .def("draw", &Bess::Canvas::SceneComponent::draw,
             py::arg("scene_state"),
             py::arg("material_renderer"),
             py::arg("path_renderer"))
        .def("draw_schematic", &Bess::Canvas::SceneComponent::drawSchematic,
             py::arg("scene_state"),
             py::arg("material_renderer"),
             py::arg("path_renderer"))
        .def("update", &Bess::Canvas::SceneComponent::update,
             py::arg("time_step"),
             py::arg("scene_state"));

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
