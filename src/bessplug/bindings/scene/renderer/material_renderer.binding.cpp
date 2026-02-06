#include "scene/renderer/material_renderer.h"
#include "vulkan_subtexture.h"
#include "vulkan_texture.h"
#include <pybind11/pybind11.h>

namespace py = pybind11;

void bind_material_renderer(py::module_ &m) {

    py::class_<Bess::Renderer::ShadowProps>(m, "ShadowProps")
        .def(py::init<>())
        .def_readwrite("enabled", &Bess::Renderer::ShadowProps::enabled)
        .def_readwrite("offset", &Bess::Renderer::ShadowProps::offset)
        .def_readwrite("scale", &Bess::Renderer::ShadowProps::scale)
        .def_readwrite("color", &Bess::Renderer::ShadowProps::color);

    py::class_<Bess::Renderer::QuadRenderProperties>(m, "QuadRenderProperties")
        .def(py::init<>())
        .def_readwrite("angle", &Bess::Renderer::QuadRenderProperties::angle)
        .def_readwrite("borderColor", &Bess::Renderer::QuadRenderProperties::borderColor)
        .def_readwrite("borderRadius", &Bess::Renderer::QuadRenderProperties::borderRadius)
        .def_readwrite("borderSize", &Bess::Renderer::QuadRenderProperties::borderSize)
        .def_readwrite("shadow", &Bess::Renderer::QuadRenderProperties::shadow)
        .def_readwrite("isMica", &Bess::Renderer::QuadRenderProperties::isMica);

    py::class_<Bess::Vulkan::VulkanTexture, py::smart_holder>(m, "VulkanTexture");

    const auto createSubTexture = [](std::shared_ptr<VulkanTexture> texture,
                                     const glm::vec2 &coord,
                                     const glm::vec2 &spriteSize,
                                     float margin, const glm::vec2 &cellSize) {
        py::gil_scoped_acquire gil;
        return std::make_shared<Bess::Vulkan::SubTexture>(std::move(texture), coord, spriteSize, margin, cellSize);
    };

    py::class_<Bess::Vulkan::SubTexture, py::smart_holder>(m, "SubTexture")
        .def_static("create", createSubTexture,
                    "Create a SubTexture from a VulkanTexture with margin and cell size",
                    py::arg("texture"),
                    py::arg("coord"),
                    py::arg("sprite_size"),
                    py::arg("margin"),
                    py::arg("cell_size"))
        .def_property_readonly("size", &Bess::Vulkan::SubTexture::getScale,
                               "Get the size of the SubTexture");

    const auto draw_textured_quad_overload = static_cast<void (Bess::Renderer::MaterialRenderer::*)(
        const glm::vec3 &,
        const glm::vec2 &,
        const glm::vec4 &,
        uint64_t,
        const std::shared_ptr<Bess::Vulkan::VulkanTexture> &,
        Bess::Renderer::QuadRenderProperties)>(&Bess::Renderer::MaterialRenderer::drawTexturedQuad);

    const auto draw_textured_quad_subtexture_overload = static_cast<void (Bess::Renderer::MaterialRenderer::*)(
        const glm::vec3 &,
        const glm::vec2 &,
        const glm::vec4 &,
        uint64_t,
        const std::shared_ptr<Bess::Vulkan::SubTexture> &,
        Bess::Renderer::QuadRenderProperties)>(&Bess::Renderer::MaterialRenderer::drawTexturedQuad);

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
        .def("draw_textured_quad", draw_textured_quad_overload,
             "Draw a textured quad on the screen using a VulkanTexture",
             py::arg("pos"),
             py::arg("size"),
             py::arg("tint"),
             py::arg("id"),
             py::arg("texture"),
             py::arg("props"))
        .def("draw_sub_textured_quad", draw_textured_quad_subtexture_overload,
             "Draw a textured quad on the screen using a SubTexture",
             py::arg("pos"),
             py::arg("size"),
             py::arg("tint"),
             py::arg("id"),
             py::arg("sub_texture"),
             py::arg("props"))
        .def("draw_circle",
             &Bess::Renderer::MaterialRenderer::drawCircle,
             "Draw a colored circle on the screen",
             py::arg("center"),
             py::arg("radius"),
             py::arg("color"),
             py::arg("id"),
             py::arg("inner_radius") = 0.0f)
        .def("draw_text",
             &Bess::Renderer::MaterialRenderer::drawText,
             "Draw text on the screen",
             py::arg("text"),
             py::arg("position"),
             py::arg("size"),
             py::arg("color"),
             py::arg("id"),
             py::arg("angle") = 0.0f);
}
