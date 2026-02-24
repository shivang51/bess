#pragma once

#include "ui_panel.h"

namespace Bess::UI {
    class PropertiesPanel : public Panel {
      public:
        PropertiesPanel();
        void onDraw() override;
    };
} // namespace Bess::UI
