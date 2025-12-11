#pragma once

namespace Bess::UI {
    class TruthTableWindow {
      public:
        static constexpr const char *windowName = "Truth Table Viewer";

        static void draw();

        static bool isShown;

      private:
        static void firstTime();
        static bool isfirstTimeDraw;
    };
} // namespace Bess::UI
