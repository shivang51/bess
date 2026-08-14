#pragma once

#include "common/bess_api.h"
#include "ui/ui_panel.h"

#include <string>

namespace Bess::UI {
    class BESS_API ProjectSettingsWindow : public Panel {
      public:
        ProjectSettingsWindow();

        void onBeforeDraw() override;
        void onDraw() override;
    };
} // namespace Bess::UI
