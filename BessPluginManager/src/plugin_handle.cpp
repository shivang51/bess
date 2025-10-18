#include "plugin_handle.h"

#include "component_definition.h"
#include "types.h"

namespace Bess::Plugins {
    PluginHandle::PluginHandle(const pybind11::object &pluginObj)
        : m_pluginObj(pluginObj) {}

    std::vector<SimEngine::ComponentDefinition> PluginHandle::onComponentsRegLoad() const {
        std::vector<SimEngine::ComponentDefinition> components;

        if (pybind11::hasattr(m_pluginObj, "on_components_reg_load")) {
            pybind11::object compList = m_pluginObj.attr("on_components_reg_load")();
            for (auto item : compList) {
                auto comp = item.attr("_native").cast<SimEngine::ComponentDefinition>();
                components.push_back(comp);
            }
        }

        return components;
    }

    const pybind11::object &PluginHandle::getPluginObject() const {
        return m_pluginObj;
    }
} // namespace Bess::Plugins
