#include "bess_core/scene/scene_state/scene_state.h"
#include "pybind11_abseil/absl_casters.h"
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

namespace py = pybind11;

void bind_scene_state(py::module_ &m) {

    py::class_<Bess::Canvas::SceneState>(m, "SceneState")
        .def(py::init<Bess::Canvas::SceneState &>())
        .def("clear", &Bess::Canvas::SceneState::clear)
        .def("is_root_component", &Bess::Canvas::SceneState::isRootComponent)
        .def("get_component_by_uuid",
             [](const Bess::Canvas::SceneState &self,
                const Bess::UUID &uuid) -> py::object {
                 return py::cast(self.getComponentByUuidSP(uuid));
             },
             py::arg("uuid"))
        .def_property_readonly(
            "ui_node_registry",
            [](Bess::Canvas::SceneState &self) -> py::object {
                return py::cast(self.getUINodeRegistry());
            })
        // .def("get_all_components",
        //      &Bess::Canvas::SceneState::getAllComponents,
        //      py::return_value_policy::reference)
        .def("get_selected_components",
             &Bess::Canvas::SceneState::getSelectedComponents,
             py::return_value_policy::reference);
}
