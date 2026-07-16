#pragma once

#include "common/bess_api.h"

#include "ext/vector_float2.hpp"
#include "fwd.hpp"
#include "bess_core/sub_systems/input_sub_system_types.h"
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

    class BESS_API ApplicationEvent {
      public:
        ApplicationEvent(ApplicationEventType type, std::any data);
        ApplicationEventType getType() const;
        template <typename T> T getData() const {
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
            MouseButtonAction action;
            glm::vec2 pos = {};
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
