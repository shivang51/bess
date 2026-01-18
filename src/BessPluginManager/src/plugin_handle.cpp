#include "plugin_handle.h"
#include "component_definition.h"
#include "scene/scene_state/components/scene_component.h"
#include "scene/scene_state/components/sim_scene_comp_draw_hook.h"
#include "scene/scene_state/components/sim_scene_component.h"
#include "scene/schematic_diagram.h"
#include "spdlog/spdlog.h"
#include <memory>
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

namespace py = pybind11;

namespace Bess::Plugins {

    PluginHandle::PluginHandle(const pybind11::object &pluginObj)
        : m_pluginObj(pluginObj) {}

    std::vector<std::shared_ptr<SimEngine::ComponentDefinition>> PluginHandle::onComponentsRegLoad() const {
        std::vector<std::shared_ptr<SimEngine::ComponentDefinition>> components;

        py::gil_scoped_acquire gil;
        if (py::hasattr(m_pluginObj, "on_components_reg_load")) {
            py::object compList = m_pluginObj.attr("on_components_reg_load")();

            for (py::handle item : compList) {
                py::object pyComp = py::reinterpret_borrow<py::object>(item);
                auto d = item.cast<std::shared_ptr<SimEngine::ComponentDefinition>>();
                components.emplace_back(std::move(d));
            }
        }

        py::gil_scoped_release release;
        return components;
    }

    void PluginHandle::onSceneComponentsLoad(std::unordered_map<uint64_t, std::shared_ptr<Canvas::SimSceneCompDrawHook>> &reg) {
        py::gil_scoped_acquire gil;
        if (py::hasattr(m_pluginObj, "on_scene_comp_load")) {
            py::object compDict = m_pluginObj.attr("on_scene_comp_load")();

            for (auto item : compDict.cast<py::dict>()) {
                uint64_t key = item.first.cast<uint64_t>();
                auto hook = item.second.cast<std::shared_ptr<Canvas::SimSceneCompDrawHook>>();
                reg.emplace(key, std::move(hook));
            }
        }

        py::gil_scoped_release release;
    }

    const pybind11::object &PluginHandle::getPluginObject() const {
        return m_pluginObj;
    }
} // namespace Bess::Plugins
