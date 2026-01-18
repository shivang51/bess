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

    Bess::Canvas::DrawHookOnDrawResult onDraw(const Bess::Canvas::Transform &transform,
                                              const Bess::Canvas::PickingId &pickingId,
                                              const Bess::SimEngine::ComponentState &compState,
                                              std::shared_ptr<Bess::Renderer::MaterialRenderer> materialRenderer,
                                              std::shared_ptr<Bess::Renderer2D::Vulkan::PathRenderer> pathRenderer) override {
        PYBIND11_OVERRIDE_PURE(
            Bess::Canvas::DrawHookOnDrawResult, /* Return type */
            Bess::Canvas::SimSceneCompDrawHook, /* Parent class */
            onDraw,                             /* Name of function in C++ (must match Python name) */
            std::ref(transform),                /* Argument(s) */
            std::ref(pickingId),
            std::ref(compState),
            materialRenderer,
            pathRenderer);
    }

    glm::vec2 onSchematicDraw(const Bess::Canvas::Transform &transform,
                              const Bess::Canvas::PickingId &pickingId,
                              std::shared_ptr<Bess::Renderer::MaterialRenderer> materialRenderer,
                              std::shared_ptr<Bess::Renderer2D::Vulkan::PathRenderer> pathRenderer) override {
        PYBIND11_OVERRIDE_PURE(
            glm::vec2,                          /* Return type */
            Bess::Canvas::SimSceneCompDrawHook, /* Parent class */
            onSchematicDraw,                    /* Name of function in C++ (must match Python name) */
            std::ref(transform),                /* Argument(s) */
            std::ref(pickingId),
            materialRenderer,
            pathRenderer);
    }
};

void bind_sim_comp_draw_hook(py::module &m) {
    py::class_<Bess::Canvas::DrawHookOnDrawResult>(m, "DrawHookOnDrawResult")
        .def(py::init<>())
        .def_readwrite("size_changed", &Bess::Canvas::DrawHookOnDrawResult::sizeChanged)
        .def_readwrite("new_size", &Bess::Canvas::DrawHookOnDrawResult::newSize)
        .def_readwrite("draw_original", &Bess::Canvas::DrawHookOnDrawResult::drawOriginal)
        .def_readwrite("draw_children", &Bess::Canvas::DrawHookOnDrawResult::drawChildren);

    py::class_<Bess::Canvas::SimSceneCompDrawHook,
               PySimCompDrawHook,
               py::smart_holder>(m, "SimCompDrawHook")
        .def(py::init<>())
        .def("onDraw",
             &Bess::Canvas::SimSceneCompDrawHook::onDraw,
             py::arg("transform"),
             py::arg("pickingId"),
             py::arg("compState"),
             py::arg("materialRenderer"),
             py::arg("pathRenderer"))
        .def("onSchematicDraw",
             &Bess::Canvas::SimSceneCompDrawHook::onSchematicDraw,
             py::arg("transform"),
             py::arg("pickingId"),
             py::arg("materialRenderer"),
             py::arg("pathRenderer"))
        .def_property("draw_enabled",
                      &Bess::Canvas::SimSceneCompDrawHook::isDrawEnabled,
                      &Bess::Canvas::SimSceneCompDrawHook::setDrawEnabled)
        .def_property("schematic_draw_enabled",
                      &Bess::Canvas::SimSceneCompDrawHook::isSchematicDrawEnabled,
                      &Bess::Canvas::SimSceneCompDrawHook::setSchematicDrawEnabled);
}
