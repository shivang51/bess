#pragma once
#include "common/sub_system.h"
#include "fwd.hpp"
#include "sub_systems/input_sub_system_types.h"
#define GLFW_INCLUDE_VULKAN
#include "GLFW/glfw3.h"

#include <memory>
#include <string>
#include <vector>

namespace Bess {
    class Window : public ISubSystem {
      public:
        struct GLFWwindowDeleter {
            void operator()(GLFWwindow *window) { glfwDestroyWindow(window); }
        };

        Window() = default;

        Window(int width, int height, const std::string &title);

        void onPreUpdate() override;
        void onPreInit() override;
        void onInit() override;
        void onDestroy() override;

        void onBeginFrame() override;

        bool isClosed() const;
        void close() const;

        void setName(const std::string &name) const;

        static void pollEvents() { glfwPollEvents(); }
        static void waitEvents() { glfwWaitEvents(); }
        static void waitEventsTimeout(double seconds) {
            glfwWaitEventsTimeout(seconds);
        }

        static bool isGLFWInitialized;

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
        bool m_framebufferResized = false;
        size_t m_width, m_height;
        std::string m_title;

        void initGLFW() const;
        static void framebufferResizeCallback(GLFWwindow *window, int width,
                                              int height);
    };
} // namespace Bess
