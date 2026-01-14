#include "bess_uuid.h"
#include "glm.hpp"

#include <format>
#include <pybind11/functional.h>
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>
#include <pybind11/stl_bind.h>

namespace py = pybind11;

typedef Bess::UUID UUID;

void bind_vec2(py::module_ &m);
void bind_bess_uuid(py::module_ &m);

void bind_common_bindings(py::module_ &m) {
    bind_vec2(m);
    bind_bess_uuid(m);
}

void bind_vec2(py::module_ &m) {
    py::class_<glm::vec2>(m, "vec2")
        // Constructors
        .def(py::init<>()) // default (0,0)
        .def(py::init<float, float>(), py::arg("x"), py::arg("y"))
        .def(py::init([](py::sequence seq) {
                 if (py::len(seq) != 2)
                     throw std::runtime_error("vec2 requires a sequence of length 2");
                 return glm::vec2(seq[0].cast<float>(), seq[1].cast<float>());
             }),
             py::arg("seq"))

        // Data members
        .def_readwrite("x", &glm::vec2::x)
        .def_readwrite("y", &glm::vec2::y)

        // Basic arithmetic
        .def(py::self + py::self)
        .def(py::self - py::self)
        .def(py::self * float())
        .def(float() * py::self)
        .def(py::self / float())
        .def(-py::self)

        // Equality and comparison
        .def("__eq__", [](const glm::vec2 &a, const glm::vec2 &b) {
            return a.x == b.x && a.y == b.y;
        })
        .def("__ne__", [](const glm::vec2 &a, const glm::vec2 &b) {
            return !(a.x == b.x && a.y == b.y);
        })

        // Sequence-like access (v[0], v[1])
        .def("__getitem__", [](const glm::vec2 &v, size_t i) {
            if (i >= 2)
                throw py::index_error();
            return i == 0 ? v.x : v.y;
        })
        .def("__setitem__", [](glm::vec2 &v, size_t i, float val) {
            if (i >= 2)
                throw py::index_error();
            (i == 0 ? v.x : v.y) = val;
        })
        .def("__len__", [](const glm::vec2 &) { return 2; })

        // String representations
        .def("__repr__", [](const glm::vec2 &v) {
            return "(" + std::to_string(v.x) + ", " + std::to_string(v.y) + ")";
        })
        .def("__str__", [](const glm::vec2 &v) {
            return "(" + std::to_string(v.x) + ", " + std::to_string(v.y) + ")";
        })

        // Conversions
        .def("to_tuple", [](const glm::vec2 &v) {
            return py::make_tuple(v.x, v.y);
        })

        // Hash support (optional, useful for dicts/sets)
        .def("__hash__", [](const glm::vec2 &v) {
            std::hash<float> hf;
            // Simple but consistent hash
            return hf(v.x) ^ (hf(v.y) << 1);
        });
}

void bind_bess_uuid(py::module_ &m) {
    py::class_<UUID>(m, "UUID")
        .def(py::init<>())
        .def(py::init<uint64_t>(), py::arg("id"))
        .def(py::init<const UUID &>())

        .def("__int__", [](const UUID &self) {
            return static_cast<uint64_t>(self);
        })

        .def("__repr__", [](const UUID &self) {
            return std::format("<UUID {}>", static_cast<uint64_t>(self));
        })

        .def("__eq__", [](const UUID &a, const UUID &b) { return a == b; })
        .def("__ne__", [](const UUID &a, const UUID &b) { return !(a == b); })

        .def_property_readonly("value", [](const UUID &self) {
            return static_cast<uint64_t>(self);
        })

        .def_readonly_static("null", &UUID::null);
}
