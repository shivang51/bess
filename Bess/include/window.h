#pragma once
#define GLFW_INCLUDE_VULKAN
#include "GLFW/glfw3.h"
#include "glm.hpp"

#include <any>
#include <functional>
#include <iostream>
#include <memory>
#include <string>
#include <vector>
#include <unordered_map>

namespace Bess {

    enum Callback {
        WindowResize,
        MouseWheel,
        KeyPress,
        KeyRelease,
        LeftMouse,
        RightMouse,
        MiddleMouse,
        MouseMove
    };

    typedef std::function<void(int, int)> WindowResizeCallback;
    typedef std::function<void(double, double)> MouseWheelCallback;
    typedef std::function<void(int)> KeyReleaseCallback;
    typedef std::function<void(int)> KeyPressCallback;
    typedef std::function<void(bool)> LeftMouseCallback;
    typedef std::function<void(bool)> RightMouseCallback;
    typedef std::function<void(bool)> MiddleMouseCallback;
    typedef std::function<void(double, double)> MouseMoveCallback;

    class Window {
      public:
        struct GLFWwindowDeleter {
            void operator()(GLFWwindow *window) {
                glfwDestroyWindow(window);
            }
        };

        Window(int width, int height, const std::string &title);
        ~Window();

        void update() const;
        void makeCurrent() const;
        bool isClosed() const;
        void close() const;

        void setName(const std::string &name);

        static void pollEvents() { glfwPollEvents(); }
        static void waitEvents() { glfwWaitEvents(); }
        static void waitEventsTimeout(double seconds) {
            glfwWaitEventsTimeout(seconds);
        }

        static bool isGLFWInitialized;
        static bool isVulkanInitialized;

        void onWindowResize(WindowResizeCallback callback);
        void onMouseWheel(MouseWheelCallback callback);
        void onKeyPress(KeyPressCallback callback);
        void onKeyRelease(KeyReleaseCallback callback);
        void onLeftMouse(LeftMouseCallback callback);
        void onRightMouse(RightMouseCallback callback);
        void onMiddleMouse(MiddleMouseCallback callback);
        void onMouseMove(MouseMoveCallback callback);

        glm::vec2 getMousePos() const;

        GLFWwindow *getGLFWHandle() const { return mp_window.get(); }

        // Vulkan-specific methods
        void createWindowSurface(VkInstance instance, VkSurfaceKHR& surface);
        std::vector<const char*> getVulkanExtensions();
        VkExtent2D getExtent() const;
        bool wasWindowResized() const { return m_framebufferResized; }
        void resetWindowResizedFlag() { m_framebufferResized = false; }

      private:
        std::unique_ptr<GLFWwindow, GLFWwindowDeleter> mp_window;
        std::unordered_map<Callback, std::any> m_callbacks;
        bool m_framebufferResized = false;

        void initVulkan();
        void initGLFW();
        static void framebufferResizeCallback(GLFWwindow* window, int width, int height);
    };
} // namespace Bess
