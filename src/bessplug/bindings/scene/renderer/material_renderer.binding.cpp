#include "scene/renderer/material_renderer.h"
#include "vulkan_texture.h"
#include <pybind11/pybind11.h>

namespace py = pybind11;

void bind_material_renderer(py::module_ &m) {
    py::class_<Bess::Renderer::QuadRenderProperties>(m, "QuadRenderProperties")
        .def(py::init<>())
        .def_readwrite("angle", &Bess::Renderer::QuadRenderProperties::angle)
        .def_readwrite("borderColor", &Bess::Renderer::QuadRenderProperties::borderColor)
        .def_readwrite("borderRadius", &Bess::Renderer::QuadRenderProperties::borderRadius)
        .def_readwrite("borderSize", &Bess::Renderer::QuadRenderProperties::borderSize)
        .def_readwrite("hasShadow", &Bess::Renderer::QuadRenderProperties::hasShadow)
        .def_readwrite("isMica", &Bess::Renderer::QuadRenderProperties::isMica);

    py::class_<Bess::Renderer::MaterialRenderer, py::smart_holder>(m, "MaterialRenderer")
        .def("get_text_render_size", &Bess::Renderer::MaterialRenderer::getTextRenderSize,
             "Calculate the size of the rendered text",
             py::arg("text"),
             py::arg("render_size"))
        .def("draw_quad", &Bess::Renderer::MaterialRenderer::drawQuad,
             "Draw a colored quad on the screen",
             py::arg("pos"),
             py::arg("size"),
             py::arg("color"),
             py::arg("id"),
             py::arg("props"))
        .def("draw_circle", &Bess::Renderer::MaterialRenderer::drawCircle,
             "Draw a colored circle on the screen",
             py::arg("center"),
             py::arg("radius"),
             py::arg("color"),
             py::arg("id"),
             py::arg("inner_radius") = 0.0f)
        .def("draw_text", &Bess::Renderer::MaterialRenderer::drawText,
             "Draw text on the screen",
             py::arg("text"),
             py::arg("position"),
             py::arg("size"),
             py::arg("color"),
             py::arg("id"),
             py::arg("angle") = 0.0f);
}
