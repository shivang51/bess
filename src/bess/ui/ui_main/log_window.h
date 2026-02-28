#pragma once

#include "ui_panel.h"
namespace Bess::UI {
    class LogWindow : public Panel {
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
