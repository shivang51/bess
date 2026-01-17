#include "scene/renderer/material_renderer.h"
#include <pybind11/pybind11.h>

namespace py = pybind11;

void bind_material_renderer(py::module_ &m) {
    py::class_<Bess::Renderer::MaterialRenderer, py::smart_holder>(m, "MaterialRenderer")
        .def("drawText", &Bess::Renderer::MaterialRenderer::drawText,
             py::arg("text"),
             py::arg("position"),
             py::arg("size"),
             py::arg("color"),
             py::arg("id"),
             py::arg("angle") = 0.0f);
}
