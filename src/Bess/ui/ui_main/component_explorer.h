#pragma once

#include "component_definition.h"
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

        static void createComponent(const std::shared_ptr<SimEngine::ComponentDefinition> &def,
                                    const glm::vec2 &pos = {0.f, 0.f});
        static void createComponent(std::type_index tIdx, const glm::vec2 &pos = {0.f, 0.f});

      private:
        static std::string m_searchQuery;
        static void firstTime();
        static bool m_isfirstTimeDraw;
    };
} // namespace Bess::UI
