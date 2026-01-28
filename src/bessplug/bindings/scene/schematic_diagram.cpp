#include "scene/schematic_diagram.h"

#include <pybind11/functional.h>
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>
#include <pybind11/stl_bind.h>

namespace py = pybind11;

void bind_scene_schematic_diagram(py::module_ &m) {
    py::class_<Bess::Canvas::SchematicDiagram>(m, "SchematicDiagram")
        .def(py::init<>())
        .def_property("paths", &Bess::Canvas::SchematicDiagram::getPathsMut,
                      &Bess::Canvas::SchematicDiagram::setPaths)
        .def_property("size",
                      &Bess::Canvas::SchematicDiagram::getSize,
                      &Bess::Canvas::SchematicDiagram::setSize)
        .def("add_path", &Bess::Canvas::SchematicDiagram::addPath)
        .def("normalize_paths", &Bess::Canvas::SchematicDiagram::normalizePaths)
        .def_property("show_name",
                      &Bess::Canvas::SchematicDiagram::getShowName,
                      &Bess::Canvas::SchematicDiagram::setShowName)
        .def_property("stroke_size",
                      &Bess::Canvas::SchematicDiagram::getStrokeSize,
                      &Bess::Canvas::SchematicDiagram::setStrokeSize);
}
