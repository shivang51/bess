#pragma once

#include "GLFW/glfw3.h"
#include "bess_core/input/input_event.h"
#include "common/bess_api.h"
#include "common/sub_system.h"
#include "fwd.hpp"
#include "ui_core.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>

namespace Bess {

    struct BESS_API WindowSurface {
        void *rendereHwd = nullptr;
    };

    class BESS_API Window : public ISubSystem,
                            public std::enable_shared_from_this<Window> {
      public:
        struct GLFWwindowDeleter {
            void operator()(GLFWwindow *window) const noexcept {
                glfwDestroyWindow(window);
            }
        };

        struct GLFWcursorDeleter {
            void operator()(GLFWcursor *cursor) const noexcept {
                glfwDestroyCursor(cursor);
            }
        };

        Window() = default;

        Window(const Window &) = delete;
        Window &operator=(const Window &) = delete;

        Window(int width, int height, const std::string &title);

        void onPreUpdate() override;
        void onUpdate(TimeMs dt) override;
        void onPreInit() override;
        void onInit() override;
        void onPostInit() override;
        void onShutdown() override;
        void onDestroy() override;

        void onPostDraw() override;

        void onBeginFrame() override;

        bool isClosed() const;
        void close() const;

        void setName(const std::string &name) const;

        static void pollEvents() {
            glfwPollEvents();
        }
        static void waitEvents() {
            glfwWaitEvents();
        }
        static void waitEventsTimeout(double seconds) {
            glfwWaitEventsTimeout(seconds);
        }

        static bool isGLFWInitialized;

        glm::vec2 getMousePos() const;

        void setMousePos(const glm::vec2 &pos) const;

        void setEnableCursor(bool enable) const;

        void setCursor(CursorIcon shape) noexcept;

        GLFWwindow *getGLFWHandle() const {
            return mp_window.get();
        }

#if defined(__linux__)
        bool isNativeX11() const;
        void *getNativeX11Display() const;
        unsigned long getNativeX11Window() const;
#endif

        bool wasWindowResized() const {
            return m_framebufferResized;
        }
        void resetWindowResizedFlag() {
            m_framebufferResized = false;
        }

        MAKE_GETTER_SETTER(WindowSurface, surface, m_surface)

      private:
        KeyCode glfwKeyToKeyCode(int glfwKey) const;
        [[nodiscard]] Input::Modifiers
        glfwModifiersToInput(int glfwModifiers) const;
        [[nodiscard]] Input::Modifiers currentInputModifiers() const;
        void dispatchInputEvent(Input::Event event);
        [[nodiscard]] glm::vec2
        windowToUITargetPos(double x, double y) const;

        static constexpr std::size_t cursorShapeCount =
            static_cast<std::size_t>(CursorIcon::resizeDiagonalNESW) + 1;

      private:
        std::unique_ptr<GLFWwindow, GLFWwindowDeleter> mp_window;
        bool m_framebufferResized = false;
        size_t m_width, m_height;
        std::string m_title;

        void initGLFW() const;
        bool syncFramebufferSize(bool notifyResizeEvent);
        void
        applyFramebufferSize(int width, int height, bool notifyResizeEvent);
        static void
        framebufferResizeCallback(GLFWwindow *window, int width, int height);

        WindowSurface m_surface;
        UI::UITarget m_uiTarget;
        Input::Modifiers m_inputModifiers;
        std::array<std::unique_ptr<GLFWcursor, GLFWcursorDeleter>,
                   cursorShapeCount>
            m_standardCursors;
        std::array<bool, cursorShapeCount> m_cursorCreationFailed{};
        CursorIcon m_cursorShape = CursorIcon::arrow;
    };
} // namespace Bess
