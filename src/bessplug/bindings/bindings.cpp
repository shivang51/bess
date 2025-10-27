#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

namespace py = pybind11;

void bind_common_bindings(py::module_ &m);
void bind_renderer_path(py::module_ &m);
void bind_sim_engine_types(py::module_ &m);
void bind_sim_functions(py::module_ &m);
void bind_sim_engine_component_definition(py::module_ &m);
void bind_scene_schematic_diagram(py::module_ &m);

PYBIND11_MODULE(_bindings, m) {
    m.doc() = "BESS Python bindings";

    auto common = m.def_submodule("common", "Common bindings");
    auto simEngine = m.def_submodule("sim_engine", "Simulation engine bindings");
    auto simFn = simEngine.def_submodule("sim_functions", "Simulation engine prebuilt simulation functions");
    auto renderer = m.def_submodule("renderer", "Renderer bindings");
    auto scene = m.def_submodule("scene", "Scene bindings");

    // Common
    bind_common_bindings(m);

    // Sim Engine
    bind_sim_functions(simFn);
    bind_sim_engine_types(simEngine);
    bind_sim_engine_component_definition(simEngine);

    // Scene
    bind_scene_schematic_diagram(scene);

    // Renderer
    bind_renderer_path(renderer);
}
