#include "scene/renderer/vulkan/path_renderer.h"
#include <pybind11/pybind11.h>

namespace py = pybind11;

void bind_path_renderer(py::module_ &m) {

    py::class_<Bess::Renderer2D::Vulkan::ContoursDrawInfo, py::smart_holder>(m, "ContoursDrawInfo")
        .def(py::init<>())
        .def_readwrite("gen_stroke", &Bess::Renderer2D::Vulkan::ContoursDrawInfo::genStroke)
        .def_readwrite("gen_fill", &Bess::Renderer2D::Vulkan::ContoursDrawInfo::genFill)
        .def_readwrite("close_path", &Bess::Renderer2D::Vulkan::ContoursDrawInfo::closePath)
        .def_readwrite("rouned_joint", &Bess::Renderer2D::Vulkan::ContoursDrawInfo::rounedJoint)
        .def_readwrite("translate", &Bess::Renderer2D::Vulkan::ContoursDrawInfo::translate)
        .def_readwrite("scale", &Bess::Renderer2D::Vulkan::ContoursDrawInfo::scale)
        .def_readwrite("fill_color", &Bess::Renderer2D::Vulkan::ContoursDrawInfo::fillColor)
        .def_readwrite("stroke_color", &Bess::Renderer2D::Vulkan::ContoursDrawInfo::strokeColor)
        .def_readwrite("glyph_id", &Bess::Renderer2D::Vulkan::ContoursDrawInfo::glyphId);

    py::class_<Bess::Renderer2D::Vulkan::PathRenderer, py::smart_holder>(m, "PathRenderer")
        .def("drawPath", &Bess::Renderer2D::Vulkan::PathRenderer::drawPath,
             py::arg("path"), py::arg("info") = Bess::Renderer2D::Vulkan::ContoursDrawInfo{});
}
