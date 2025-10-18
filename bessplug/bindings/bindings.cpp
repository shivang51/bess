#include "types.h"

#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

namespace py = pybind11;

void bind_component_state(py::module_ &m) {
    using namespace Bess::SimEngine;

    py::class_<ComponentState>(m, "ComponentState")
        .def(py::init<>())
        .def_readwrite("input_states", &ComponentState::inputStates)
        .def_readwrite("output_states", &ComponentState::outputStates)
        .def_readwrite("is_changed", &ComponentState::isChanged)
        .def_property("input_connected", [](const ComponentState &self) { return self.inputConnected; }, [](ComponentState &self, const std::vector<bool> &v) { self.inputConnected = v; })
        .def_property("output_connected", [](const ComponentState &self) { return self.outputConnected; }, [](ComponentState &self, const std::vector<bool> &v) { self.outputConnected = v; })
        .def("__repr__", [](const ComponentState &self) { return "<ComponentState is_changed=" + std::string(self.isChanged ? "True" : "False") + ">"; });
}

PYBIND11_MODULE(_bindings, m) {
    m.doc() = "BESS Python bindings";

    // Create submodules
    auto sim_engine = m.def_submodule("sim_engine", "Simulation engine bindings");
    bind_component_state(sim_engine);
}
