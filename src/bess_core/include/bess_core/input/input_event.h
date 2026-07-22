#pragma once

#include "bess_core/input/input_types.h"
#include "ext/vector_float2.hpp"

#include <chrono>
#include <concepts>
#include <cstddef>
#include <string>
#include <type_traits>
#include <utility>
#include <variant>

namespace Bess::Input {

    struct Modifiers {
        bool control = false;
        bool shift = false;
        bool alt = false;
        bool super = false;
        bool capsLock = false;
        bool numLock = false;

        bool operator==(const Modifiers &) const noexcept = default;
    };

    // Pointer positions are local to the receiving input surface. A boundary
    // that changes surfaces is responsible for transforming the position.
    struct MouseMoveEvent {
        glm::vec2 pos = {0.f, 0.f};
        glm::vec2 delta = {0.f, 0.f};
    };

    struct MouseWheelEvent {
        glm::vec2 pos = {0.f, 0.f};
        glm::vec2 offset = {0.f, 0.f};
    };

    struct MouseButtonEvent {
        MouseButton button = MouseButton::unknown;
        MouseButtonAction action = MouseButtonAction::unknown;
        glm::vec2 pos = {0.f, 0.f};
        std::chrono::steady_clock::time_point timestamp;
    };

    struct KeyEvent {
        KeyCode key = KeyCode::unknown;
        KeyAction action = KeyAction::unknown;
    };

    struct TextInputEvent {
        char32_t codepoint = 0;
    };

    enum class TextCompositionPhase : uint8_t {
        begin,
        update,
        commit,
        cancel,
    };

    // Platform IME pre-edit data. Offsets are UTF-8 byte offsets within
    // `text`, which keeps the platform boundary allocation-free for clients
    // that already store UTF-8 and avoids conflating bytes with graphemes.
    struct TextCompositionEvent {
        TextCompositionPhase phase = TextCompositionPhase::update;
        std::string text;
        size_t selectionStart = 0;
        size_t selectionLength = 0;
    };

    using KeyboardEvent =
        std::variant<KeyEvent, TextInputEvent, TextCompositionEvent>;
    using EventData = std::variant<MouseMoveEvent,
                                   MouseWheelEvent,
                                   MouseButtonEvent,
                                   KeyEvent,
                                   TextInputEvent,
                                   TextCompositionEvent>;

    struct Event {
        EventData data;
        Modifiers modifiers;

        template <typename T>
            requires(!std::same_as<std::remove_cvref_t<T>, Event> &&
                     std::constructible_from<EventData, T>)
        Event(T &&event, Modifiers eventModifiers = {})
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

} // namespace Bess::Input
