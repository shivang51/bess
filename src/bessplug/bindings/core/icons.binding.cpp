#include "ui/icons/ComponentIcons_Remapped.h"

#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

namespace py = pybind11;
using namespace Bess::UI::Icons;

void bind_icons(py::module_ &m) {
    auto iconsModule =
        m.def_submodule("icons", "Module for accessing component icons");

    auto compIconsMod =
        iconsModule.def_submodule("component_icons", "Component icons");

    compIconsMod.attr("AND_GATE") = py::cast(ComponentIcons::AND_GATE);
    compIconsMod.attr("OR_GATE") = py::cast(ComponentIcons::OR_GATE);
    compIconsMod.attr("NOT_GATE") = py::cast(ComponentIcons::NOT_GATE);
    compIconsMod.attr("NAND_GATE") = py::cast(ComponentIcons::NAND_GATE);
    compIconsMod.attr("NOR_GATE") = py::cast(ComponentIcons::NOR_GATE);
    compIconsMod.attr("XOR_GATE") = py::cast(ComponentIcons::XOR_GATE);
    compIconsMod.attr("XNOR_GATE") = py::cast(ComponentIcons::XNOR_GATE);
}
