#pragma once

#include "component_catalog.h"
#include "component_definition.h"
#include "scene/components/non_sim_comp.h"
#include "simulation_engine.h"
#include "ui/icons/MaterialIcons.h"

#include <cstring>
#include <string>

namespace Bess::UI {
    class ComponentExplorer {
      public:
        static std::string windowName;
        static void draw();

      private:
        typedef std::unordered_map<SimEngine::ComponentType, std::vector<std::pair<std::string, std::pair<SimEngine::Properties::ComponentProperty, std::any>>>> ModifiablePropertiesStr;

      private:
        static void createComponent(const std::shared_ptr<const SimEngine::ComponentDefinition> def, int inputCount, int outputCount);
        static void createComponent(const Canvas::Components::NSComponent &comp);
        static ModifiablePropertiesStr generateModifiablePropertiesStr();

      private:
        static std::string m_searchQuery;
        static void firstTime();
        static bool m_isfirstTimeDraw;
    };
} // namespace Bess::UI
