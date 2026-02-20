#pragma once

#include "common/bess_uuid.h"
#include "common/helpers.h"
#include "types.h"
#include "ui/icons/FontAwesomeIcons.h"
#include <string>

namespace Bess::UI {
    class TruthTableWindow {
      public:
        static constexpr auto windowName = Common::Helpers::concat(Icons::FontAwesomeIcons::FA_TABLE,
                                                                   "  Truth Table Viewer");

        static void draw();

        static bool isShown;

      private:
        static void firstTime();
        static bool isfirstTimeDraw;
        static std::string *selectedNetName;
        static UUID selectedNetId;
        static bool isDirty;
        static SimEngine::TruthTable currentTruthTable;
        static std::unordered_map<UUID, std::string> compIdToNameMap;
    };
} // namespace Bess::UI
