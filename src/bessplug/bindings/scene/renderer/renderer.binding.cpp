#include "bess_core/renderer/renderer_2d.h"
#include "bess_core/renderer/renderer_types.h"
#include "bess_core/renderer/subtexture.h"
#include "bess_core/renderer/texture.h"
#include "bess_wgpu/wgpu_texture.h"
#include <algorithm>
#include <memory>
#include <pybind11/pybind11.h>
#include <string>

namespace py = pybind11;

namespace {
    using Bess::Core::Renderer::CircleProps;
    using Bess::Core::Renderer::Color;
    using Bess::Core::Renderer::FontProps;
    using Bess::Core::Renderer::IRenderer2D;
    using Bess::Core::Renderer::ITexture;
    using Bess::Core::Renderer::LineProps;
    using Bess::Core::Renderer::PathCommandStroke;
    using Bess::Core::Renderer::PathLineJoin;
    using Bess::Core::Renderer::PathProps;
    using Bess::Core::Renderer::QuadProps;
    using Bess::Wgpu::WgpuTexture;

    struct PySubTexture {
        std::shared_ptr<WgpuTexture> texture;
        Bess::Core::Renderer::SubTexture uv;

        [[nodiscard]] glm::vec2 getScale() const noexcept {
            return uv.getPixelSize();
        }
    };

    bool hasAnyNonZero(const glm::vec4 &value) {
        return value.x != 0.f || value.y != 0.f || value.z != 0.f ||
               value.w != 0.f;
    }

    Bess::PickingId toPickingId(uint64_t id) {
        return Bess::PickingId::fromUint64(id);
    }

    void drawRendererQuad(IRenderer2D &renderer,
                          const glm::vec3 &pos,
                          const glm::vec2 &size,
                          const glm::vec4 &color,
                          uint64_t id,
                          float angle = 0.f) {

        QuadProps quad;
        quad.position = {pos.x, pos.y};
        quad.size = size;
        quad.rotation = angle;
        quad.zIndex = pos.z;
        quad.color = color;
        quad.id = toPickingId(id);
        renderer.drawQuad(quad);
    }

    void drawRendererTexturedQuad(IRenderer2D &renderer,
                                  const glm::vec3 &pos,
                                  const glm::vec2 &size,
                                  const glm::vec4 &tint,
                                  uint64_t id,
                                  const std::shared_ptr<WgpuTexture> &texture,
                                  float angle = 0.f) {
        if (!texture) {
            return;
        }

        QuadProps quad;
        quad.position = {pos.x, pos.y};
        quad.size = size;
        quad.rotation = angle;
        quad.zIndex = pos.z;
        quad.color = tint;
        quad.id = toPickingId(id);
        quad.texture = texture->getHandle();
        renderer.drawQuad(quad);
    }

    void
    drawRendererSubTexturedQuad(IRenderer2D &renderer,
                                const glm::vec3 &pos,
                                const glm::vec2 &size,
                                const glm::vec4 &tint,
                                uint64_t id,
                                const std::shared_ptr<PySubTexture> &subTexture,
                                float angle = 0.f) {
        if (!subTexture || !subTexture->texture) {
            return;
        }

        QuadProps quad;
        quad.position = {pos.x, pos.y};
        quad.size = size;
        quad.rotation = angle;
        quad.zIndex = pos.z;
        quad.color = tint;
        quad.id = toPickingId(id);
        quad.texture = subTexture->texture->getHandle();
        const auto &uv = subTexture->uv.getStartWH();
        quad.uvRect = {uv.x, uv.y, uv.x + uv.z, uv.y + uv.w};
        renderer.drawQuad(quad);
    }

    void bindRendererTypes(py::module_ &m) {
    }
} // namespace

void bind_renderer(py::module_ &m) {

    bindRendererTypes(m);
    py::class_<WgpuTexture, std::shared_ptr<WgpuTexture>>(m, "Texture");

    const auto createSubTexture = [](std::shared_ptr<WgpuTexture> texture,
                                     const glm::vec2 &coord,
                                     const glm::vec2 &spriteSize,
                                     float margin,
                                     const glm::vec2 &cellSize) {
        py::gil_scoped_acquire gil;
        auto st = std::make_shared<PySubTexture>();
        st->texture = std::move(texture);
        if (st->texture) {
            const glm::vec2 texSize = st->texture->getSize();
            st->uv = Bess::Core::Renderer::SubTexture::fromGrid(
                texSize, coord, spriteSize, margin, cellSize);
        }
        return st;
    };

    py::class_<PySubTexture, py::smart_holder>(m, "SubTexture")
        .def_static("create",
                    createSubTexture,
                    "Create a SubTexture from a Texture (WGPU) with margin "
                    "and cell size",
                    py::arg("texture"),
                    py::arg("coord"),
                    py::arg("sprite_size"),
                    py::arg("margin"),
                    py::arg("cell_size"))
        .def_property_readonly(
            "size", &PySubTexture::getScale, "Get the size of the SubTexture");

    py::class_<Bess::Core::Renderer::IRenderer2D,
               std::shared_ptr<Bess::Core::Renderer::IRenderer2D>>(
        m, "IRenderer2D")
        .def(
            "get_text_render_size",
            [](IRenderer2D &renderer,
               const std::string &text,
               float renderSize) {
                FontProps props;
                props.fontSize = renderSize;
                return renderer.measureText(text, props);
            },
            py::arg("text"),
            py::arg("render_size"))
        .def(
            "draw_quad",
            [](IRenderer2D &renderer,
               const glm::vec3 &pos,
               const glm::vec2 &size,
               const glm::vec4 &color,
               uint64_t id) {
                drawRendererQuad(renderer, pos, size, color, id, {});
            },
            py::arg("pos"),
            py::arg("size"),
            py::arg("color"),
            py::arg("id"))
        .def("draw_quad",
             &drawRendererQuad,
             py::arg("pos"),
             py::arg("size"),
             py::arg("color"),
             py::arg("id"),
             py::arg("props"))
        .def("draw_quad",
             &drawRendererSubTexturedQuad,
             py::arg("pos"),
             py::arg("size"),
             py::arg("tint"),
             py::arg("id"),
             py::arg("sub_texture"),
             py::arg("props"))
        .def("draw_textured_quad",
             &drawRendererTexturedQuad,
             py::arg("pos"),
             py::arg("size"),
             py::arg("tint"),
             py::arg("id"),
             py::arg("texture"),
             py::arg("props"))
        .def("draw_sub_textured_quad",
             &drawRendererSubTexturedQuad,
             py::arg("pos"),
             py::arg("size"),
             py::arg("tint"),
             py::arg("id"),
             py::arg("sub_texture"),
             py::arg("props"))
        .def("draw_subtextured_quad",
             &drawRendererSubTexturedQuad,
             py::arg("pos"),
             py::arg("size"),
             py::arg("tint"),
             py::arg("id"),
             py::arg("sub_texture"),
             py::arg("props"))
        .def(
            "draw_circle",
            [](IRenderer2D &renderer,
               const glm::vec3 &center,
               float radius,
               const glm::vec4 &color,
               uint64_t id,
               float innerRadius) {
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
            py::arg("center"),
            py::arg("radius"),
            py::arg("color"),
            py::arg("id"),
            py::arg("inner_radius") = 0.0f)
        .def(
            "draw_line",
            [](IRenderer2D &renderer,
               const glm::vec3 &start,
               const glm::vec3 &end,
               float thickness,
               const glm::vec4 &color,
               uint64_t id) {
                LineProps props;
                props.p0 = {start.x, start.y};
                props.p1 = {end.x, end.y};
                props.thickness = thickness;
                props.zIndex = (start.z + end.z) * 0.5f;
                props.color = color;
                props.id = toPickingId(id);
                renderer.drawLine(props);
            },
            py::arg("start"),
            py::arg("end"),
            py::arg("thickness"),
            py::arg("color"),
            py::arg("id"))
        .def(
            "draw_text",
            [](IRenderer2D &renderer,
               const std::string &text,
               const glm::vec3 &pos,
               size_t size,
               const glm::vec4 &color,
               uint64_t id,
               float angle) {
                (void)angle;
                FontProps props;
                props.position = {pos.x, pos.y};
                props.fontSize = static_cast<float>(size);
                props.color = color;
                props.zIndex = pos.z;
                props.id = toPickingId(id);
                renderer.drawFont(text, props);
            },
            py::arg("text"),
            py::arg("position"),
            py::arg("size"),
            py::arg("color"),
            py::arg("id"),
            py::arg("angle") = 0.0f)
        .def(
            "begin_path",
            [](IRenderer2D &renderer,
               const glm::vec3 &startPos,
               float weight,
               const glm::vec4 &color,
               uint64_t id,
               bool closePath,
               bool renderFill,
               const glm::vec4 &fillColor,
               bool renderStroke,
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
            py::arg("start_pos"),
            py::arg("weight"),
            py::arg("color"),
            py::arg("id"),
            py::arg("close_path") = false,
            py::arg("render_fill") = false,
            py::arg("fill_color") = glm::vec4(1.f),
            py::arg("render_stroke") = true,
            py::arg("rounded_joints") = false)
        .def(
            "path_line_to",
            [](IRenderer2D &renderer, const glm::vec3 &pos, float weight) {
                renderer.pathLineTo({pos.x, pos.y},
                                    PathCommandStroke::withWidth(weight));
            },
            py::arg("pos"),
            py::arg("weight"))
        .def(
            "path_line_to_with_id",
            [](IRenderer2D &renderer,
               const glm::vec3 &pos,
               float weight,
               uint64_t id) {
                renderer.pathLineTo(
                    {pos.x, pos.y},
                    PathCommandStroke::withWidthAndId(weight, toPickingId(id)));
            },
            py::arg("pos"),
            py::arg("weight"),
            py::arg("id"))
        .def(
            "path_cubic_to",
            [](IRenderer2D &renderer,
               const glm::vec3 &end,
               const glm::vec2 &controlPoint1,
               const glm::vec2 &controlPoint2,
               float weight) {
                renderer.pathCubicTo(controlPoint1,
                                     controlPoint2,
                                     {end.x, end.y},
                                     PathCommandStroke::withWidth(weight));
            },
            py::arg("end"),
            py::arg("control_point_1"),
            py::arg("control_point_2"),
            py::arg("weight"))
        .def(
            "path_cubic_to_with_id",
            [](IRenderer2D &renderer,
               const glm::vec3 &end,
               const glm::vec2 &controlPoint1,
               const glm::vec2 &controlPoint2,
               float weight,
               uint64_t id) {
                renderer.pathCubicTo(
                    controlPoint1,
                    controlPoint2,
                    {end.x, end.y},
                    PathCommandStroke::withWidthAndId(weight, toPickingId(id)));
            },
            py::arg("end"),
            py::arg("control_point_1"),
            py::arg("control_point_2"),
            py::arg("weight"),
            py::arg("id"))
        .def("end_path", &IRenderer2D::endPath);
}
