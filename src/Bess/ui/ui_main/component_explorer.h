#pragma once

#include "component_definition.h"
#include "scene/components/non_sim_comp.h"
#include "ui/icons/CodIcons.h"

#include "common/helpers.h"

#include <string>
#include <typeindex>

namespace Bess::UI {

    class ComponentExplorer {
      public:
        static constexpr auto windowName = Common::Helpers::concat(Icons::CodIcons::LIST_TREE,
                                                                   "  Component Explorer");
        static void draw();

        static bool isShown;

      private:
        static void createComponent(const std::shared_ptr<SimEngine::ComponentDefinition> &def, int inputCount, int outputCount);
        static void createComponent(std::type_index tIdx);

      private:
        static std::string m_searchQuery;
        static void firstTime();
        static bool m_isfirstTimeDraw;
    };
} // namespace Bess::UI
