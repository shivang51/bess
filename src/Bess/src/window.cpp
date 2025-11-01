#include "window.h"

#include "common/log.h"
#include <cassert>
#include <cstdint>
#include <math.h>
#include <memory>
#include <stdexcept>

namespace Bess {
    bool Window::isGLFWInitialized = false;

    Window::Window(int width, int height, const std::string &title) {

        this->initGLFW();

        GLFWwindow *window = glfwCreateWindow(width, height, title.c_str(), nullptr, nullptr);

        assert(window != nullptr);
        glfwSetWindowUserPointer(window, this);

        mp_window = std::unique_ptr<GLFWwindow, GLFWwindowDeleter>(window);

        glfwSetWindowSizeLimits(window, 600, 500, GLFW_DONT_CARE, GLFW_DONT_CARE);

        glfwSetFramebufferSizeCallback(
            window, [](GLFWwindow *window, int w, int h) {
                const auto this_ = (Window *)glfwGetWindowUserPointer(window);
                this_->m_framebufferResized = true;
                if (this_->m_callbacks.find(Callback::WindowResize) ==
                    this_->m_callbacks.end())
                    return;
                const auto cb = std::any_cast<WindowResizeCallback>(
                    this_->m_callbacks[Callback::WindowResize]);
                cb(w, h);
            });

        glfwSetScrollCallback(window, [](GLFWwindow *window, double x, double y) {
            const auto this_ = (Window *)glfwGetWindowUserPointer(window);
            if (this_->m_callbacks.find(Callback::WindowResize) ==
                this_->m_callbacks.end())
                return;
            const auto cb = std::any_cast<MouseWheelCallback>(
                this_->m_callbacks[Callback::MouseWheel]);
            cb(x, y);
        });

        glfwSetKeyCallback(window, [](GLFWwindow *window, int key, int scancode,
                                      int action, int mods) {
            const auto this_ = (Window *)glfwGetWindowUserPointer(window);
            switch (action) {
            case GLFW_PRESS: {
                if (this_->m_callbacks.find(Callback::KeyPress) ==
                    this_->m_callbacks.end())
                    return;
                const auto cb = std::any_cast<KeyPressCallback>(
                    this_->m_callbacks[Callback::KeyPress]);
                cb(key);
            } break;
            case GLFW_RELEASE: {
                if (this_->m_callbacks.find(Callback::KeyRelease) ==
                    this_->m_callbacks.end())
                    return;
                const auto cb = std::any_cast<KeyReleaseCallback>(
                    this_->m_callbacks[Callback::KeyRelease]);
                cb(key);
            } break;
            }
        });

        glfwSetMouseButtonCallback(
            window, [](GLFWwindow *window, int button, int action, int mods) {
                const auto this_ = (Window *)glfwGetWindowUserPointer(window);
                switch (button) {
                case GLFW_MOUSE_BUTTON_LEFT: {
                    if (this_->m_callbacks.find(Callback::LeftMouse) ==
                        this_->m_callbacks.end())
                        return;
                    const auto cb = std::any_cast<LeftMouseCallback>(
                        this_->m_callbacks[Callback::LeftMouse]);
                    cb(action == GLFW_PRESS);
                } break;
                case GLFW_MOUSE_BUTTON_RIGHT: {
                    if (this_->m_callbacks.find(Callback::RightMouse) ==
                        this_->m_callbacks.end())
                        return;
                    const auto cb = std::any_cast<RightMouseCallback>(
                        this_->m_callbacks[Callback::RightMouse]);
                    cb(action == GLFW_PRESS);
                } break;
                case GLFW_MOUSE_BUTTON_MIDDLE: {
                    if (this_->m_callbacks.find(Callback::MiddleMouse) ==
                        this_->m_callbacks.end())
                        return;
                    const auto cb = std::any_cast<MiddleMouseCallback>(
                        this_->m_callbacks[Callback::MiddleMouse]);
                    cb(action == GLFW_PRESS);
                } break;
                }
            });

        glfwSetCursorPosCallback(
            window, [](GLFWwindow *window, double x, double y) {
                const auto this_ = (Window *)glfwGetWindowUserPointer(window);
                if (this_->m_callbacks.find(Callback::MouseMove) ==
                    this_->m_callbacks.end())
                    return;
                const auto cb = std::any_cast<MouseMoveCallback>(this_->m_callbacks[Callback::MouseMove]);
                cb(x, y);
            });
    }

    void Window::initGLFW() const {
        if (isGLFWInitialized)
            return;

        glfwSetErrorCallback([](int code, const char *msg) {
            if (code == 65548)
                return;
            BESS_ERROR("[-] GLFW ERROR {} -> {}", code, msg);
        });

        BESS_INFO("[Window] GLFW {}.{}", GLFW_VERSION_MAJOR, GLFW_VERSION_MINOR);

        const auto res = glfwInit();
        assert(res == GLFW_TRUE);
        glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
        glfwWindowHint(GLFW_MAXIMIZED, 1);

        isGLFWInitialized = true;
    }

    Window::~Window() {
        if (!isGLFWInitialized)
            return;

        if (mp_window) // clear if not nullptr
            this->mp_window.reset();

        glfwTerminate();
        isGLFWInitialized = false;
    }

    void Window::update() const {
    }

    bool Window::isClosed() const { return glfwWindowShouldClose(mp_window.get()); }

    void Window::onWindowResize(WindowResizeCallback callback) {
        m_callbacks[Callback::WindowResize] = callback;
    }

    void Window::onMouseWheel(MouseWheelCallback callback) {
        m_callbacks[Callback::MouseWheel] = callback;
    }

    void Window::onKeyPress(KeyPressCallback callback) {
        m_callbacks[Callback::KeyPress] = callback;
    }

    void Window::onKeyRelease(KeyReleaseCallback callback) {
        m_callbacks[Callback::KeyRelease] = callback;
    }

    void Window::onLeftMouse(LeftMouseCallback callback) {
        m_callbacks[Callback::LeftMouse] = callback;
    }

    void Window::onRightMouse(RightMouseCallback callback) {
        m_callbacks[Callback::RightMouse] = callback;
    }

    void Window::onMiddleMouse(MiddleMouseCallback callback) {
        m_callbacks[Callback::MiddleMouse] = callback;
    }

    void Window::onMouseMove(MouseMoveCallback callback) {
        m_callbacks[Callback::MouseMove] = callback;
    }

    void Window::close() const { glfwSetWindowShouldClose(mp_window.get(), true); }

    void Window::setName(const std::string &name) const {
        glfwSetWindowTitle(mp_window.get(), name.c_str());
    }

    glm::vec2 Window::getMousePos() const {
        double x = 0.0, y = 0.0;
        glfwGetCursorPos(mp_window.get(), &x, &y);
        return {x, y};
    }

    void Window::createWindowSurface(VkInstance instance, VkSurfaceKHR &surface) const {
        if (glfwCreateWindowSurface(instance, mp_window.get(), nullptr, &surface) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create window surface!");
        }
    }

    std::vector<const char *> Window::getVulkanExtensions() const {
        uint32_t glfwExtensionCount = 0;
        const char **glfwExtensions = nullptr;
        glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);

        std::vector<const char *> extensions(glfwExtensions, glfwExtensionCount + glfwExtensions);

        return extensions;
    }

    VkExtent2D Window::getExtent() const {
        int width = 0, height = 0;
        glfwGetFramebufferSize(mp_window.get(), &width, &height);
        return {static_cast<uint32_t>(width), static_cast<uint32_t>(height)};
    }

    void Window::framebufferResizeCallback(GLFWwindow *window, int width, int height) {
        const auto this_ = static_cast<Window *>(glfwGetWindowUserPointer(window));
        this_->m_framebufferResized = true;
    }
} // namespace Bess
