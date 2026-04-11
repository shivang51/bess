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
    class SimulationSceneComponent;
} // namespace Bess::Canvas

namespace Bess::Plugins {
    class __attribute__((visibility("default"))) PluginHandle {
      public:
        PluginHandle() = default;
        PluginHandle(const pybind11::object &pluginObj);
        ~PluginHandle() = default;

        const pybind11::object &getPluginObject() const;

        std::vector<std::shared_ptr<SimEngine::ComponentDefinition>> onComponentsRegLoad() const;

        void cleanup();

        void drawUI();

        bool hasSimComponent(const uint64_t &baseHash) const;

        std::shared_ptr<Canvas::SimulationSceneComponent> getSimComponent(
            const std::shared_ptr<SimEngine::ComponentDefinition> &def) const;

      private:
        pybind11::object m_pluginObj;
    };
} // namespace Bess::Plugins
