#pragma once

#include "fwd.hpp"
#include <any>
namespace Bess {
    enum class ApplicationEventType : uint8_t {
        WindowResize,
        MouseMove,
        MouseWheel,
        MouseButton,
        KeyPress,
        KeyRelease
    };

    enum class MouseClickAction : uint8_t {
        release = 0,
        press = 1,
        repeat = 2
    };

    enum class MouseButton : uint8_t {
        left = 0,
        right = 1,
        middle = 2,
        button4 = 3,
        button5 = 4,
        button6 = 5,
        button7 = 6,
        button8 = 7
    };

    class ApplicationEvent {
      public:
        ApplicationEvent(ApplicationEventType type, std::any data);
        ApplicationEventType getType() const;
        template <typename T>
        T getData() const {
            auto data = std::any_cast<T>(m_data);
            return data;
        }

      public:
        struct WindowResizeData {
            int width;
            int height;
        };

        struct MouseMoveData {
            double x;
            double y;
        };

        struct MouseButtonData {
            MouseButton button;
            bool pressed;
        };

        struct KeyPressData {
            int key;
        };

        struct KeyReleaseData {
            int key;
        };

        struct MouseWheelData {
            double x;
            double y;
        };

      private:
        ApplicationEventType m_type;
        std::any m_data;
    };
} // namespace Bess
