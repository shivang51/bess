#pragma once

#include "common/bess_api.h"

#include "ui/ui_panel.h"

namespace Bess::UI {
    class BESS_API DebugPanel : public Panel {
      public:
        DebugPanel();
        void onDraw() override;

      private:
        void drawDependencyGraph(const UUID &compId);
    };
} // namespace Bess::UI
