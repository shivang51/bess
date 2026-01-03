
#include "scene/scene_state/components/sim_scene_component.h"
#include "scene/scene_state/components/scene_component.h"
#include "scene/scene_state/scene_state.h" // included for pybind11
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
            state,
            materialRenderer,
            pathRenderer);
    }

    void drawSchematic(Bess::Canvas::SceneState &state,
                       std::shared_ptr<Bess::Renderer::MaterialRenderer> materialRenderer,
                       std::shared_ptr<Bess::Canvas::PathRenderer> pathRenderer) override {
        PYBIND11_OVERRIDE(
            void,
            Bess::Canvas::SimulationSceneComponent,
            drawSchematic,
            state,
            materialRenderer,
            pathRenderer);
    }
};

void bind_sim_scene_component(py::module_ &m) {
    py::class_<Bess::Canvas::SimulationSceneComponent,
               PySimSceneComponent,
               py::smart_holder>(m, "SimSceneComponent")
        .def(py::init<>())
        .def("draw", &Bess::Canvas::SimulationSceneComponent::draw)
        .def("draw_schematic", &Bess::Canvas::SimulationSceneComponent::drawSchematic);
}
