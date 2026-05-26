#include "pages/main_page/scene_components/slot_scene_component.h"
#include "pybind11/pybind11.h"
#include "scene/scene_state/components/scene_component_types.h"

namespace py = pybind11;

void bind_scene_common_binding(py::module &m) {

    py::class_<Bess::Canvas::Transform>(m, "Transform")
        .def(py::init<>())
        .def_readwrite("position", &Bess::Canvas::Transform::position)
        .def_readwrite("angle", &Bess::Canvas::Transform::angle)
        .def_readwrite("scale", &Bess::Canvas::Transform::scale);

    // PickingId class binding
    py::class_<Bess::PickingId>(m, "PickingId")
        .def(py::init<>())
        .def_readwrite("runtime_id", &Bess::PickingId::runtimeId)
        .def_readwrite("info", &Bess::PickingId::info)
        .def_static("invalid", &Bess::PickingId::invalid)
        .def("asUint64",
             [](const Bess::PickingId &self) {
                 return static_cast<uint64_t>(self);
             })
        .def("__eq__", &Bess::PickingId::operator==);

    py::enum_<Bess::Canvas::SlotType>(m, "SlotType")
        .value("dInp", Bess::Canvas::SlotType::digitalInput,
               "Digital Input Slot")
        .value("dOut", Bess::Canvas::SlotType::digitalOutput,
               "Digital Output Slot")
        .export_values();
}
