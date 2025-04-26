#pragma once

#include "ui/icons/MaterialIcons.h"
#include <cstring>
#include <string>

namespace Bess::UI {
    class ProjectExplorer {
      public:
        static std::string windowName;
        static void draw();

      private:
        static void firstTime();
        static bool isfirstTimeDraw;
    };
} // namespace Bess::UI
