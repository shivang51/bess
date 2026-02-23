#include "application/window.h"
#include "common/bess_assert.h"
#include "common/logger.h"
#include "events/application_event.h"
#include "stb_image.h"
#include "vec2.hpp"
#include <GLFW/glfw3.h>
#include <cassert>
#include <cstdint>
#include <memory>
#include <stdexcept>

namespace Bess {
    bool Window::isGLFWInitialized = false;

    constexpr char const *instanceClass = "com.shivang.bess";

    Window::Window(int width, int height, const std::string &title) {

        this->initGLFW();
#ifdef __linux__
        glfwWindowHintString(GLFW_WAYLAND_APP_ID, instanceClass);
        glfwWindowHintString(GLFW_X11_CLASS_NAME, "Bess");
        glfwWindowHintString(GLFW_X11_INSTANCE_NAME, instanceClass);
#endif

        GLFWwindow *window = glfwCreateWindow(width, height, title.c_str(), nullptr, nullptr);

        GLFWimage images[1];
        images[0].pixels = stbi_load("assets/images/logo/BessLogo.png",
                                     &images[0].width,
                                     &images[0].height, nullptr, 4); // rgba channels
        glfwSetWindowIcon(window, 1, images);
        stbi_image_free(images[0].pixels);

#ifdef _WIN32
        SetCurrentProcessExplicitAppUserModelID(L"com.shivang.bess");
#endif

        auto platform = glfwGetPlatform();
        switch (platform) {
        case GLFW_PLATFORM_WIN32:
            BESS_INFO("[Window] Platform: Win32");
            break;
        case GLFW_PLATFORM_X11:
            BESS_INFO("[Window] Platform: X11");
            break;
        case GLFW_PLATFORM_WAYLAND:
            BESS_INFO("[Window] Platform: Wayland");
            break;
        case GLFW_PLATFORM_COCOA:
            BESS_INFO("[Window] Platform: Cocoa");
            break;
        default:
            BESS_INFO("[Window] Platform: Unknown");
            break;
        }

        assert(window != nullptr);
        glfwSetWindowUserPointer(window, this);

        mp_window = std::unique_ptr<GLFWwindow, GLFWwindowDeleter>(window);

        glfwSetWindowSizeLimits(window, 600, 500, GLFW_DONT_CARE, GLFW_DONT_CARE);

        glfwSetFramebufferSizeCallback(
            window, [](GLFWwindow *window, int w, int h) {
                const auto this_ = (Window *)glfwGetWindowUserPointer(window);
                this_->m_framebufferResized = true;
                if (!this_->m_callbacks.contains(Callback::WindowResize))
                    return;
                const auto cb = std::any_cast<WindowResizeCallback>(
                    this_->m_callbacks[Callback::WindowResize]);
                cb(w, h);
            });

        glfwSetScrollCallback(window, [](GLFWwindow *window, double x, double y) {
            const auto this_ = (Window *)glfwGetWindowUserPointer(window);
            if (!this_->m_callbacks.contains(Callback::MouseWheel))
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
                if (!this_->m_callbacks.contains(Callback::KeyPress))
                    return;
                const auto cb = std::any_cast<KeyPressCallback>(
                    this_->m_callbacks[Callback::KeyPress]);
                cb(key);
            } break;
            case GLFW_RELEASE: {
                if (!this_->m_callbacks.contains(Callback::KeyRelease))
                    return;
                const auto cb = std::any_cast<KeyReleaseCallback>(
                    this_->m_callbacks[Callback::KeyRelease]);
                cb(key);
            } break;
            default:
                BESS_WARN("[Window] Unhandled key action type {}", action);
                break;
            }
        });

        glfwSetMouseButtonCallback(
            window, [](GLFWwindow *window, int button, int action, int mods) {
                const auto this_ = (Window *)glfwGetWindowUserPointer(window);
                const auto cb = std::any_cast<MouseButtonCallback>(
                    this_->m_callbacks[Callback::MouseButton]);

                MouseButton btn = MouseButton::unknown;

                switch (button) {
                case GLFW_MOUSE_BUTTON_LEFT: {
                    btn = MouseButton::left;
                } break;
                case GLFW_MOUSE_BUTTON_RIGHT: {
                    btn = MouseButton::right;
                } break;
                case GLFW_MOUSE_BUTTON_MIDDLE: {
                    btn = MouseButton::middle;
                } break;
                default:
                    BESS_WARN("[Window] Unhandled mouse button type {}", button);
                    break;
                }

                const auto btnAction = action == GLFW_PRESS
                                           ? MouseButtonAction::press
                                           : MouseButtonAction::release;

                cb(btn, btnAction);
            });

        glfwSetCursorPosCallback(
            window, [](GLFWwindow *window, double x, double y) {
                const auto this_ = (Window *)glfwGetWindowUserPointer(window);
                if (!this_->m_callbacks.contains(Callback::MouseMove))
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
        glfwWindowHint(GLFW_SCALE_TO_MONITOR, GLFW_TRUE);
        glfwWindowHint(GLFW_SCALE_FRAMEBUFFER, GLFW_TRUE);
        glfwWindowHint(GLFW_MAXIMIZED, 1);
        isGLFWInitialized = true;
    }

    Window::~Window() {
        if (!isGLFWInitialized)
            return;

        if (mp_window) // clear if not nullptr
            this->mp_window.reset();

        BESS_INFO("[Window] Window destroyed, terminating GLFW");
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

    void Window::onMouseButton(MouseButtonCallback callback) {
        m_callbacks[Callback::MouseButton] = callback;
    }

    void Window::onMouseMove(MouseMoveCallback callback) {
        m_callbacks[Callback::MouseMove] = callback;
    }

    void Window::close() const {
        glfwSetWindowShouldClose(mp_window.get(), true);
    }

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

    void Window::destroy() {
        if (mp_window)
            mp_window.reset();
    }
} // namespace Bess
