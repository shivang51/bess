
#include "scene/scene_state/components/scene_component.h"
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
            state,
            materialRenderer,
            pathRenderer);
    }

    void drawSchematic(Bess::Canvas::SceneState &state,
                       std::shared_ptr<Bess::Renderer::MaterialRenderer> materialRenderer,
                       std::shared_ptr<Bess::Canvas::PathRenderer> pathRenderer) override {
        PYBIND11_OVERRIDE(
            void,
            Bess::Canvas::SceneComponent,
            drawSchematic,
            state,
            materialRenderer,
            pathRenderer);
    }
};

void bind_scene_component(py::module_ &m) {
    py::class_<Bess::Canvas::SceneComponent,
               PySceneComponent,
               py::smart_holder>(m, "SceneComponent")
        .def(py::init<>())
        .def("draw", &Bess::Canvas::SceneComponent::draw)
        .def("draw_schematic", &Bess::Canvas::SceneComponent::drawSchematic);
}
