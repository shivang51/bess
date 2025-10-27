#include "scene/schematic_diagram.h"

#include <pybind11/functional.h>
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>
#include <pybind11/stl_bind.h>

namespace py = pybind11;

void bind_scene_schematic_diagram(py::module_ &m) {
    py::bind_vector<std::vector<Bess::Renderer::Path>>(m, "PathVector");
    py::class_<Bess::Canvas::SchematicDiagram>(m, "SchematicDiagram")
        .def(py::init<>())
        .def("get_paths", &Bess::Canvas::SchematicDiagram::getPaths, py::return_value_policy::reference_internal)
        .def("set_paths", &Bess::Canvas::SchematicDiagram::setPaths)
        .def("get_size", &Bess::Canvas::SchematicDiagram::getSize, py::return_value_policy::reference_internal)
        .def("set_size", &Bess::Canvas::SchematicDiagram::setSize)
        .def("add_path", [](Bess::Canvas::SchematicDiagram &self, const Bess::Renderer::Path &p) {
            self.getPathsMut().emplace_back(p);
        });
}
