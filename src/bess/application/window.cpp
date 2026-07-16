#include "window.h"
#include "bess_core/g_app_context.h"
#include "bess_core/sub_systems/input_sub_system.h"
#include "bess_wgpu/wgpu_renderer_2d.h"
#include "common/bess_assert.h"
#include "common/events.h"
#include "common/logger.h"
#include "event_dispatcher.h"
#include "ext/vector_float2.hpp"
#include "imgui_impl_wgpu.h"
#include "stb_image.h"
#include "sub_systems/renderer_context.h"

#include <GLFW/glfw3.h>
#ifdef __linux__
    #define GLFW_EXPOSE_NATIVE_X11
    #include <GLFW/glfw3native.h>
#endif
#include <cassert>
#include <cstdint>
#include <memory>

#ifdef _WIN32
    #ifndef WIN32_LEAN_AND_MEAN
        #define WIN32_LEAN_AND_MEAN
    #endif
    #ifndef NOMINMAX
        #define NOMINMAX
    #endif
    #include <windows.h>
    #include <shobjidl.h>
#endif

namespace Bess {
    bool Window::isGLFWInitialized = false;

    constexpr char const *instanceClass = "com.shivang.bess";

    Window::Window(int width, int height, const std::string &title)
        : m_width(width),
          m_height(height),
          m_title(title) {
    }

    void Window::onPreUpdate() {
        pollEvents();
    }

    void Window::onUpdate(TimeMs dt) {
        m_ui.update(dt);
    }

    void Window::onPreInit() {
        initGLFW();
    }

    void Window::initGLFW() const {
        if (isGLFWInitialized)
            return;

        glfwSetErrorCallback([](int code, const char *msg) {
            if (code == 65548)
                return;
            BESS_ERROR("[-] GLFW ERROR {} -> {}", code, msg);
        });

        BESS_INFO(
            "[Window] GLFW {}.{}", GLFW_VERSION_MAJOR, GLFW_VERSION_MINOR);

#ifdef __linux__
        // Dawn in this build supports X11 surfaces, not Wayland surfaces.
        glfwInitHint(GLFW_PLATFORM, GLFW_PLATFORM_X11);

        // because renderdoc doesn't support wayland
        if (std::getenv("RENDERDOC_CAPFILE")) {
            BESS_WARN("[Window] RenderDoc detected, forcing X11 backend");
            glfwInitHint(GLFW_PLATFORM, GLFW_PLATFORM_X11);
        }
#endif

        const auto res = glfwInit();
        BESS_ASSERT(res == GLFW_TRUE, "Failed to initialize GLFW");
        glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
        glfwWindowHint(GLFW_SCALE_TO_MONITOR, GLFW_TRUE);
        glfwWindowHint(GLFW_SCALE_FRAMEBUFFER, GLFW_TRUE);
        glfwWindowHint(GLFW_MAXIMIZED, 1);

        isGLFWInitialized = true;
    }

    void Window::onInit() {
#ifdef __linux__
        glfwWindowHintString(GLFW_WAYLAND_APP_ID, instanceClass);
        glfwWindowHintString(GLFW_X11_CLASS_NAME, "Bess");
        glfwWindowHintString(GLFW_X11_INSTANCE_NAME, instanceClass);
#endif

        GLFWwindow *window = glfwCreateWindow(
            (int)m_width, (int)m_height, m_title.c_str(), nullptr, nullptr);

        GLFWimage images[1];
        images[0].pixels = stbi_load("assets/images/logo/BessLogo.png",
                                     &images[0].width,
                                     &images[0].height,
                                     nullptr,
                                     4); // rgba channels
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

        glfwSetWindowSizeLimits(
            window, 600, 500, GLFW_DONT_CARE, GLFW_DONT_CARE);

        glfwSetFramebufferSizeCallback(
            window, [](GLFWwindow *window, int w, int h) {
                const auto this_ = (Window *)glfwGetWindowUserPointer(window);
                this_->m_framebufferResized = true;

                Events::WindowResizeEvent evt{(uint32_t)w, (uint32_t)h};

                auto &ctx = GAppContext::getInstance();
                auto eventDispatcher =
                    ctx.getSubSystem<EventSystem::EventDispatcher>();
                eventDispatcher->queue(evt);
            });

        glfwSetScrollCallback(
            window, [](GLFWwindow *window, double x, double y) {
                const auto this_ = (Window *)glfwGetWindowUserPointer(window);
                auto inputSubSystem =
                    GAppContext::getInstance().getSubSystem<InputSubSystem>();

                inputSubSystem->onMouseWheelEvent({x, y});
            });

        glfwSetKeyCallback(
            window,
            [](GLFWwindow *window,
               int key,
               int scancode,
               int action,
               int mods) {
                const auto this_ = (Window *)glfwGetWindowUserPointer(window);
                KeyAction keyAction =
                    action == GLFW_PRESS
                        ? KeyAction::press
                        : (action == GLFW_RELEASE
                               ? KeyAction::release
                               : (action == GLFW_REPEAT ? KeyAction::hold
                                                        : KeyAction::unknown));

                auto inputSubSystem =
                    GAppContext::getInstance().getSubSystem<InputSubSystem>();
                inputSubSystem->onKeyEvent(this_->glfwKeyToKeyCode(key),
                                           keyAction);
            });

        glfwSetCharCallback(
            window, [](GLFWwindow *window, unsigned int codepoint) {
                auto inputSubSystem =
                    GAppContext::getInstance().getSubSystem<InputSubSystem>();
                inputSubSystem->onTextInputEvent(
                    static_cast<char32_t>(codepoint));
            });

        glfwSetMouseButtonCallback(
            window, [](GLFWwindow *window, int button, int action, int mods) {
                const auto this_ = (Window *)glfwGetWindowUserPointer(window);
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
                    BESS_WARN("[Window] Unhandled mouse button type {}",
                              button);
                    break;
                }

                const auto btnAction = action == GLFW_PRESS
                                           ? MouseButtonAction::press
                                           : MouseButtonAction::release;

                auto inputSubSystem =
                    GAppContext::getInstance().getSubSystem<InputSubSystem>();

                double x = 0.0, y = 0.0;
                glfwGetCursorPos(window, &x, &y);
                inputSubSystem->onMouseButtonEvent(btn, btnAction, {x, y});
            });

        glfwSetCursorPosCallback(
            window, [](GLFWwindow *window, double x, double y) {
                auto inputSubSystem =
                    GAppContext::getInstance().getSubSystem<InputSubSystem>();
                inputSubSystem->onMouseMoveEvent({x, y});
            });

        BESS_INFO("[Window] Created GLFW window {}", m_title);
    }

    void Window::onPostInit() {
        m_ui.init(shared_from_this());
    }

    void Window::onShutdown() {
        m_ui.shutdown();
    }

    void Window::onDestroy() {
        if (!isGLFWInitialized)
            return;

        if (mp_window) {
            BESS_INFO("[Window] Destroying GLFW window {}", m_title);
            mp_window.reset();
        }

        BESS_INFO("[Window] Terminating GLFW");
        glfwTerminate();
        isGLFWInitialized = false;
    }

    void Window::onPreDraw() {
        m_ui.begin();
    }

    void Window::onDraw() {
        m_ui.draw();
    }

    void Window::onPostDraw() {
        m_ui.end();

        const auto &renderer = GAppContext::getInstance()
                                   .getSubSystem<RendererContext>()
                                   ->getRenderer<Wgpu::WgpuRenderer2D>();

        renderer->drawToWindow(shared_from_this(), // FIXME: temp
                               [&](void *renderPass) {
                                   ImGui_ImplWGPU_RenderDrawData(
                                       ImGui::GetDrawData(),
                                       (WGPURenderPassEncoder)renderPass);
                               });
    }

    void Window::onBeginFrame() {
        pollEvents();
    }

    bool Window::isClosed() const {
        return glfwWindowShouldClose(mp_window.get());
    }

#ifdef __linux__
    bool Window::isNativeX11() const {
        return mp_window && glfwGetPlatform() == GLFW_PLATFORM_X11;
    }

    void *Window::getNativeX11Display() const {
        if (!isNativeX11()) {
            return nullptr;
        }

        return glfwGetX11Display();
    }

    unsigned long Window::getNativeX11Window() const {
        if (!isNativeX11()) {
            return 0;
        }

        return glfwGetX11Window(mp_window.get());
    }
#endif

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

    void Window::framebufferResizeCallback(GLFWwindow *window,
                                           int width,
                                           int height) {
        const auto this_ =
            static_cast<Window *>(glfwGetWindowUserPointer(window));
        this_->m_framebufferResized = true;
    }

    void Window::setMousePos(const glm::vec2 &pos) const {
        glfwSetCursorPos(mp_window.get(), pos.x, pos.y);
    }

    void Window::setEnableCursor(bool enable) const {
        if (enable) {
            glfwSetInputMode(mp_window.get(), GLFW_CURSOR, GLFW_CURSOR_NORMAL);
        } else {
            glfwSetInputMode(
                mp_window.get(), GLFW_CURSOR, GLFW_CURSOR_DISABLED);
        }
    }

    KeyCode Window::glfwKeyToKeyCode(int glfwKey) const {
        switch (glfwKey) {
        // Printable Punctuation
        case GLFW_KEY_SPACE:
            return KeyCode::space;
        case GLFW_KEY_APOSTROPHE:
            return KeyCode::apostrophe;
        case GLFW_KEY_COMMA:
            return KeyCode::comma;
        case GLFW_KEY_MINUS:
            return KeyCode::minus;
        case GLFW_KEY_PERIOD:
            return KeyCode::period;
        case GLFW_KEY_SLASH:
            return KeyCode::slash;
        case GLFW_KEY_SEMICOLON:
            return KeyCode::semicolon;
        case GLFW_KEY_EQUAL:
            return KeyCode::equal;
        case GLFW_KEY_LEFT_BRACKET:
            return KeyCode::leftBracket;
        case GLFW_KEY_BACKSLASH:
            return KeyCode::backslash;
        case GLFW_KEY_RIGHT_BRACKET:
            return KeyCode::rightBracket;
        case GLFW_KEY_GRAVE_ACCENT:
            return KeyCode::graveAccent;

        // Numbers
        case GLFW_KEY_0:
            return KeyCode::d0;
        case GLFW_KEY_1:
            return KeyCode::d1;
        case GLFW_KEY_2:
            return KeyCode::d2;
        case GLFW_KEY_3:
            return KeyCode::d3;
        case GLFW_KEY_4:
            return KeyCode::d4;
        case GLFW_KEY_5:
            return KeyCode::d5;
        case GLFW_KEY_6:
            return KeyCode::d6;
        case GLFW_KEY_7:
            return KeyCode::d7;
        case GLFW_KEY_8:
            return KeyCode::d8;
        case GLFW_KEY_9:
            return KeyCode::d9;

        // Letters (A - M)
        case GLFW_KEY_A:
            return KeyCode::a;
        case GLFW_KEY_B:
            return KeyCode::b;
        case GLFW_KEY_C:
            return KeyCode::c;
        case GLFW_KEY_D:
            return KeyCode::d;
        case GLFW_KEY_E:
            return KeyCode::e;
        case GLFW_KEY_F:
            return KeyCode::f;
        case GLFW_KEY_G:
            return KeyCode::g;
        case GLFW_KEY_H:
            return KeyCode::h;
        case GLFW_KEY_I:
            return KeyCode::i;
        case GLFW_KEY_J:
            return KeyCode::j;
        case GLFW_KEY_K:
            return KeyCode::k;
        case GLFW_KEY_L:
            return KeyCode::l;
        case GLFW_KEY_M:
            return KeyCode::m;
            // Letters (N - Z)
        case GLFW_KEY_N:
            return KeyCode::n;
        case GLFW_KEY_O:
            return KeyCode::o;
        case GLFW_KEY_P:
            return KeyCode::p;
        case GLFW_KEY_Q:
            return KeyCode::q;
        case GLFW_KEY_R:
            return KeyCode::r;
        case GLFW_KEY_S:
            return KeyCode::s;
        case GLFW_KEY_T:
            return KeyCode::t;
        case GLFW_KEY_U:
            return KeyCode::u;
        case GLFW_KEY_V:
            return KeyCode::v;
        case GLFW_KEY_W:
            return KeyCode::w;
        case GLFW_KEY_X:
            return KeyCode::x;
        case GLFW_KEY_Y:
            return KeyCode::y;
        case GLFW_KEY_Z:
            return KeyCode::z;

        // Function & Controls
        case GLFW_KEY_ESCAPE:
            return KeyCode::escape;
        case GLFW_KEY_ENTER:
            return KeyCode::enter;
        case GLFW_KEY_TAB:
            return KeyCode::tab;
        case GLFW_KEY_BACKSPACE:
            return KeyCode::backspace;
        case GLFW_KEY_INSERT:
            return KeyCode::insert;
        case GLFW_KEY_DELETE:
            return KeyCode::del;

        // Navigation & Arrow Keys
        case GLFW_KEY_RIGHT:
            return KeyCode::arrowRight;
        case GLFW_KEY_LEFT:
            return KeyCode::arrowLeft;
        case GLFW_KEY_DOWN:
            return KeyCode::arrowDown;
        case GLFW_KEY_UP:
            return KeyCode::arrowUp;
        case GLFW_KEY_PAGE_UP:
            return KeyCode::pageUp;
        case GLFW_KEY_PAGE_DOWN:
            return KeyCode::pageDown;
        case GLFW_KEY_HOME:
            return KeyCode::home;
        case GLFW_KEY_END:
            return KeyCode::end;
            // System Locks & Printing
        case GLFW_KEY_CAPS_LOCK:
            return KeyCode::capsLock;
        case GLFW_KEY_SCROLL_LOCK:
            return KeyCode::scrollLock;
        case GLFW_KEY_NUM_LOCK:
            return KeyCode::numLock;
        case GLFW_KEY_PRINT_SCREEN:
            return KeyCode::printScreen;
        case GLFW_KEY_PAUSE:
            return KeyCode::pause;

        // Function Keys (F1 - F12)
        case GLFW_KEY_F1:
            return KeyCode::f1;
        case GLFW_KEY_F2:
            return KeyCode::f2;
        case GLFW_KEY_F3:
            return KeyCode::f3;
        case GLFW_KEY_F4:
            return KeyCode::f4;
        case GLFW_KEY_F5:
            return KeyCode::f5;
        case GLFW_KEY_F6:
            return KeyCode::f6;
        case GLFW_KEY_F7:
            return KeyCode::f7;
        case GLFW_KEY_F8:
            return KeyCode::f8;
        case GLFW_KEY_F9:
            return KeyCode::f9;
        case GLFW_KEY_F10:
            return KeyCode::f10;
        case GLFW_KEY_F11:
            return KeyCode::f11;
        case GLFW_KEY_F12:
            return KeyCode::f12;

        // Modifier Keys
        case GLFW_KEY_LEFT_SHIFT:
            return KeyCode::leftShift;
        case GLFW_KEY_LEFT_CONTROL:
            return KeyCode::leftControl;
        case GLFW_KEY_LEFT_ALT:
            return KeyCode::leftAlt;
        case GLFW_KEY_LEFT_SUPER:
            return KeyCode::leftSuper;
        case GLFW_KEY_RIGHT_SHIFT:
            return KeyCode::rightShift;
        case GLFW_KEY_RIGHT_CONTROL:
            return KeyCode::rightControl;
        case GLFW_KEY_RIGHT_ALT:
            return KeyCode::rightAlt;
        case GLFW_KEY_RIGHT_SUPER:
            return KeyCode::rightSuper;
        case GLFW_KEY_MENU:
            return KeyCode::menu;

        // Fallback Unhandled Keys
        default:
            BESS_WARN("[Window] Unhandled key code {}", glfwKey);
            return KeyCode::unknown;
        }
    }
} // namespace Bess
