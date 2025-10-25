#pragma once

#include <string>

namespace Bess::UI {
    class Popups {
    public:
        class PopupIds {
        public:
            static const std::string unsavedProjectWarning;
        };

        enum class PopupRes {
            none = -1,
            yes,
            no,
            cancel
        };

        static PopupRes handleUnsavedProjectWarning();
    };
}