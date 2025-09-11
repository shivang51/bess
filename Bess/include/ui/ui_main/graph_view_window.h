#pragma once

#include "common/helpers.h"
#include "ui/icons/CodIcons.h"

namespace Bess::UI {

    struct GraphViewWindowData {
        bool isWindowShown;
    };

    class GraphViewWindow {
      public:
        static void hide();
        static void show();
        static void draw();

        static bool isShown();

        static constexpr auto windowName = Common::Helpers::concat(Icons::CodIcons::GRAPH_LINE, "  Graph View");

        static GraphViewWindowData &getDataRef();

      private:
        static void plotDigitalSignals();

      private:
        static GraphViewWindowData s_data;
    };
} // namespace Bess::UI
