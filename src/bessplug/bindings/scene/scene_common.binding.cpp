#include "pybind11/pybind11.h"
#include "scene/scene_state/components/scene_component_types.h"
#include <iostream>

namespace py = pybind11;

void bind_scene_common_binding(py::module &m) {

    py::class_<Bess::Canvas::Transform>(m, "Transform")
        .def(py::init<>())
        .def_readwrite("position", &Bess::Canvas::Transform::position)
        .def_readwrite("angle", &Bess::Canvas::Transform::angle)
        .def_readwrite("scale", &Bess::Canvas::Transform::scale);

    // PickingId class binding
    py::class_<Bess::Canvas::PickingId>(m, "PickingId")
        .def(py::init<>())
        .def_readwrite("runtime_id", &Bess::Canvas::PickingId::runtimeId)
        .def_readwrite("info", &Bess::Canvas::PickingId::info)
        .def_static("invalid", &Bess::Canvas::PickingId::invalid)
        .def("asUint64", [](const Bess::Canvas::PickingId &self) {
            return static_cast<uint64_t>(self);
        })
        .def("__eq__", &Bess::Canvas::PickingId::operator==);
}
