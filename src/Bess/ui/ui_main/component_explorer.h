#pragma once

#include "component_definition.h"
#include "scene/components/non_sim_comp.h"
#include "ui/icons/CodIcons.h"

#include "common/helpers.h"

#include <string>

namespace Bess::UI {

    class ComponentExplorer {
      public:
        static constexpr auto windowName = Common::Helpers::concat(Icons::CodIcons::LIST_TREE,
                                                                   "  Component Explorer");
        static void draw();

        static bool isShown;

      private:
        static void createComponent(const std::shared_ptr<const SimEngine::ComponentDefinition> &def, int inputCount, int outputCount);
        static void createComponent(const Canvas::Components::NSComponent &comp);

      private:
        static std::string m_searchQuery;
        static void firstTime();
        static bool m_isfirstTimeDraw;
    };
} // namespace Bess::UI
