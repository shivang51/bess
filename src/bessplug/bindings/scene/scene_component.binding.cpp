
#include "scene/scene_state/components/scene_component.h"
#include "camera.h"
#include "scene/scene_state/scene_state.h" // included for pybind11
#include "scene_draw_context.h"
#include <pybind11/functional.h>
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>
#include <pybind11/stl_bind.h>

namespace py = pybind11;

class PySceneComponent : public Bess::Canvas::SceneComponent,
                         public py::trampoline_self_life_support {
  public:
    PySceneComponent() = default;

    void draw(Bess::SceneDrawContext &context) override {
        PYBIND11_OVERRIDE(
            void,
            Bess::Canvas::SceneComponent,
            draw,
            std::ref(context));
    }

    void drawSchematic(Bess::SceneDrawContext &context) override {
        PYBIND11_OVERRIDE_NAME(
            void,
            Bess::Canvas::SceneComponent,
            "draw_schematic",
            drawSchematic,
            std::ref(context));
    }

    void update(Bess::TimeMs timeStep, Bess::Canvas::SceneState &state) override {
        PYBIND11_OVERRIDE(
            void,
            Bess::Canvas::SceneComponent,
            update,
            timeStep,
            std::ref(state));
    }

    std::string getTypeName() override {
        PYBIND11_OVERRIDE_NAME(
            std::string,
            Bess::Canvas::SceneComponent,
            "get_type_name",
            getTypeName);
    }
};

void bind_scene_component(py::module_ &m) {

    py::class_<Bess::Camera>(m, "Camera")
        .def("get_pos", &Bess::Camera::getPos);

    py::class_<Bess::SceneDrawContext>(m, "SceneDrawContext")
        .def_readonly("scene_state", &Bess::SceneDrawContext::sceneState)
        .def_readonly("material_renderer", &Bess::SceneDrawContext::materialRenderer)
        .def_readonly("path_renderer", &Bess::SceneDrawContext::pathRenderer)
        .def_readonly("camera", &Bess::SceneDrawContext::camera);

    py::class_<Bess::Canvas::SceneComponent,
               PySceneComponent,
               py::smart_holder>(m, "SceneComponent")
        .def(py::init<>())
        .def("draw", &Bess::Canvas::SceneComponent::draw, py::arg("context"))
        .def("draw_schematic", &Bess::Canvas::SceneComponent::drawSchematic, py::arg("context"))
        .def("update", &Bess::Canvas::SceneComponent::update,
             py::arg("time_step"),
             py::arg("scene_state"))
        .def("get_type_name", &Bess::Canvas::SceneComponent::getTypeName);
}
