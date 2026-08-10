#pragma once

#include "common/bess_api.h"

#include "ui/ui_panel.h"
namespace Bess::UI {
    class BESS_API LogWindow : public Panel {
      public:
        LogWindow();

      private:
        void onDraw() override;
        void drawControls();

        struct Controls {
            bool autoScroll = true;
        } m_controls;
    };
} // namespace Bess::UI
