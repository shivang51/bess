#include "scene/scene_state/components/sim_scene_comp_draw_hook.h"
#include "scene/scene_state/scene_state.h"
#include <pybind11/pybind11.h>

namespace py = pybind11;

class PySimCompDrawHook : public Bess::Canvas::SimSceneCompDrawHook,
                          public py::trampoline_self_life_support {
  public:
    using Bess::Canvas::SimSceneCompDrawHook::SimSceneCompDrawHook;

    ~PySimCompDrawHook() override {
        py::gil_scoped_acquire gil;
    }

    void onDraw(Bess::Canvas::SceneState &state,
                std::shared_ptr<Bess::Renderer::MaterialRenderer> materialRenderer,
                std::shared_ptr<Bess::Renderer2D::Vulkan::PathRenderer> pathRenderer) override {
        PYBIND11_OVERRIDE_PURE(
            void,                               /* Return type */
            Bess::Canvas::SimSceneCompDrawHook, /* Parent class */
            onDraw,                             /* Name of function in C++ (must match Python name) */
            std::ref(state),                    /* Argument(s) */
            materialRenderer,
            pathRenderer);
    }

    void onSchematicDraw(Bess::Canvas::SceneState &state,
                         std::shared_ptr<Bess::Renderer::MaterialRenderer> materialRenderer,
                         std::shared_ptr<Bess::Renderer2D::Vulkan::PathRenderer> pathRenderer) override {
        PYBIND11_OVERRIDE_PURE(
            void,                               /* Return type */
            Bess::Canvas::SimSceneCompDrawHook, /* Parent class */
            onSchematicDraw,                    /* Name of function in C++ (must match Python name) */
            state,                              /* Argument(s) */
            materialRenderer,
            pathRenderer);
    }
};

void bind_sim_comp_draw_hook(py::module &m) {
    py::class_<Bess::Canvas::SimSceneCompDrawHook,
               PySimCompDrawHook,
               py::smart_holder>(m, "SimCompDrawHook")
        .def(py::init<>())
        .def("onDraw",
             &Bess::Canvas::SimSceneCompDrawHook::onDraw,
             py::arg("state"),
             py::arg("materialRenderer"),
             py::arg("pathRenderer"))
        .def("onSchematicDraw",
             &Bess::Canvas::SimSceneCompDrawHook::onSchematicDraw,
             py::arg("state"),
             py::arg("materialRenderer"),
             py::arg("pathRenderer"));
}
