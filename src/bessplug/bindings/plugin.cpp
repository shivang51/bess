#include "pages/main_page/scene_components/sim_scene_component.h"
#include <map>
#include <memory>
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

namespace Bess::SimEngine {
    class ComponentDefinition;
}

namespace py = pybind11;

class Plugin {
  public:
    using CompDefPtr = std::shared_ptr<Bess::SimEngine::Drivers::CompDef>;
    using CompVec = std::vector<CompDefPtr>;
    using SceneMap = std::map<int, py::object>;
    using SceneCompPtr = std::shared_ptr<Bess::Canvas::SimulationSceneComponent>;

    Plugin() = default;

    CompVec on_comp_catalog_load() {
        PYBIND11_OVERRIDE_PURE(CompVec, Plugin, on_comp_catalog_load);
    }

    bool has_sim_scene_comp(const std::string &name) const {
        return false;
    }

    std::optional<SceneCompPtr> get_sim_scene_comp(const CompDefPtr &compDef) const {
        return std::nullopt;
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
        .def("on_comp_catalog_load", &Plugin::on_comp_catalog_load)
        .def("cleanup", [](Plugin &self) {
            py::print(std::format("Cleaning plugin: {} v{}",
                                  self.get_name(), self.get_version()));
        })
        .def("draw_ui", &Plugin::draw_ui)
        .def("has_sim_scene_comp", &Plugin::has_sim_scene_comp, py::arg("def_name"))
        .def("get_sim_scene_comp", &Plugin::get_sim_scene_comp, py::arg("comp_def"))
        .def_property("name", &Plugin::get_name, &Plugin::set_name)
        .def_property("version", &Plugin::get_version, &Plugin::set_version);
}
