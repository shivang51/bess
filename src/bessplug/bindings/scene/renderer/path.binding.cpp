#include "bess_core/renderer/renderer_path.h"

#include <pybind11/detail/common.h>
#include <pybind11/functional.h>
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>
#include <pybind11/stl_bind.h>

namespace py = pybind11;

typedef Bess::Core::Renderer::Path2D Path;

void bind_renderer_path(py::module_ &m) {

    py::enum_<Bess::Core::Renderer::PathCommandKind>(m, "PathCommandKind")
        .value("Move", Bess::Core::Renderer::PathCommandKind::Move)
        .value("Line", Bess::Core::Renderer::PathCommandKind::Line)
        .value("Quad", Bess::Core::Renderer::PathCommandKind::Quad)
        .value("Cubic", Bess::Core::Renderer::PathCommandKind::Cubic)
        .export_values();

    py::class_<Bess::Core::Renderer::PathCommandStroke>(m, "PathCmdStroke")
        .def(py::init<>())
        .def_readwrite("width", &Bess::Core::Renderer::PathCommandStroke::width)
        .def_readwrite("dash_length",
                       &Bess::Core::Renderer::PathCommandStroke::dashLength)
        .def_readwrite("gap_length",
                       &Bess::Core::Renderer::PathCommandStroke::gapLength)
        .def_readwrite("dash_offset",
                       &Bess::Core::Renderer::PathCommandStroke::dashOffset)
        .def_readwrite("break_before",
                       &Bess::Core::Renderer::PathCommandStroke::breakBefore)
        .def_readwrite("break_after",
                       &Bess::Core::Renderer::PathCommandStroke::breakAfter)
        .def_readwrite("id", &Bess::Core::Renderer::PathCommandStroke::id)
        .def_readwrite("has_id",
                       &Bess::Core::Renderer::PathCommandStroke::hasId)
        .def_static("with_width",
                    &Bess::Core::Renderer::PathCommandStroke::withWidth,
                    py::arg("stroke_width"))
        .def_static("with_picking_id",
                    &Bess::Core::Renderer::PathCommandStroke::withPickingId,
                    py::arg("picking_id"), py::arg("stroke_width") = 0.f)
        .def_static("with_id", &Bess::Core::Renderer::PathCommandStroke::withId,
                    py::arg("picking_id"), py::arg("stroke_width") = 0.f)
        .def_static("with_width_and_id",
                    &Bess::Core::Renderer::PathCommandStroke::withWidthAndId,
                    py::arg("stroke_width"), py::arg("picking_id"))
        .def_static("dashed", &Bess::Core::Renderer::PathCommandStroke::dashed,
                    py::arg("dash"), py::arg("gap"),
                    py::arg("stroke_width") = 0.f, py::arg("offset") = 0.f)
        .def_static("broken", &Bess::Core::Renderer::PathCommandStroke::broken,
                    py::arg("stroke_width") = 0.f)
        .def_static("broken_with_id",
                    &Bess::Core::Renderer::PathCommandStroke::brokenWithId,
                    py::arg("stroke_width") = 0.f, py::arg("picking_id"))
        .def("has_width_override",
             &Bess::Core::Renderer::PathCommandStroke::hasWidthOverride)
        .def("has_id_override",
             &Bess::Core::Renderer::PathCommandStroke::hasIdOverride)
        .def("is_dashed", &Bess::Core::Renderer::PathCommandStroke::isDashed)
        .def("is_styled", &Bess::Core::Renderer::PathCommandStroke::isStyled);

    py::class_<Bess::Core::Renderer::PathCommand>(m, "PathCommand")
        .def(py::init<>())
        .def_readwrite("kind", &Bess::Core::Renderer::PathCommand::kind)
        .def_readwrite("p", &Bess::Core::Renderer::PathCommand::p)
        .def_readwrite("control", &Bess::Core::Renderer::PathCommand::control)
        .def_readwrite("control2", &Bess::Core::Renderer::PathCommand::control2)
        .def_readwrite("stroke", &Bess::Core::Renderer::PathCommand::stroke)
        .def_static("move_to", &Bess::Core::Renderer::PathCommand::moveTo,
                    py::arg("pos"),
                    "Create a MoveTo command with the given position and "
                    "optional stroke.")
        .def_static("line_to",
                    py::overload_cast<const glm::vec2 &>(
                        &Bess::Core::Renderer::PathCommand::lineTo),
                    py::arg("pos"),
                    "Create a LineTo command with the given position.")
        .def_static(
            "line_to",
            py::overload_cast<const glm::vec2 &,
                              const Bess::Core::Renderer::PathCommandStroke &>(
                &Bess::Core::Renderer::PathCommand::lineTo),
            py::arg("pos"), py::arg("stroke"),
            "Create a LineTo command with the given position and stroke.")
        .def_static(
            "quad_to",
            py::overload_cast<const glm::vec2 &, const glm::vec2 &>(
                &Bess::Core::Renderer::PathCommand::quadTo),
            py::arg("control"), py::arg("pos"),
            "Create a QuadTo command with the given control and position.")
        .def_static("cubic_to",
                    py::overload_cast<const glm::vec2 &, const glm::vec2 &,
                                      const glm::vec2 &>(
                        &Bess::Core::Renderer::PathCommand::cubicTo),
                    py::arg("control1"), py::arg("control2"), py::arg("pos"),
                    "Create a CubicTo command with the given control points "
                    "and position.")
        .def("__repr__", [](const Bess::Core::Renderer::PathCommand &self) {
            switch (self.kind) {
            case Bess::Core::Renderer::PathCommandKind::Move:
                return "<PathCommand Move>";
            case Bess::Core::Renderer::PathCommandKind::Line:
                return "<PathCommand Line>";
            case Bess::Core::Renderer::PathCommandKind::Quad:
                return "<PathCommand Quad>";
            case Bess::Core::Renderer::PathCommandKind::Cubic:
                return "<PathCommand Cubic>";
            default:
                return "<PathCommand Unknown>";
            }
        });

    py::class_<Bess::Core::Renderer::PathBounds>(m, "PathBounds")
        .def(py::init<>())
        .def_readonly("min", &Bess::Core::Renderer::PathBounds::min)
        .def_readonly("max", &Bess::Core::Renderer::PathBounds::max)
        .def_readonly("valid", &Bess::Core::Renderer::PathBounds::valid)
        .def("size", &Bess::Core::Renderer::PathBounds::size)
        .def("__repr__", [](const Bess::Core::Renderer::PathBounds &self) {
            return "<PathBounds min=" + std::to_string(self.min.x) + "," +
                   std::to_string(self.min.y) +
                   " max=" + std::to_string(self.max.x) + "," +
                   std::to_string(self.max.y) +
                   " valid=" + (self.valid ? "True" : "False") + ">";
        });

    auto move_to_flat = [](Path &self, float x, float y) {
        self.moveTo(glm::vec2(x, y));
        return &self;
    };

    auto line_to_flat = [](Path &self, float x, float y) {
        self.lineTo(glm::vec2(x, y));
        return &self;
    };

    auto quad_to_flat = [](Path &self, float cx, float cy, float px, float py) {
        self.quadTo(glm::vec2(cx, cy), glm::vec2(px, py));
        return &self;
    };

    auto cubic_to_flat = [](Path &self, float c1x, float c1y, float c2x,
                            float c2y, float px, float py) {
        self.cubicTo(glm::vec2(c1x, c1y), glm::vec2(c2x, c2y),
                     glm::vec2(px, py));
        return &self;
    };

    py::class_<Path>(m, "Path")
        .def(py::init<>())
        .def("move_to_vec", &Bess::Core::Renderer::Path2D::moveTo,
             py::arg("pos"))
        .def("move_to", move_to_flat, py::arg("x"), py::arg("y"),
             py::return_value_policy::reference_internal)
        .def("line_to_vec",
             py::overload_cast<const glm::vec2 &>(
                 &Bess::Core::Renderer::Path2D::lineTo),
             py::return_value_policy::reference_internal)
        .def("line_to", line_to_flat, py::arg("x"), py::arg("y"),
             py::return_value_policy::reference_internal)
        .def("line_to_stroke_vec",
             py::overload_cast<const glm::vec2 &,
                               const Bess::Core::Renderer::PathCommandStroke &>(
                 &Bess::Core::Renderer::Path2D::lineTo),
             py::return_value_policy::reference_internal)
        .def(
            "line_to_stroke",
            [](Path &self, float x, float y,
               const Bess::Core::Renderer::PathCommandStroke &stroke) {
                self.lineTo(glm::vec2(x, y), stroke);
                return &self;
            },
            py::arg("x"), py::arg("y"), py::arg("stroke"),
            py::return_value_policy::reference_internal)
        .def("quad_to_vec",
             py::overload_cast<const glm::vec2 &, const glm::vec2 &>(
                 &Bess::Core::Renderer::Path2D::quadTo),
             py::return_value_policy::reference_internal)
        .def("quad_to", quad_to_flat, py::arg("cx"), py::arg("cy"),
             py::arg("px"), py::arg("py"),
             py::return_value_policy::reference_internal)
        .def("quad_to_stroke_vec",
             py::overload_cast<const glm::vec2 &, const glm::vec2 &,
                               const Bess::Core::Renderer::PathCommandStroke &>(
                 &Bess::Core::Renderer::Path2D::quadTo),
             py::return_value_policy::reference_internal)
        .def(
            "quad_to_stroke",
            [](Path &self, float cx, float cy, float px, float py,
               const Bess::Core::Renderer::PathCommandStroke &stroke) {
                self.quadTo(glm::vec2(cx, cy), glm::vec2(px, py), stroke);
                return &self;
            },
            py::arg("cx"), py::arg("cy"), py::arg("px"), py::arg("py"),
            py::arg("stroke"), py::return_value_policy::reference_internal)
        .def("cubic_to_vec",
             py::overload_cast<const glm::vec2 &, const glm::vec2 &,
                               const glm::vec2 &>(
                 &Bess::Core::Renderer::Path2D::cubicTo),
             py::return_value_policy::reference_internal)
        .def("cubic_to", cubic_to_flat, py::arg("c1x"), py::arg("c1y"),
             py::arg("c2x"), py::arg("c2y"), py::arg("px"), py::arg("py"),
             py::return_value_policy::reference_internal)
        .def("cubic_to_stroke_vec",
             py::overload_cast<const glm::vec2 &, const glm::vec2 &,
                               const glm::vec2 &,
                               const Bess::Core::Renderer::PathCommandStroke &>(
                 &Bess::Core::Renderer::Path2D::cubicTo),
             py::return_value_policy::reference_internal)

        .def(
            "cubic_to_stroke",
            [](Path &self, float c1x, float c1y, float c2x, float c2y, float px,
               float py,
               const Bess::Core::Renderer::PathCommandStroke &stroke) {
                self.cubicTo(glm::vec2(c1x, c1y), glm::vec2(c2x, c2y),
                             glm::vec2(px, py), stroke);
                return &self;
            },
            py::arg("c1x"), py::arg("c1y"), py::arg("c2x"), py::arg("c2y"),
            py::arg("px"), py::arg("py"), py::arg("stroke"),
            py::return_value_policy::reference_internal)
        .def("close_path",
             py::overload_cast<>(&Bess::Core::Renderer::Path2D::closePath),
             py::return_value_policy::reference_internal)
        .def("close_path",
             py::overload_cast<const Bess::Core::Renderer::PathCommandStroke &>(
                 &Bess::Core::Renderer::Path2D::closePath),
             py::arg("stroke"), py::return_value_policy::reference_internal)
        .def("close_path",
             py::overload_cast<float>(&Bess::Core::Renderer::Path2D::closePath),
             py::arg("stroke_width"),
             py::return_value_policy::reference_internal)
        .def("close", py::overload_cast<>(&Bess::Core::Renderer::Path2D::close),
             py::return_value_policy::reference_internal)

        .def("close",
             py::overload_cast<float>(&Bess::Core::Renderer::Path2D::close),
             py::arg("stroke_width"),
             py::return_value_policy::reference_internal)
        .def("close",
             py::overload_cast<const Bess::Core::Renderer::PathCommandStroke &>(
                 &Bess::Core::Renderer::Path2D::close),
             py::arg("stroke"), py::return_value_policy::reference_internal)
        .def("close", py::overload_cast<>(&Bess::Core::Renderer::Path2D::close),
             py::return_value_policy::reference_internal)
        .def("bounds", &Bess::Core::Renderer::Path2D::bounds,
             py::return_value_policy::reference_internal)
        .def("command_count", &Bess::Core::Renderer::Path2D::commandCount)
        .def("commands", &Bess::Core::Renderer::Path2D::commands,
             py::return_value_policy::reference_internal)
        .def("is_empty", &Bess::Core::Renderer::Path2D::empty)
        .def("has_bounds", &Bess::Core::Renderer::Path2D::hasBounds)
        .def("revision", &Bess::Core::Renderer::Path2D::revision)
        .def("translate", &Bess::Core::Renderer::Path2D::translate,
             py::arg("pos"))
        .def("set_pos", &Bess::Core::Renderer::Path2D::setPos, py::arg("pos"))
        .def_static("from_svg_str",
                    &Bess::Core::Renderer::Path2D::fromSvgString)
        .def("copy", [](const Path &self) { return Path(self); })
        .def("__repr__", [](const Path &self) {
            return "<Path with " + std::to_string(self.commandCount()) +
                   " commands, bounds=" +
                   (self.hasBounds() ? "Valid" : "Invalid") +
                   ", revision=" + std::to_string(self.revision()) + ">";
        });
}
