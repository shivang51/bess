#pragma once
#include "common/sub_system.h"
#include "fwd.hpp"
#include "sub_systems/input_sub_system_types.h"
#include <cstdint>
#define GLFW_INCLUDE_VULKAN
#include "GLFW/glfw3.h"

#include <any>
#include <functional>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace Bess {

    enum class Callback : uint8_t {
        WindowResize,
        MouseWheel,
        KeyPress,
        KeyRelease,
        MouseButton,
        MouseMove
    };

    typedef std::function<void(int, int)> WindowResizeCallback;
    typedef std::function<void(double, double)> MouseWheelCallback;
    typedef std::function<void(int)> KeyReleaseCallback;
    typedef std::function<void(int)> KeyPressCallback;
    typedef std::function<void(MouseButton, MouseButtonAction, glm::vec2)>
        MouseButtonCallback;
    typedef std::function<void(double, double)> MouseMoveCallback;

    class Window : public ISubSystem {
      public:
        struct GLFWwindowDeleter {
            void operator()(GLFWwindow *window) { glfwDestroyWindow(window); }
        };

        Window() = default;

        Window(int width, int height, const std::string &title);

        void onPreInit() override;
        void onInit() override;
        void onDestroy() override;

        bool isClosed() const;
        void close() const;

        void setName(const std::string &name) const;

        static void pollEvents() { glfwPollEvents(); }
        static void waitEvents() { glfwWaitEvents(); }
        static void waitEventsTimeout(double seconds) {
            glfwWaitEventsTimeout(seconds);
        }

        static bool isGLFWInitialized;

        void onWindowResize(WindowResizeCallback callback);
        void onMouseWheel(MouseWheelCallback callback);
        void onKeyPress(KeyPressCallback callback);
        void onKeyRelease(KeyReleaseCallback callback);
        void onMouseButton(MouseButtonCallback callback);
        void onMouseMove(MouseMoveCallback callback);

        glm::vec2 getMousePos() const;

        void setMousePos(const glm::vec2 &pos) const;

        void setEnableCursor(bool enable) const;

        GLFWwindow *getGLFWHandle() const { return mp_window.get(); }

        // Vulkan-specific methods
        void createWindowSurface(VkInstance instance,
                                 VkSurfaceKHR &surface) const;
        std::vector<const char *> getVulkanExtensions() const;
        VkExtent2D getExtent() const;
        bool wasWindowResized() const { return m_framebufferResized; }
        void resetWindowResizedFlag() { m_framebufferResized = false; }

      private:
        KeyCode glfwKeyToKeyCode(int glfwKey) const;

      private:
        std::unique_ptr<GLFWwindow, GLFWwindowDeleter> mp_window;
        std::unordered_map<Callback, std::any> m_callbacks;
        bool m_framebufferResized = false;
        size_t m_width, m_height;
        std::string m_title;

        void initGLFW() const;
        static void framebufferResizeCallback(GLFWwindow *window, int width,
                                              int height);
    };
} // namespace Bess
