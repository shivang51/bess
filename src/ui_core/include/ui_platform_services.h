#pragma once

#include "common/bess_api.h"
#include "ui_types.h"

#include <memory>
#include <optional>
#include <string>
#include <string_view>

namespace Bess::UI {

    // Per-target bridge for facilities owned by the host platform. Widgets
    // never call GLFW, Win32, Cocoa, X11, or Wayland directly.
    class BESS_API UIPlatformServices {
      public:
        virtual ~UIPlatformServices();

        [[nodiscard]] virtual std::optional<std::string>
        readClipboardText() const;
        virtual bool writeClipboardText(std::string_view text);

        // Native integrations use these hooks to enable their IME and keep
        // candidate windows aligned with the active caret. Bounds are local to
        // the UITarget and expressed in framebuffer pixels.
        virtual void beginTextInput();
        virtual void updateTextInputArea(WidgetBounds caretBounds);
        virtual void endTextInput();
    };

    // Shared inert implementation used by offscreen targets and tests unless
    // a host explicitly supplies platform services.
    [[nodiscard]] BESS_API std::shared_ptr<UIPlatformServices>
    nullUIPlatformServices();

} // namespace Bess::UI
