#pragma once

#include "drivers/sim_driver.h"
#include "ui_panel.h"

#include <string>
#include <typeindex>

namespace Bess::UI {

    class ComponentExplorer : public Panel {
      public:
        ComponentExplorer();
        void onDraw() override;

        void onBeforeDraw() override;

        static UUID createComponent(const std::shared_ptr<SimEngine::Drivers::ComponentDef> &def,
                                    const glm::vec2 &pos = {0.f, 0.f});
        static UUID createComponent(std::type_index tIdx, const glm::vec2 &pos = {0.f, 0.f});

      private:
        std::string m_searchQuery;
    };
} // namespace Bess::UI
