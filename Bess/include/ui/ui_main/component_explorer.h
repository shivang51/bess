#pragma once

#include "scene/components/non_sim_comp.h"
#include "ui/icons/MaterialIcons.h"
#include "simulation_engine.h"
#include "component_catalog.h"
#include "component_definition.h"

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
        static void createComponent(const SimEngine::ComponentDefinition &def, int inputCount, int outputCount);
        static void createComponent(const Canvas::Components::NSComponent &comp);
        static ModifiablePropertiesStr generateModifiablePropertiesStr();

      private:
        static std::string m_searchQuery;
        static void firstTime();
        static bool isfirstTimeDraw;
    };
} // namespace Bess::UI
