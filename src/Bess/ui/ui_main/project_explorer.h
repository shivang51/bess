#pragma once

#include "common/helpers.h"
#include "ui/icons/CodIcons.h"
#include <string>

namespace Bess::UI {
    class ProjectExplorer {
      public:
        static constexpr auto windowName = Common::Helpers::concat(Icons::CodIcons::SYMBOL_CLASS, "  Project Explorer");
        static void draw();

        static bool isShown;

      private:
        static void firstTime();
        static bool isfirstTimeDraw;
    };
} // namespace Bess::UI
