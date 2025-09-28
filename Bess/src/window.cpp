#include "window.h"
#include "common/log.h"
#include "scene/renderer/gl/gl_wrapper.h"
#include <cassert>
#include <iostream>
#include <memory>

namespace Bess {
    bool Window::isGLFWInitialized = false;
    bool Window::isGladInitialized = false;

    Window::Window(const int width, const int height, const std::string &title) {

        this->initGLFW();

        GLFWwindow *window = glfwCreateWindow(width, height, title.c_str(), nullptr, nullptr);

        assert(window != nullptr);
        glfwSetWindowUserPointer(window, this);

        mp_window = std::unique_ptr<GLFWwindow, GLFWwindowDeleter>(window);
        this->makeCurrent();

        glfwSwapInterval(0);

        glfwSetWindowSizeLimits(window, 600, 500, GLFW_DONT_CARE, GLFW_DONT_CARE);

        glfwSetFramebufferSizeCallback(
            window, [](GLFWwindow *window, const int w, const int h) {
                const auto this_ = (Window *)glfwGetWindowUserPointer(window);
                if (!this_->m_callbacks.contains(Callback::WindowResize))
                    return;
                const auto cb = std::any_cast<WindowResizeCallback>(
                    this_->m_callbacks[Callback::WindowResize]);
                cb(w, h);
            });

        glfwSetScrollCallback(window, [](GLFWwindow *window, const double x, const double y) {
            const auto this_ = (Window *)glfwGetWindowUserPointer(window);
            if (!this_->m_callbacks.contains(Callback::WindowResize))
                return;
            const auto cb = std::any_cast<MouseWheelCallback>(
                this_->m_callbacks[Callback::MouseWheel]);
            cb(x, y);
        });

        glfwSetKeyCallback(window, [](GLFWwindow *window, const int key, int scancode,
                                      const int action, int mods) {
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
            }
        });

        glfwSetMouseButtonCallback(
            window, [](GLFWwindow *window, const int button, const int action, int mods) {
                const auto this_ = (Window *)glfwGetWindowUserPointer(window);
                switch (button) {
                case GLFW_MOUSE_BUTTON_LEFT: {
                    if (!this_->m_callbacks.contains(Callback::LeftMouse))
                        return;
                    const auto cb = std::any_cast<LeftMouseCallback>(
                        this_->m_callbacks[Callback::LeftMouse]);
                    cb(action == GLFW_PRESS);
                } break;
                case GLFW_MOUSE_BUTTON_RIGHT: {
                    if (!this_->m_callbacks.contains(Callback::RightMouse))
                        return;
                    const auto cb = std::any_cast<RightMouseCallback>(
                        this_->m_callbacks[Callback::RightMouse]);
                    cb(action == GLFW_PRESS);
                } break;
                case GLFW_MOUSE_BUTTON_MIDDLE: {
                    if (!this_->m_callbacks.contains(Callback::MiddleMouse))
                        return;
                    const auto cb = std::any_cast<MiddleMouseCallback>(
                        this_->m_callbacks[Callback::MiddleMouse]);
                    cb(action == GLFW_PRESS);
                } break;
                }
            });

        glfwSetCursorPosCallback(
            window, [](GLFWwindow *window, const double x, const double y) {
                const auto this_ = (Window *)glfwGetWindowUserPointer(window);
                if (!this_->m_callbacks.contains(Callback::MouseMove))
                    return;
                const auto cb = std::any_cast<MouseMoveCallback>(this_->m_callbacks[Callback::MouseMove]);
                cb(x, y);
            });

        this->initOpenGL();
    }

    void Window::initGLFW() {
        if (isGLFWInitialized)
            return;

        glfwSetErrorCallback([](int code, const char *msg) {
            if (code == 65548)
                return;
            BESS_ERROR("[-] GLFW ERROR {} -> {}", code, msg);
        });
        const auto res = glfwInit();
        assert(res == GLFW_TRUE);
        glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
        glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
        glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
        glfwWindowHint(GLFW_SAMPLES, 4);
        glfwWindowHint(GLFW_MAXIMIZED, 1);

        isGLFWInitialized = true;
    }

    void Window::initOpenGL() {
        if (isGladInitialized)
            return;
        if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
            std::cerr << "Failed to initialize GLAD" << std::endl;
        } else {
            isGladInitialized = true;
        }

        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

        glEnable(GL_MULTISAMPLE);
        glEnable(GL_LINE_SMOOTH);
        glEnable(GL_POLYGON_SMOOTH);
        glHint(GL_LINE_SMOOTH_HINT, GL_NICEST);
        glHint(GL_POLYGON_SMOOTH_HINT, GL_NICEST);

        glEnable(GL_DEPTH_TEST);
        glDepthFunc(GL_LEQUAL);
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
        makeCurrent();
        glfwSwapBuffers(mp_window.get());
    }

    void Window::makeCurrent() const { glfwMakeContextCurrent(mp_window.get()); }

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

    void Window::setName(const std::string &name) {
        glfwSetWindowTitle(mp_window.get(), name.c_str());
    }

    glm::vec2 Window::getMousePos() const {
        double x, y;
        glfwGetCursorPos(mp_window.get(), &x, &y);
        return glm::vec2(x, y);
    }
} // namespace Bess
