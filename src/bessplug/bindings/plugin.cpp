#include "component_definition.h"
#include "pages/main_page/scene_components/sim_scene_component.h"
#include <map>
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

namespace Bess::SimEngine {
    class ComponentDefinition;
}

namespace py = pybind11;

class Plugin {
  public:
    using CompVec = std::vector<Bess::SimEngine::ComponentDefinition>;
    using SceneMap = std::map<int, py::object>;

    Plugin() = default;

    CompVec on_components_reg_load() {
        PYBIND11_OVERRIDE_PURE(CompVec, Plugin, on_components_reg_load);
    }

    SceneMap on_scene_comp_load() {
        PYBIND11_OVERRIDE_PURE(SceneMap, Plugin, on_scene_comp_load);
    }

    bool has_scene_comp_with_name(const std::string &name) const {
        return false;
    }

    std::shared_ptr<Bess::Canvas::SimulationSceneComponent> get_scene_comp(
        const Bess::SimEngine::ComponentDefinition &compDef) const {
        return nullptr;
    }

    std::string get_name() const {
        return name;
    }

    void set_name(const std::string &new_name) {
        name = new_name;
    }

    std::string get_version() const {
        return version;
    }

    void set_version(const std::string &new_version) {
        version = new_version;
    }

    void draw_ui() {
    }

  private:
    std::string name;
    std::string version;
};

void bind_plugin(py::module &m) {
    py::class_<Plugin>(m, "Plugin")
        .def(py::init<>())
        .def("on_components_reg_load", &Plugin::on_components_reg_load)
        .def("on_scene_comp_load", &Plugin::on_scene_comp_load)
        .def("cleanup", [](Plugin &self) {
            py::print(std::format("Cleaning plugin: {} v{}",
                                  self.get_name(), self.get_version()));
        })
        .def("draw_ui", &Plugin::draw_ui)
        .def("has_scene_comp_with_name", &Plugin::has_scene_comp_with_name, py::arg("name"))
        .def("get_scene_comp", &Plugin::get_scene_comp, py::arg("comp_def"))
        .def_property("name", &Plugin::get_name, &Plugin::set_name)
        .def_property("version", &Plugin::get_version, &Plugin::set_version);
}
