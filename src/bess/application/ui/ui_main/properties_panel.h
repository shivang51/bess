#pragma once

#include "ui/ui_panel.h"

namespace Bess::UI {
    class PropertiesPanel : public Panel {
      public:
        PropertiesPanel();
        void onDraw() override;
    };
} // namespace Bess::UI
