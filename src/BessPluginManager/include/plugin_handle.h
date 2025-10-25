#pragma once

#include <pybind11/pybind11.h>

namespace Bess::SimEngine {
    class ComponentDefinition;
}

namespace Bess::Renderer {
    class Path;
}

namespace Bess::Plugins {
    class __attribute__((visibility("default"))) PluginHandle {
      public:
        PluginHandle() = default;
        PluginHandle(const pybind11::object &pluginObj);
        ~PluginHandle() = default;

        const pybind11::object &getPluginObject() const;

        std::vector<SimEngine::ComponentDefinition> onComponentsRegLoad() const;
        std::unordered_map<uint64_t, Renderer::Path> onSchematicSymbolsLoad() const;

      private:
        pybind11::object m_pluginObj;
    };
} // namespace Bess::Plugins
