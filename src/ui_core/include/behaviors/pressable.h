#pragma once

#include "common/bess_api.h"
#include "widget.h"

namespace Bess::UI {

    struct PressableResult {
        UIEventReply reply;
        bool activated = false;
        bool stateChanged = false;
    };

    // Reusable pointer/keyboard activation state machine. It deliberately has
    // no drawing or callback ownership, so Button, tabs, menu items, tree rows,
    // and future controls can share identical interaction semantics.
    class BESS_API Pressable {
      public:
        [[nodiscard]] PressableResult handle(WidgetEventContext &context,
                                             const UIEvent &event);
        void reset() noexcept;

        [[nodiscard]] bool isHovered() const noexcept;
        [[nodiscard]] bool isPressed() const noexcept;

      private:
        bool m_hovered = false;
        bool m_pointerPressed = false;
        bool m_keyboardPressed = false;
    };

} // namespace Bess::UI
