#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

namespace py = pybind11;

// Forward declarations for binder functions implemented in submodules
void bind_sim_engine_types(py::module_ &m);
void bind_sim_engine_component_definition(py::module_ &m);

PYBIND11_MODULE(_bindings, m) {
    m.doc() = "BESS Python bindings";

    auto sim_engine = m.def_submodule("sim_engine", "Simulation engine bindings");

    // Types and state containers
    bind_sim_engine_types(sim_engine);

    // Component definition APIs
    bind_sim_engine_component_definition(sim_engine);
}
