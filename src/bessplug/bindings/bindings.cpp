#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

namespace py = pybind11;

void bind_common_bindings(py::module_ &m);
void bind_renderer_path(py::module_ &m);
void bind_sim_engine_types(py::module_ &m);
void bind_sim_functions(py::module_ &m);
void bind_sim_engine_component_definition(py::module_ &m);
void bind_scene_schematic_diagram(py::module_ &m);
void bind_scene_component(py::module_ &m);
void bind_sim_comp_draw_hook(py::module_ &m);
void bind_scene_state(py::module_ &m);
void bind_path_renderer(py::module_ &m);
void bind_material_renderer(py::module_ &m);
void bind_scene_common_binding(py::module_ &m);
void bind_asset_manager(py::module_ &m);

PYBIND11_MODULE(_bindings, m) {
    m.doc() = "BESS Python bindings";

    auto common = m.def_submodule("common", "Common bindings");
    auto simEngine = m.def_submodule("sim_engine", "Simulation engine bindings");
    auto simFn = simEngine.def_submodule("sim_functions", "Simulation engine prebuilt simulation functions");
    auto scene = m.def_submodule("scene", "Scene bindings");
    auto renderer = scene.def_submodule("renderer", "Scene Renderer bindings");
    auto assetMgr = m.def_submodule("asset_manager", "Asset Manager bindings");

    // Common
    bind_common_bindings(common);

    // Sim Engine
    bind_sim_engine_types(simEngine);
    bind_sim_engine_component_definition(simEngine);
    bind_sim_functions(simFn);

    // Scene
    bind_scene_state(scene);
    bind_scene_common_binding(scene);
    bind_renderer_path(renderer);
    bind_scene_schematic_diagram(scene);
    bind_path_renderer(renderer);
    bind_material_renderer(renderer);
    bind_scene_component(scene);
    bind_sim_comp_draw_hook(scene);

    // Asset Manager
    bind_asset_manager(assetMgr);
}
