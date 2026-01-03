#include "plugin_handle.h"
#include "component_definition.h"
#include "scene/scene_state/components/scene_component.h"
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
                auto d = pyComp.cast<std::shared_ptr<SimEngine::ComponentDefinition>>();
                components.emplace_back(std::move(d));
            }
        }

        py::gil_scoped_release release;
        return components;
    }

    std::unordered_map<uint64_t, Canvas::SchematicDiagram> PluginHandle::onSchematicSymbolsLoad() const {
        std::unordered_map<uint64_t, Canvas::SchematicDiagram> symbols;

        py::gil_scoped_acquire gil;
        if (py::hasattr(m_pluginObj, "on_schematic_symbols_load")) {
            py::object symDict = m_pluginObj.attr("on_schematic_symbols_load")();

            for (auto item : symDict.cast<py::dict>()) {
                uint64_t key = item.first.cast<uint64_t>();
                auto symbol = item.second.attr("_native").cast<Canvas::SchematicDiagram>();
                symbols.emplace(key, std::move(symbol));
            }
        }

        py::gil_scoped_release release;
        return symbols;
    }

    void PluginHandle::onSceneComponentsLoad(std::unordered_map<uint64_t, pybind11::type> &reg) {
        if (py::hasattr(m_pluginObj, "on_scene_comp_load")) {
            // maping of uint64_t to py::type, a python class
            py::object compDict = m_pluginObj.attr("on_scene_comp_load")();

            for (auto item : compDict.cast<py::dict>()) {
                uint64_t key = item.first.cast<uint64_t>();
                py::type pyCompType = item.second.cast<py::type>();
                reg.emplace(key, std::move(pyCompType));
            }
        }

        py::gil_scoped_release release;
    }

    const pybind11::object &PluginHandle::getPluginObject() const {
        return m_pluginObj;
    }
} // namespace Bess::Plugins
