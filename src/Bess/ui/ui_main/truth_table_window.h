#pragma once

#include "bess_uuid.h"
#include "types.h"
#include <string>
namespace Bess::UI {
    class TruthTableWindow {
      public:
        static constexpr const char *windowName = "Truth Table Viewer";

        static void draw();

        static bool isShown;

      private:
        static void firstTime();
        static bool isfirstTimeDraw;
        static std::string selectedNetName;
        static UUID selectedNetId;
        static bool isDirty;
        static std::vector<std::vector<SimEngine::LogicState>> currentTruthTable;
    };
} // namespace Bess::UI
