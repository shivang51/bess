#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

namespace py = pybind11;

void bind_common_bindings(py::module_ &m);
void bind_renderer_path(py::module_ &m);
void bind_sim_engine_types(py::module_ &m);
void bind_sim_engine_component_definition(py::module_ &m);

PYBIND11_MODULE(_bindings, m) {
    m.doc() = "BESS Python bindings";

    auto common = m.def_submodule("common", "Common bindings");
    auto simEngine = m.def_submodule("sim_engine", "Simulation engine bindings");
    auto renderer = m.def_submodule("renderer", "Renderer bindings");

    // Common
    bind_common_bindings(m);

    // Sim Engine
    bind_sim_engine_types(simEngine);
    bind_sim_engine_component_definition(simEngine);

    // Renderer
    bind_renderer_path(renderer);
}
