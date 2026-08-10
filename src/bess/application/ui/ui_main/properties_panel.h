#pragma once

#include "common/bess_api.h"

#include "ui/ui_panel.h"

namespace Bess::UI {
    class BESS_API PropertiesPanel : public Panel {
      public:
        PropertiesPanel();
        void onDraw() override;
    };
} // namespace Bess::UI
