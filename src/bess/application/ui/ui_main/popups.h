#pragma once

#include "common/bess_api.h"

#include <cstdint>

namespace Bess::UI {
    class BESS_API Popups {
      public:
        // const char* is easier to use with ImGui than string_view
        struct PopupIds {
            static constexpr const char *unsavedProjectWarning =
                "Save Current Project";
            static constexpr const char *about = "About BESS";
        };

        enum class PopupRes : std::int8_t {
            none = -1,
            yes,
            no,
            cancel,
        };

        static PopupRes handleUnsavedProjectWarning();

        static void showAboutPopup();

        static void centerNextPopup();
    };
} // namespace Bess::UI
