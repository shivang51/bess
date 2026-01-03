#pragma once

#include <pybind11/pybind11.h>

namespace Bess::SimEngine {
    class ComponentDefinition;
}

namespace Bess::Renderer {
    class Path;
}

namespace Bess::Canvas {
    class SchematicDiagram;
    class SceneComponent;
} // namespace Bess::Canvas

namespace Bess::Plugins {
    class __attribute__((visibility("default"))) PluginHandle {
      public:
        PluginHandle() = default;
        PluginHandle(const pybind11::object &pluginObj);
        ~PluginHandle() = default;

        const pybind11::object &getPluginObject() const;

        std::vector<std::shared_ptr<SimEngine::ComponentDefinition>> onComponentsRegLoad() const;
        std::unordered_map<uint64_t, Canvas::SchematicDiagram> onSchematicSymbolsLoad() const;
        void onSceneComponentsLoad(std::unordered_map<uint64_t, pybind11::type> &reg);

      private:
        pybind11::object m_pluginObj;
    };
} // namespace Bess::Plugins
