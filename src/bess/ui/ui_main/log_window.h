#pragma once

#include "common/helpers.h"
#include "ui/icons/CodIcons.h"
namespace Bess::UI {

    class LogWindow {
      public:
        static constexpr auto windowName = Common::Helpers::concat(
            Icons::CodIcons::HISTORY, "  Log Window");

        static bool isShown;
        static void draw();

      private:
        static void drawControls();

        static struct Controls {
            bool autoScroll = true;
        } m_controls;
    };
} // namespace Bess::UI
