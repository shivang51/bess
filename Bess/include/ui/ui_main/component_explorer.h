#pragma once

#include "ui/icons/MaterialIcons.h"
#include <cstring>
#include <string>

namespace Bess::UI {
    class ComponentExplorer {
      public:
        static std::string windowName;
        static void draw();

      private:
        static std::string m_searchQuery;
        static void firstTime();
        static bool isfirstTimeDraw;
    };
} // namespace Bess::UI
