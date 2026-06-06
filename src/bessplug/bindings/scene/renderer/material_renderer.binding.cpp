#include "bess_core/renderer/renderer_2d.h"
#include "scene/renderer/material_renderer.h"
#include "vulkan_subtexture.h"
#include "vulkan_texture.h"
#include <algorithm>
#include <pybind11/pybind11.h>
#include <string>

namespace py = pybind11;

namespace {
    using Bess::Core::Renderer::CircleProps;
    using Bess::Core::Renderer::Color;
    using Bess::Core::Renderer::FontProps;
    using Bess::Core::Renderer::IRenderer2D;
    using Bess::Core::Renderer::LineProps;
    using Bess::Core::Renderer::PathCommandStroke;
    using Bess::Core::Renderer::PathLineJoin;
    using Bess::Core::Renderer::PathProps;
    using Bess::Core::Renderer::QuadProps;
    using Bess::Core::Renderer::RoundedBorderProps;

    bool hasAnyNonZero(const glm::vec4 &value) {
        return value.x != 0.f || value.y != 0.f || value.z != 0.f ||
               value.w != 0.f;
    }

    Bess::PickingId toPickingId(uint64_t id) {
        return Bess::PickingId::fromUint64(id);
    }

    QuadProps makeQuadProps(const glm::vec3 &pos, const glm::vec2 &size,
                            const glm::vec4 &color, uint64_t id,
                            const Bess::Renderer::QuadRenderProperties &props) {
        QuadProps quad;
        quad.position = {pos.x, pos.y};
        quad.size = size;
        quad.rotation = props.angle;
        quad.zIndex = pos.z;
        quad.color = color;
        quad.id = toPickingId(id);
        return quad;
    }

    void drawRendererQuad(IRenderer2D &renderer, const glm::vec3 &pos,
                          const glm::vec2 &size, const glm::vec4 &color,
                          uint64_t id,
                          const Bess::Renderer::QuadRenderProperties &props) {
        auto quad = makeQuadProps(pos, size, color, id, props);
        if (hasAnyNonZero(props.borderRadius) ||
            hasAnyNonZero(props.borderSize)) {
            RoundedBorderProps border;
            border.radius = props.borderRadius;
            border.thickness = props.borderSize;
            border.color = props.borderColor;
            renderer.drawRoundedQuad(quad, border);
            return;
        }
        renderer.drawQuad(quad);
    }
} // namespace

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
        .def_readwrite("borderColor",
                       &Bess::Renderer::QuadRenderProperties::borderColor)
        .def_readwrite("borderRadius",
                       &Bess::Renderer::QuadRenderProperties::borderRadius)
        .def_readwrite("borderSize",
                       &Bess::Renderer::QuadRenderProperties::borderSize)
        .def_readwrite("shadow", &Bess::Renderer::QuadRenderProperties::shadow)
        .def_readwrite("isMica", &Bess::Renderer::QuadRenderProperties::isMica);

    py::class_<Bess::Vulkan::VulkanTexture, py::smart_holder>(m,
                                                              "VulkanTexture");

    const auto createSubTexture = [](std::shared_ptr<VulkanTexture> texture,
                                     const glm::vec2 &coord,
                                     const glm::vec2 &spriteSize, float margin,
                                     const glm::vec2 &cellSize) {
        py::gil_scoped_acquire gil;
        return std::make_shared<Bess::Vulkan::SubTexture>(
            std::move(texture), coord, spriteSize, margin, cellSize);
    };

    py::class_<Bess::Vulkan::SubTexture, py::smart_holder>(m, "SubTexture")
        .def_static("create", createSubTexture,
                    "Create a SubTexture from a VulkanTexture with margin and "
                    "cell size",
                    py::arg("texture"), py::arg("coord"),
                    py::arg("sprite_size"), py::arg("margin"),
                    py::arg("cell_size"))
        .def_property_readonly("size", &Bess::Vulkan::SubTexture::getScale,
                               "Get the size of the SubTexture");

    const auto draw_textured_quad_overload =
        static_cast<void (Bess::Renderer::MaterialRenderer::*)(
            const glm::vec3 &, const glm::vec2 &, const glm::vec4 &, uint64_t,
            const std::shared_ptr<Bess::Vulkan::VulkanTexture> &,
            Bess::Renderer::QuadRenderProperties)>(
            &Bess::Renderer::MaterialRenderer::drawTexturedQuad);

    const auto draw_textured_quad_subtexture_overload =
        static_cast<void (Bess::Renderer::MaterialRenderer::*)(
            const glm::vec3 &, const glm::vec2 &, const glm::vec4 &, uint64_t,
            const std::shared_ptr<Bess::Vulkan::SubTexture> &,
            Bess::Renderer::QuadRenderProperties)>(
            &Bess::Renderer::MaterialRenderer::drawTexturedQuad);

    py::class_<Bess::Renderer::MaterialRenderer, py::smart_holder>(
        m, "MaterialRenderer")
        .def_static("get_text_render_size",
                    &Bess::Renderer::MaterialRenderer::getTextRenderSize,
                    "Calculate the size of the rendered text", py::arg("text"),
                    py::arg("render_size"))
        .def("draw_quad", &Bess::Renderer::MaterialRenderer::drawQuad,
             "Draw a colored quad on the screen", py::arg("pos"),
             py::arg("size"), py::arg("color"), py::arg("id"), py::arg("props"))
        .def("draw_textured_quad", draw_textured_quad_overload,
             "Draw a textured quad on the screen using a VulkanTexture",
             py::arg("pos"), py::arg("size"), py::arg("tint"), py::arg("id"),
             py::arg("texture"), py::arg("props"))
        .def("draw_sub_textured_quad", draw_textured_quad_subtexture_overload,
             "Draw a textured quad on the screen using a SubTexture",
             py::arg("pos"), py::arg("size"), py::arg("tint"), py::arg("id"),
             py::arg("sub_texture"), py::arg("props"))
        .def("draw_circle", &Bess::Renderer::MaterialRenderer::drawCircle,
             "Draw a colored circle on the screen", py::arg("center"),
             py::arg("radius"), py::arg("color"), py::arg("id"),
             py::arg("inner_radius") = 0.0f)
        .def("draw_line", &Bess::Renderer::MaterialRenderer::drawLine,
             py::arg("start"), py::arg("end"), py::arg("thickness"),
             py::arg("color"), py::arg("id"))
        .def("draw_text", &Bess::Renderer::MaterialRenderer::drawText,
             "Draw text on the screen", py::arg("text"), py::arg("position"),
             py::arg("size"), py::arg("color"), py::arg("id"),
             py::arg("angle") = 0.0f);

    py::class_<Bess::Core::Renderer::IRenderer2D,
               std::shared_ptr<Bess::Core::Renderer::IRenderer2D>>(
        m, "IRenderer2D")
        .def("draw_quad",
             [](IRenderer2D &renderer, const glm::vec3 &pos,
                const glm::vec2 &size, const glm::vec4 &color, uint64_t id) {
                 drawRendererQuad(renderer, pos, size, color, id, {});
             },
             py::arg("pos"), py::arg("size"), py::arg("color"), py::arg("id"))
        .def("draw_quad", &drawRendererQuad, py::arg("pos"), py::arg("size"),
             py::arg("color"), py::arg("id"), py::arg("props"))
        .def("draw_circle",
             [](IRenderer2D &renderer, const glm::vec3 &center, float radius,
                const glm::vec4 &color, uint64_t id, float innerRadius) {
                 CircleProps props;
                 props.position = {center.x, center.y};
                 props.radius = radius;
                 props.thickness = innerRadius > 0.f
                                       ? std::max(0.f, radius - innerRadius)
                                       : 0.f;
                 props.zIndex = center.z;
                 props.color = color;
                 props.id = toPickingId(id);
                 renderer.drawCircle(props);
             },
             py::arg("center"), py::arg("radius"), py::arg("color"),
             py::arg("id"), py::arg("inner_radius") = 0.0f)
        .def("draw_line",
             [](IRenderer2D &renderer, const glm::vec3 &start,
                const glm::vec3 &end, float thickness,
                const glm::vec4 &color, uint64_t id) {
                 LineProps props;
                 props.p0 = {start.x, start.y};
                 props.p1 = {end.x, end.y};
                 props.thickness = thickness;
                 props.zIndex = (start.z + end.z) * 0.5f;
                 props.color = color;
                 props.id = toPickingId(id);
                 renderer.drawLine(props);
             },
             py::arg("start"), py::arg("end"), py::arg("thickness"),
             py::arg("color"), py::arg("id"))
        .def("draw_text",
             [](IRenderer2D &renderer, const std::string &text,
                const glm::vec3 &pos, size_t size, const glm::vec4 &color,
                uint64_t id, float angle) {
                 (void)angle;
                 FontProps props;
                 props.position = {pos.x, pos.y};
                 props.fontSize = static_cast<float>(size);
                 props.color = color;
                 props.zIndex = pos.z;
                 props.id = toPickingId(id);
                 renderer.drawFont(text, props);
             },
             py::arg("text"), py::arg("position"), py::arg("size"),
             py::arg("color"), py::arg("id"), py::arg("angle") = 0.0f)
        .def("begin_path",
             [](IRenderer2D &renderer, const glm::vec3 &startPos, float weight,
                const glm::vec4 &color, uint64_t id, bool closePath,
                bool renderFill, const glm::vec4 &fillColor, bool renderStroke,
                bool roundedJoints) {
                 PathProps props;
                 props.strokeColor =
                     renderStroke ? Color(color) : Color(0.f, 0.f, 0.f, 0.f);
                 props.strokeSize = renderStroke ? weight : 0.f;
                 props.fillColor = fillColor;
                 props.renderFill = renderFill;
                 props.closePath = closePath;
                 props.zIndex = startPos.z;
                 props.id = toPickingId(id);
                 props.lineJoin =
                     roundedJoints ? PathLineJoin::Round : PathLineJoin::Miter;
                 renderer.beginPath(props);
                 renderer.pathMoveTo({startPos.x, startPos.y});
             },
             py::arg("start_pos"), py::arg("weight"), py::arg("color"),
             py::arg("id"), py::arg("close_path") = false,
             py::arg("render_fill") = false,
             py::arg("fill_color") = glm::vec4(1.f),
             py::arg("render_stroke") = true,
             py::arg("rounded_joints") = false)
        .def("path_line_to",
             [](IRenderer2D &renderer, const glm::vec3 &pos, float weight) {
                 renderer.pathLineTo({pos.x, pos.y},
                                     PathCommandStroke::withWidth(weight));
             },
             py::arg("pos"), py::arg("weight"))
        .def("path_cubic_to",
             [](IRenderer2D &renderer, const glm::vec3 &end,
                const glm::vec2 &controlPoint1,
                const glm::vec2 &controlPoint2, float weight) {
                 renderer.pathCubicBezierTo(
                     controlPoint1, controlPoint2, {end.x, end.y},
                     PathCommandStroke::withWidth(weight));
             },
             py::arg("end"), py::arg("control_point_1"),
             py::arg("control_point_2"), py::arg("weight"))
        .def("end_path", &IRenderer2D::endPath);
}
