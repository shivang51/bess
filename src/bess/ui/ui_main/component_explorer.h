#pragma once

#include "component_definition.h"

#include "ui_panel.h"

#include <string>
#include <typeindex>

namespace Bess::UI {

    class ComponentExplorer : public Panel {
      public:
        ComponentExplorer();
        void onDraw() override;

        void onBeforeDraw() override;

      private:
        void createComponent(const std::shared_ptr<SimEngine::ComponentDefinition> &def,
                             const glm::vec2 &pos = {0.f, 0.f});
        void createComponent(std::type_index tIdx, const glm::vec2 &pos = {0.f, 0.f});

      private:
        std::string m_searchQuery;
    };
} // namespace Bess::UI
