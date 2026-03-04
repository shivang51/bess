#include "scene/renderer/path.h"

#include <pybind11/functional.h>
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>
#include <pybind11/stl_bind.h>

namespace py = pybind11;

typedef Bess::Renderer::Path Path;

void bind_renderer_path(py::module_ &m) {
    py::class_<Path::MoveTo>(m, "MoveTo")
        .def(py::init<>())
        .def_readwrite("p", &Path::MoveTo::p);

    py::class_<Path::LineTo>(m, "LineTo")
        .def(py::init<>())
        .def_readwrite("p", &Path::LineTo::p);

    py::class_<Path::QuadTo>(m, "QuadTo")
        .def(py::init<>())
        .def_readwrite("c", &Path::QuadTo::c)
        .def_readwrite("p", &Path::QuadTo::p);

    py::class_<Path::CubicTo>(m, "CubicTo")
        .def(py::init<>())
        .def_readwrite("c1", &Path::CubicTo::c1)
        .def_readwrite("c2", &Path::CubicTo::c2)
        .def_readwrite("p", &Path::CubicTo::p);

    py::enum_<Path::PathCommand::Kind>(m, "PathCommandKind")
        .value("Move", Path::PathCommand::Kind::Move)
        .value("Line", Path::PathCommand::Kind::Line)
        .value("Quad", Path::PathCommand::Kind::Quad)
        .value("Cubic", Path::PathCommand::Kind::Cubic)
        .export_values();

    py::class_<Path::PathCommand>(m, "PathCommand")
        .def(py::init<>())
        .def_readwrite("kind", &Path::PathCommand::kind)
        .def_readwrite("z", &Path::PathCommand::z)
        .def_readwrite("weight", &Path::PathCommand::weight)
        .def_readwrite("id", &Path::PathCommand::id)
        .def_property(
            "move",
            [](Path::PathCommand &self) -> Path::MoveTo * {
                if (self.kind != Path::PathCommand::Kind::Move)
                    throw std::runtime_error("Command is not MoveTo");
                return &self.move;
            },
            [](Path::PathCommand &self, const Path::MoveTo &val) {
                self.kind = Path::PathCommand::Kind::Move;
                self.move = val;
            },
            py::return_value_policy::reference_internal)
        .def_property(
            "line",
            [](Path::PathCommand &self) -> Path::LineTo * {
                if (self.kind != Path::PathCommand::Kind::Line)
                    throw std::runtime_error("Command is not LineTo");
                return &self.line;
            },
            [](Path::PathCommand &self, const Path::LineTo &val) {
                self.kind = Path::PathCommand::Kind::Line;
                self.line = val;
            },
            py::return_value_policy::reference_internal)
        .def_property(
            "quad",
            [](Path::PathCommand &self) -> Path::QuadTo * {
                if (self.kind != Path::PathCommand::Kind::Quad)
                    throw std::runtime_error("Command is not QuadTo");
                return &self.quad;
            },
            [](Path::PathCommand &self, const Path::QuadTo &val) {
                self.kind = Path::PathCommand::Kind::Quad;
                self.quad = val;
            },
            py::return_value_policy::reference_internal)
        .def_property(
            "cubic",
            [](Path::PathCommand &self) -> Path::CubicTo * {
                if (self.kind != Path::PathCommand::Kind::Cubic)
                    throw std::runtime_error("Command is not CubicTo");
                return &self.cubic;
            },
            [](Path::PathCommand &self, const Path::CubicTo &val) {
                self.kind = Path::PathCommand::Kind::Cubic;
                self.cubic = val;
            },
            py::return_value_policy::reference_internal)
        .def("__repr__", [](const Path::PathCommand &self) {
            switch (self.kind) {
            case Path::PathCommand::Kind::Move:
                return "<PathCommand Move>";
            case Path::PathCommand::Kind::Line:
                return "<PathCommand Line>";
            case Path::PathCommand::Kind::Quad:
                return "<PathCommand Quad>";
            case Path::PathCommand::Kind::Cubic:
                return "<PathCommand Cubic>";
            default:
                return "<PathCommand Unknown>";
            }
        });

    py::class_<Bess::Renderer::PathProperties>(m, "PathProperties")
        .def(py::init<>())
        .def_readwrite("is_closed", &Bess::Renderer::PathProperties::isClosed)
        .def_readwrite("rounded_joints", &Bess::Renderer::PathProperties::roundedJoints)
        .def_readwrite("render_stroke", &Bess::Renderer::PathProperties::renderStroke)
        .def_readwrite("render_fill", &Bess::Renderer::PathProperties::renderFill)
        .def("__repr__", [](const Bess::Renderer::PathProperties &props) {
            return "<PathProperties is_closed=" + std::to_string(props.isClosed) +
                   " rounded_joints=" + std::to_string(props.roundedJoints) +
                   " render_stroke=" + std::to_string(props.renderStroke) +
                   " render_fill=" + std::to_string(props.renderFill) + ">";
        });

    py::class_<Bess::Renderer2D::Vulkan::PathPoint>(m, "PathPoint");

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

    auto cubic_to_flat = [](Path &self, float c1x, float c1y, float c2x, float c2y, float px, float py) {
        self.cubicTo(glm::vec2(c1x, c1y), glm::vec2(c2x, c2y), glm::vec2(px, py));
        return &self;
    };

    py::class_<Path>(m, "Path")
        .def(py::init<>())
        .def("move_to_vec", &Path::moveTo, py::arg("pos"))
        .def("move_to",
             move_to_flat,
             py::arg("x"),
             py::arg("y"), py::return_value_policy::reference_internal)
        .def("line_to_vec", &Path::lineTo, py::return_value_policy::reference_internal)
        .def("line_to",
             line_to_flat,
             py::arg("x"),
             py::arg("y"), py::return_value_policy::reference_internal)
        .def("quad_to_vec", &Path::quadTo, py::return_value_policy::reference_internal)
        .def("quad_to",
             quad_to_flat,
             py::arg("cx"),
             py::arg("cy"),
             py::arg("px"),
             py::arg("py"), py::return_value_policy::reference_internal)
        .def("cubic_to_vec", &Path::cubicTo, py::return_value_policy::reference_internal)
        .def("cubic_to",
             cubic_to_flat,
             py::arg("c1x"),
             py::arg("c1y"),
             py::arg("c2x"),
             py::arg("c2y"),
             py::arg("px"),
             py::arg("py"), py::return_value_policy::reference_internal)
        .def("add_command", &Path::addCommand, py::return_value_policy::reference_internal)
        .def("get_commands", &Path::getCmds, py::return_value_policy::reference_internal)
        .def("set_commands", &Path::setCommands)
        .def("get_contours", &Path::getContours, py::return_value_policy::reference_internal)
        .def("get_props_ref", &Path::getPropsRef, py::return_value_policy::reference_internal)
        .def("get_props", &Path::getProps)
        .def("set_props", &Path::setProps)
        .def("get_stroke_width", &Path::getStrokeWidth)
        .def("set_stroke_width", &Path::setStrokeWidth)
        .def("set_bounds", &Path::setBounds)
        .def("get_bounds", &Path::getBounds)
        .def("get_lowest_pos", &Path::getLowestPos)
        .def("set_lowest_pos", &Path::setLowestPos)
        .def("copy", &Path::copy)
        .def_static("from_svg_str", &Path::fromSvgString, py::arg("svg_data"), "Create a Path from an SVG path data string.")
        .def_property("properties",
                      &Path::getPropsRef,
                      &Path::setProps,
                      py::return_value_policy::reference_internal)
        .def("scale", &Path::scale, py::arg("factor"), py::arg("override_original") = false)
        .def("normalize", &Path::normalize, py::arg("size"))
        .def_readwrite("uuid", &Path::uuid)
        .def("__repr__", [](const Path &) { return "<Path>"; });
}
