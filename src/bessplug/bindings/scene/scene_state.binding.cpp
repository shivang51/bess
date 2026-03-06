#include "scene/scene_state/scene_state.h"
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

namespace py = pybind11;

void bind_scene_state(py::module_ &m) {

    const auto &getMaterialRenderer = [](Bess::Canvas::SceneState &state) {
        const auto &viewport = state.getViewport();
        if (viewport) {
            return viewport->getRenderers().materialRenderer;
        }
        return std::shared_ptr<Bess::Renderer::MaterialRenderer>(nullptr);
    };

    const auto &getPathRenderer = [](Bess::Canvas::SceneState &state) {
        const auto &viewport = state.getViewport();
        if (viewport) {
            return viewport->getRenderers().pathRenderer;
        }
        return std::shared_ptr<Bess::Renderer2D::Vulkan::PathRenderer>(nullptr);
    };

    py::class_<Bess::Canvas::SceneState>(m, "SceneState")
        .def(py::init<Bess::Canvas::SceneState &>())
        .def("clear", &Bess::Canvas::SceneState::clear)
        .def("get_material_renderer", getMaterialRenderer, py::return_value_policy::reference)
        .def("get_path_renderer", getPathRenderer, py::return_value_policy::reference)
        .def("is_root_component", &Bess::Canvas::SceneState::isRootComponent)
        // .def("get_all_components", &Bess::Canvas::SceneState::getAllComponents, py::return_value_policy::reference)
        .def("get_selected_components", &Bess::Canvas::SceneState::getSelectedComponents, py::return_value_policy::reference);
}
