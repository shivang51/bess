#pragma once

#include <any>
namespace Bess {
    enum class ApplicationEventType {
        WindowResize,
        MouseMove,
        MouseWheel,
        MouseButton,
        KeyPress,
        KeyRelease
    };

    enum class MouseButton {
        left,
        right,
        middle
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
