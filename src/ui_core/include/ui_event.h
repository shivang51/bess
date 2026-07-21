#pragma once

#include "bess_core/input/input_event.h"

#include <concepts>
#include <cstdint>
#include <type_traits>
#include <utility>
#include <variant>

namespace Bess::UI {

    struct UITargetResizeEvent {
        uint32_t width = 0;
        uint32_t height = 0;
    };

    // These events are synthesized by WidgetTree. They use the same routing
    // machinery as platform input without leaking widget state into the
    // platform-facing input module.
    struct UIPointerCrossingEvent {
        bool entered = false;
    };

    struct UIFocusChangedEvent {
        bool focused = false;
    };

    using UIEventData = std::variant<UITargetResizeEvent,
                                     UIPointerCrossingEvent,
                                     UIFocusChangedEvent,
                                     Input::MouseMoveEvent,
                                     Input::MouseWheelEvent,
                                     Input::MouseButtonEvent,
                                     Input::KeyEvent,
                                     Input::TextInputEvent>;

    struct UIEvent {
        UIEventData data;
        Input::Modifiers modifiers;

        template <typename T>
            requires(!std::same_as<std::remove_cvref_t<T>, UIEvent> &&
                     std::constructible_from<UIEventData, T>)
        UIEvent(T &&event, Input::Modifiers eventModifiers = {})
            : data(std::forward<T>(event)),
              modifiers(eventModifiers) {
        }

        template <typename T> [[nodiscard]] bool is() const noexcept {
            return std::holds_alternative<T>(data);
        }

        template <typename T> [[nodiscard]] const T *getIf() const noexcept {
            return std::get_if<T>(&data);
        }

        template <typename T> [[nodiscard]] T *getIf() noexcept {
            return std::get_if<T>(&data);
        }
    };

} // namespace Bess::UI
