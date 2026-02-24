#pragma once

#include "ui_panel.h"

namespace Bess::UI {
    class DebugPanel : public Panel {
      public:
        DebugPanel();
        void onDraw() override;
    };
} // namespace Bess::UI
