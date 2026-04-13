#pragma once

#include "common/class_helpers.h"
#include "json/value.h"
#include <pybind11/pybind11.h>
#include <string>

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

        bool hasSceneComp(const std::string &typeName);

        bool canDerserialize(const std::string &typeName);

        std::shared_ptr<Canvas::SceneComponent> derserialize(
            const std::string &typeName,
            const Json::Value &json);

        std::shared_ptr<Canvas::SimulationSceneComponent> getSimComponent(
            const std::shared_ptr<SimEngine::ComponentDefinition> &def) const;

        MAKE_GETTER(std::string, Name, m_pluginName)
        MAKE_GETTER(std::string, Version, m_pluginVersion)

      private:
        pybind11::object m_pluginObj;
        std::string m_pluginName;
        std::string m_pluginVersion;

        std::unordered_map<std::string, bool> m_availableCompCache;
        std::unordered_map<std::string, std::string> m_typeNameModule;
    };
} // namespace Bess::Plugins
