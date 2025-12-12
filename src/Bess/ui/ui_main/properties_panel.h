#pragma once

#include "common/helpers.h"
#include "ui/icons/CodIcons.h"

namespace Bess::UI {
    class PropertiesPanel {
      public:
        static constexpr auto windowName = Common::Helpers::concat(Icons::CodIcons::SYMBOL_PROPERTY,
                                                                   "  Properties");
        static void draw();

        static bool isShown;
    };
} // namespace Bess::UI
