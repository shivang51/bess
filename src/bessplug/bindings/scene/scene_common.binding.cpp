#include "bess_core/scene/scene_state/components/scene_component_types.h"
#include "pages/main_page/scene_components/slot_scene_component.h"
#include "pybind11/pybind11.h"

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

    py::enum_<Bess::Canvas::PinLabelAlignment>(m, "PinLabelAlignment")
        .value("adjacent",
               Bess::Canvas::PinLabelAlignment::adjacent,
               "Left or right of the pin, depending on input or output")
        .value("top_center",
               Bess::Canvas::PinLabelAlignment::topCenter,
               "Top center of the pin")
        .value("bottom_center",
               Bess::Canvas::PinLabelAlignment::bottomCenter,
               "Bottom center of the pin")
        .export_values();

    py::enum_<Bess::Canvas::SchematicLableAlignement>(
        m, "SchematicLableAlignement")
        .value("center",
               Bess::Canvas::SchematicLableAlignement::center,
               "Center of the component")
        .value("top_center",
               Bess::Canvas::SchematicLableAlignement::topCenter,
               "Top center of the component")
        .value("bottom_center",
               Bess::Canvas::SchematicLableAlignement::bottomCenter,
               "Bottom center of the component")
        .export_values();

    py::class_<Bess::Canvas::SchematicStyle>(m, "SchematicStyle")
        .def(py::init<>())
        .def_readwrite("pin_label_align",
                       &Bess::Canvas::SchematicStyle::pinLabelAlign)
        .def_readwrite("show_pin_labels",
                       &Bess::Canvas::SchematicStyle::showPinLabels)
        .def_readwrite("schematic_label_align",
                       &Bess::Canvas::SchematicStyle::schematicLabelAlign)
        .def_readwrite("show_name", &Bess::Canvas::SchematicStyle::showName)
        .def_readwrite("flip_slots_x",
                       &Bess::Canvas::SchematicStyle::flipSlotsX);

    py::class_<Bess::Canvas::Style>(m, "Style")
        .def(py::init<>())
        .def_readwrite("schem_style", &Bess::Canvas::Style::schematicStyle)
        .def_readwrite("header_color", &Bess::Canvas::Style::headerColor)
        .def_readwrite("color", &Bess::Canvas::Style::color);
}
