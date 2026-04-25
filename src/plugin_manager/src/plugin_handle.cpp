#include "plugin_handle.h"
#include "application/pages/main_page/scene_components/sim_scene_component.h"
#include <memory>
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

namespace py = pybind11;

namespace Bess::Plugins {

    PluginHandle::PluginHandle(const pybind11::object &pluginObj)
        : m_pluginObj(pluginObj) {

        if (py::hasattr(pluginObj, "name")) {
            m_pluginName = pluginObj.attr("name").cast<std::string>();
        } else {
            BESS_WARN("Plugin object does not have 'name' attribute, using default name");
            m_pluginName = "Unknown";
        }

        if (py::hasattr(pluginObj, "version")) {
            m_pluginVersion = pluginObj.attr("version").cast<std::string>();
        } else {
            BESS_WARN("Plugin object does not have 'version' attribute, using default version");
            m_pluginVersion = "Unknown";
        }
    }

    std::vector<std::shared_ptr<SimEngine::Drivers::CompDef>> PluginHandle::onComponentsRegLoad() const {
        std::vector<std::shared_ptr<SimEngine::Drivers::CompDef>> components;

        py::gil_scoped_acquire gil;
        if (py::hasattr(m_pluginObj, "on_components_reg_load")) {
            py::object compList = m_pluginObj.attr("on_components_reg_load")();

            for (py::handle item : compList) {
                py::object pyComp = py::reinterpret_borrow<py::object>(item);
                auto d = item.cast<std::shared_ptr<SimEngine::Drivers::CompDef>>();
                components.emplace_back(std::move(d));
            }
        }

        return components;
    }

    const pybind11::object &PluginHandle::getPluginObject() const {
        return m_pluginObj;
    }

    void PluginHandle::cleanup() {
        py::gil_scoped_acquire gil;
        if (py::hasattr(m_pluginObj, "cleanup")) {
            m_pluginObj.attr("cleanup")();
        }
    }

    void PluginHandle::drawUI() {
        py::gil_scoped_acquire gil;
        if (py::hasattr(m_pluginObj, "draw_ui")) {
            m_pluginObj.attr("draw_ui")();
        }
    }

    std::shared_ptr<Canvas::SimulationSceneComponent> PluginHandle::getSimComponent(
        const std::shared_ptr<SimEngine::Drivers::CompDef> &def) const {
        py::gil_scoped_acquire gil;
        if (py::hasattr(m_pluginObj, "get_sim_comp")) {
            py::object result = m_pluginObj.attr("get_sim_comp")(def);
            if (!result.is_none()) {
                result.attr("setup")(def);
                return result.cast<std::shared_ptr<Canvas::SimulationSceneComponent>>();
            }
        }
        return nullptr;
    }

    bool PluginHandle::hasSimComponent(const uint64_t &baseHash) const {
        py::gil_scoped_acquire gil;
        if (py::hasattr(m_pluginObj, "has_sim_comp")) {
            return m_pluginObj.attr("has_sim_comp")(baseHash).cast<bool>();
        }
        return false;
    }

    bool PluginHandle::hasSceneComp(const std::string &typeName) {
        if (m_availableCompCache.contains(typeName)) {
            return m_availableCompCache.at(typeName);
        }

        py::gil_scoped_acquire gil;

        py::module_ sys = py::module_::import("sys");

        auto modules = sys.attr("modules").attr(
                                              "keys")()
                           .cast<std::vector<std::string>>();

        bool hasSceneComp = false;

        for (const auto &modName : modules) {
            py::module_ mod = py::module_::import(modName.c_str());
            if (py::hasattr(mod, typeName.c_str())) {
                hasSceneComp = true;
                m_typeNameModule[typeName] = modName;
                break;
            }
        }

        m_availableCompCache[typeName] = hasSceneComp;
        return hasSceneComp;
    }

    bool PluginHandle::canDerserialize(const std::string &typeName) {
        if (!hasSceneComp(typeName)) {
            return false;
        }

        py::gil_scoped_acquire gil;

        const auto &modName = m_typeNameModule.at(typeName);
        py::module_ mod = py::module_::import(modName.c_str());
        py::object compClass = mod.attr(typeName.c_str());

        return py::hasattr(compClass, "from_json");
    }

    std::shared_ptr<Canvas::SceneComponent> PluginHandle::derserialize(const std::string &typeName,
                                                                       const Json::Value &json) {
        if (!canDerserialize(typeName)) {
            return nullptr;
        }

        py::gil_scoped_acquire gil;

        const auto &modName = m_typeNameModule.at(typeName);
        py::module_ mod = py::module_::import(modName.c_str());
        py::object compClass = mod.attr(typeName.c_str());

        py::object compObj = compClass.attr("from_json")(json);
        return compObj.cast<std::shared_ptr<Canvas::SceneComponent>>();
    }
} // namespace Bess::Plugins
