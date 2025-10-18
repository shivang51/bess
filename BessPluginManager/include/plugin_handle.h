#pragma once

#include <pybind11/pybind11.h>

namespace Bess::SimEngine {
    class ComponentDefinition;
}

namespace Bess::Plugins {
    class PluginHandle {
      public:
        PluginHandle() = default;
        PluginHandle(const pybind11::object &pluginObj);
        ~PluginHandle() = default;

        const pybind11::object &getPluginObject() const;

        std::vector<SimEngine::ComponentDefinition> onComponentsRegLoad() const;

      private:
        pybind11::object m_pluginObj;
    };
} // namespace Bess::Plugins
