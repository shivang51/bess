#pragma once

/// Responsible for managing plugins, including loading, unloading, and providing access to plugin functionalities.
/// Simulation DOES NOT use this, it being an independent module handles plugins on its own.

#include "pages/main_page/scene_components/sim_scene_component.h"
#include "settings/settings.h"
namespace Bess::Svc {

    class PluginService {
      public:
        static PluginService &getInstance();

        void init();
        void destroy();

        bool hasSimComponent(const uint64_t &compHash) const;
        bool hasSceneComp(const std::string &typeName) const;
        bool canDerserialize(const std::string &typeName) const;

        std::shared_ptr<Canvas::SimulationSceneComponent> getSimComp(
            const std::shared_ptr<SimEngine::ComponentDefinition> &def) const;

        MAKE_GETTER(bool, IsInitialized, m_initialized)

      private:
        PluginService() = default;
        ~PluginService() = default;

      private:
        bool m_initialized = false;
    };
} // namespace Bess::Svc
