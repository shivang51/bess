#pragma once
#include "GLFW/glfw3.h"
#include "bess_core/renderer/texture.h"
#include "common/sub_system.h"
#include "fwd.hpp"
#include "bess_core/sub_systems/input_sub_system_types.h"
#include "ui/ui.h"

#include <memory>
#include <string>

namespace Bess {

    struct WindowSurface {
        void *rendereHwd = nullptr;
    };

    class Window : public ISubSystem,
                   public std::enable_shared_from_this<Window> {
      public:
        struct GLFWwindowDeleter {
            void operator()(GLFWwindow *window) {
                glfwDestroyWindow(window);
            }
        };

        Window() = default;

        Window(int width, int height, const std::string &title);

        void onPreUpdate() override;
        void onUpdate(TimeMs dt) override;
        void onPreInit() override;
        void onInit() override;
        void onPostInit() override;
        void onShutdown() override;
        void onDestroy() override;

        void onPreDraw() override;
        void onDraw() override;
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

        GLFWwindow *getGLFWHandle() const {
            return mp_window.get();
        }

        bool wasWindowResized() const {
            return m_framebufferResized;
        }
        void resetWindowResizedFlag() {
            m_framebufferResized = false;
        }

        MAKE_GETTER_SETTER(WindowSurface, surface, m_surface)
        MAKE_GETTER(UIHandle, ui, m_ui)

      private:
        KeyCode glfwKeyToKeyCode(int glfwKey) const;

      private:
        std::unique_ptr<GLFWwindow, GLFWwindowDeleter> mp_window;
        bool m_framebufferResized = false;
        size_t m_width, m_height;
        std::string m_title;

        void initGLFW() const;
        static void
        framebufferResizeCallback(GLFWwindow *window, int width, int height);

        WindowSurface m_surface;
        UIHandle m_ui;
    };
} // namespace Bess
