#pragma once

#include <string>

namespace Bess::UI {
    class Popups {
      public:
        class PopupIds {
          public:
            static constexpr const char *unsavedProjectWarning = "Save Current Project";
        };

        enum class PopupRes {
            none = -1,
            yes,
            no,
            cancel
        };

        static PopupRes handleUnsavedProjectWarning();
    };
} // namespace Bess::UI
