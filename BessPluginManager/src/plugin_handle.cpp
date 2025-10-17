#include "plugin_handle.h"

namespace Bess::Plugins {
    PluginHandle::PluginHandle(const pybind11::object &pluginObj)
        : m_pluginObj(pluginObj) {}

    std::vector<Component> PluginHandle::onComponentsRegLoad() const {
        std::vector<Component> components;

        if (pybind11::hasattr(m_pluginObj, "on_components_reg_load")) {
            pybind11::object compList = m_pluginObj.attr("on_components_reg_load")();
            for (auto item : compList) {
                Component comp;
                comp.name = item.attr("name").cast<std::string>();
                comp.type = item.attr("type").cast<int>();

                comp.onUpdateFn = item.attr("update");
                components.push_back(comp);
            }
        }

        return components;
    }

    const pybind11::object &PluginHandle::getPluginObject() const {
        return m_pluginObj;
    }
} // namespace Bess::Plugins
