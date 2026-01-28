#include "scene/scene_state/scene_state.h"
#include <pybind11/pybind11.h>

namespace py = pybind11;

void bind_scene_state(py::module_ &m) {
    py::class_<Bess::Canvas::SceneState>(m, "SceneState")
        .def(py::init<Bess::Canvas::SceneState &>())
        .def("clear", &Bess::Canvas::SceneState::clear);
}
