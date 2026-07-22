#include "window.h"
#include "bess_core/g_app_context.h"
#include "bess_core/scene/scene_state/components/scene_component_types.h"
#include "bess_core/settings/themes.h"
#include "bess_core/sub_systems/input_sub_system.h"
#include "common/bess_assert.h"
#include "common/events.h"
#include "common/logger.h"
#include "event_dispatcher.h"
#include "ext/vector_float2.hpp"
#include "stb_image.h"
#include "sub_systems/renderer_context.h"

#include <GLFW/glfw3.h>
#ifdef __linux__
    #define GLFW_EXPOSE_NATIVE_X11
    #include <GLFW/glfw3native.h>
#endif
#include <algorithm>
#include <cassert>
#include <concepts>
#include <cstdint>
#include <memory>
#include <optional>
#include <string_view>
#include <type_traits>

#ifdef _WIN32
    #ifndef WIN32_LEAN_AND_MEAN
        #define WIN32_LEAN_AND_MEAN
    #endif
    #ifndef NOMINMAX
        #define NOMINMAX
    #endif
    #include <shobjidl.h>
    #include <windows.h>
#endif

namespace Bess {
    bool Window::isGLFWInitialized = false;

    namespace {
        constexpr char const *instanceClass = "com.shivang.bess";

        MouseButton mouseButtonFromGLFW(const int button) {
            switch (button) {
            case GLFW_MOUSE_BUTTON_LEFT:
                return MouseButton::left;
            case GLFW_MOUSE_BUTTON_RIGHT:
                return MouseButton::right;
            case GLFW_MOUSE_BUTTON_MIDDLE:
                return MouseButton::middle;
            case GLFW_MOUSE_BUTTON_4:
                return MouseButton::button4;
            case GLFW_MOUSE_BUTTON_5:
                return MouseButton::button5;
            case GLFW_MOUSE_BUTTON_6:
                return MouseButton::button6;
            case GLFW_MOUSE_BUTTON_7:
                return MouseButton::button7;
            case GLFW_MOUSE_BUTTON_8:
                return MouseButton::button8;
            default:
                return MouseButton::unknown;
            }
        }

        KeyAction keyActionFromGLFW(const int action) {
            switch (action) {
            case GLFW_PRESS:
                return KeyAction::press;
            case GLFW_RELEASE:
                return KeyAction::release;
            case GLFW_REPEAT:
                return KeyAction::hold;
            default:
                return KeyAction::unknown;
            }
        }

        CursorIcon normalizedCursorShape(const CursorIcon shape) noexcept {
            switch (shape) {
            case CursorIcon::inherit:
            case CursorIcon::arrow:
                return CursorIcon::arrow;
            case CursorIcon::pointer:
            case CursorIcon::move:
            case CursorIcon::text:
            case CursorIcon::resizeHorizontal:
            case CursorIcon::resizeVertical:
            case CursorIcon::resizeDiagonalNWSE:
            case CursorIcon::resizeDiagonalNESW:
                return shape;
            }
            return CursorIcon::arrow;
        }

        int glfwCursorShape(const CursorIcon shape) noexcept {
            switch (shape) {
            case CursorIcon::pointer:
                return GLFW_POINTING_HAND_CURSOR;
            case CursorIcon::move:
                return GLFW_RESIZE_ALL_CURSOR;
            case CursorIcon::text:
                return GLFW_IBEAM_CURSOR;
            case CursorIcon::resizeHorizontal:
                return GLFW_RESIZE_EW_CURSOR;
            case CursorIcon::resizeVertical:
                return GLFW_RESIZE_NS_CURSOR;
            case CursorIcon::resizeDiagonalNWSE:
                return GLFW_RESIZE_NWSE_CURSOR;
            case CursorIcon::resizeDiagonalNESW:
                return GLFW_RESIZE_NESW_CURSOR;
            case CursorIcon::inherit:
            case CursorIcon::arrow:
                return GLFW_ARROW_CURSOR;
            }
            return GLFW_ARROW_CURSOR;
        }

        class GLFWUIPlatformServices final : public UI::UIPlatformServices {
          public:
            explicit GLFWUIPlatformServices(GLFWwindow *window) noexcept
                : m_window(window) {
            }

            std::optional<std::string> readClipboardText() const override {
                if (m_window == nullptr) {
                    return std::nullopt;
                }
                const char *text = glfwGetClipboardString(m_window);
                return text != nullptr
                           ? std::optional<std::string>{std::string{text}}
                           : std::nullopt;
            }

            bool writeClipboardText(std::string_view text) override {
                if (m_window == nullptr) {
                    return false;
                }
                // GLFW requires a null-terminated buffer.
                glfwSetClipboardString(m_window, std::string{text}.c_str());
                return true;
            }

          private:
            GLFWwindow *m_window = nullptr;
        };
    } // namespace

    Window::Window(int width, int height, const std::string &title)
        : m_width(width),
          m_height(height),
          m_title(title) {
    }

    void Window::onPreUpdate() {
        pollEvents();
    }

    void Window::onUpdate(TimeMs dt) {
        m_uiTarget.update(dt);
        setCursor(m_uiTarget.getCursorShape());
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
        glfwSetInputMode(window, GLFW_LOCK_KEY_MODS, GLFW_TRUE);

        glfwSetFramebufferSizeCallback(
            window, [](GLFWwindow *window, int w, int h) {
                framebufferResizeCallback(window, w, h);
            });

        glfwSetScrollCallback(
            window, [](GLFWwindow *window, double x, double y) {
                const auto this_ = (Window *)glfwGetWindowUserPointer(window);
                double mouseX = 0.0;
                double mouseY = 0.0;
                glfwGetCursorPos(window, &mouseX, &mouseY);
                this_->dispatchInputEvent(Input::Event{
                    Input::MouseWheelEvent{
                        .pos = {static_cast<float>(mouseX),
                                static_cast<float>(mouseY)},
                        .offset = {static_cast<float>(x),
                                   static_cast<float>(y)},
                    },
                    this_->currentInputModifiers(),
                });
            });

        glfwSetKeyCallback(
            window,
            [](GLFWwindow *window,
               int key,
               int scancode,
               int action,
               int mods) {
                const auto this_ = (Window *)glfwGetWindowUserPointer(window);
                static_cast<void>(scancode);
                const KeyAction keyAction = keyActionFromGLFW(action);
                this_->m_inputModifiers = this_->glfwModifiersToInput(mods);
                const auto keyCode = this_->glfwKeyToKeyCode(key);
                this_->dispatchInputEvent(Input::Event{
                    Input::KeyEvent{.key = keyCode, .action = keyAction},
                    this_->m_inputModifiers,
                });
            });

        glfwSetCharCallback(
            window, [](GLFWwindow *window, unsigned int codepoint) {
                const auto this_ = (Window *)glfwGetWindowUserPointer(window);
                this_->dispatchInputEvent(Input::Event{
                    Input::TextInputEvent{
                        .codepoint = static_cast<char32_t>(codepoint),
                    },
                    this_->currentInputModifiers(),
                });
            });

        glfwSetMouseButtonCallback(
            window, [](GLFWwindow *window, int button, int action, int mods) {
                const auto this_ = (Window *)glfwGetWindowUserPointer(window);
                const MouseButton btn = mouseButtonFromGLFW(button);
                if (btn == MouseButton::unknown) {
                    BESS_WARN("[Window] Unhandled mouse button type {}",
                              button);
                }
                const auto btnAction = action == GLFW_PRESS
                                           ? MouseButtonAction::press
                                           : MouseButtonAction::release;
                this_->m_inputModifiers = this_->glfwModifiersToInput(mods);

                double x = 0.0, y = 0.0;
                glfwGetCursorPos(window, &x, &y);
                this_->dispatchInputEvent(Input::Event{
                    Input::MouseButtonEvent{
                        .button = btn,
                        .action = btnAction,
                        .pos = {static_cast<float>(x), static_cast<float>(y)},
                    },
                    this_->m_inputModifiers,
                });
            });

        glfwSetCursorPosCallback(
            window, [](GLFWwindow *window, double x, double y) {
                const auto this_ = (Window *)glfwGetWindowUserPointer(window);
                this_->dispatchInputEvent(Input::Event{
                    Input::MouseMoveEvent{
                        .pos = {static_cast<float>(x), static_cast<float>(y)},
                    },
                    this_->currentInputModifiers(),
                });
            });

        BESS_INFO("[Window] Created GLFW window {}", m_title);
    }

    void Window::onPostInit() {
        syncFramebufferSize(true);

        const auto renderer = GAppContext::getInstance()
                                  .getSubSystem<RendererContext>()
                                  ->getRenderer();

        UI::UITargetDesc desc{
            .rect = {.size = {static_cast<float>(m_width),
                              static_cast<float>(m_height)}},
            .surface =
                {
                    .type = Core::Renderer::Renderer2DNativeSurfaceType::
                        PlatformHandle,
                    .handle = mp_window.get(),
                },
            .theme = Config::Themes::getCurrentTheme(),
            .platformServices =
                (m_uiPlatformServices =
                     std::make_shared<GLFWUIPlatformServices>(mp_window.get())),
        };

        m_uiTarget.init(renderer, desc);
        const std::weak_ptr<Window> weakWindow = weak_from_this();
        GAppContext::getInstance()
            .getSubSystem<EventSystem::EventDispatcher>()
            ->sink<Events::ThemeChangeEvent>()
            .connect([weakWindow](const Events::ThemeChangeEvent &event) {
                if (const auto window = weakWindow.lock();
                    window != nullptr && event.theme != nullptr) {
                    window->m_uiTarget.setTheme(*event.theme);
                }
            });
        // Temporary retained-UI integration showcase. The application can
        // replace this with its real root view through the same API.
        static_cast<void>(m_uiTarget.setContent<UI::UIDemoView>());
        m_uiTarget.enqueueEvent(UI::UITargetResizeEvent{
            .width = static_cast<uint32_t>(m_width),
            .height = static_cast<uint32_t>(m_height),
        });

        const auto mousePos = getMousePos();
        dispatchInputEvent(Input::Event{Input::MouseMoveEvent{.pos = mousePos},
                                        currentInputModifiers()});
    }

    void Window::onShutdown() {
        m_uiTarget.destroy();
        m_uiPlatformServices.reset();
    }

    void Window::onDestroy() {
        // Keep this path independently safe if a host skips onShutdown(). The
        // target releases its platform bridge before the GLFW handle becomes
        // invalid; destroy() is intentionally idempotent.
        m_uiTarget.destroy();
        m_uiPlatformServices.reset();

        if (!isGLFWInitialized)
            return;

        if (mp_window) {
            BESS_INFO("[Window] Destroying GLFW window {}", m_title);
            mp_window.reset();
        }

        for (auto &cursor : m_standardCursors) {
            cursor.reset();
        }
        m_cursorCreationFailed.fill(false);
        m_cursorShape = CursorIcon::arrow;

        BESS_INFO("[Window] Terminating GLFW");
        glfwTerminate();
        isGLFWInitialized = false;
    }

    void Window::onPostDraw() {
        if (!syncFramebufferSize(true)) {
            return;
        }

        m_uiTarget.draw();
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

    bool Window::syncFramebufferSize(bool notifyResizeEvent) {
        if (!mp_window) {
            return false;
        }

        int width = 0;
        int height = 0;
        glfwGetFramebufferSize(mp_window.get(), &width, &height);
        applyFramebufferSize(width, height, notifyResizeEvent);
        return width > 0 && height > 0;
    }

    void Window::applyFramebufferSize(int width,
                                      int height,
                                      bool notifyResizeEvent) {
        if (width <= 0 || height <= 0) {
            return;
        }

        const auto framebufferWidth = static_cast<size_t>(width);
        const auto framebufferHeight = static_cast<size_t>(height);
        const bool changed =
            m_width != framebufferWidth || m_height != framebufferHeight;
        const glm::vec2 targetSize{static_cast<float>(framebufferWidth),
                                   static_cast<float>(framebufferHeight)};
        const bool targetChanged = m_uiTarget.getRect().size != targetSize;

        if (!changed && !targetChanged) {
            return;
        }

        m_width = framebufferWidth;
        m_height = framebufferHeight;

        if (targetChanged) {
            m_uiTarget.resize(targetSize);
        }
        // Keep the routed resize notification independent from the immediate
        // target repair above. Consumers still receive the event if either
        // side was already synchronized by another integration path.
        m_uiTarget.enqueueEvent(UI::UITargetResizeEvent{
            .width = static_cast<uint32_t>(framebufferWidth),
            .height = static_cast<uint32_t>(framebufferHeight),
        });

        m_framebufferResized = true;
        if (!notifyResizeEvent || !changed) {
            return;
        }

        Events::WindowResizeEvent evt{
            static_cast<uint32_t>(framebufferWidth),
            static_cast<uint32_t>(framebufferHeight),
        };

        auto &ctx = GAppContext::getInstance();
        auto eventDispatcher = ctx.getSubSystem<EventSystem::EventDispatcher>();
        eventDispatcher->queue(evt);
    }

    void Window::framebufferResizeCallback(GLFWwindow *window,
                                           int width,
                                           int height) {
        const auto this_ =
            static_cast<Window *>(glfwGetWindowUserPointer(window));
        this_->applyFramebufferSize(width, height, true);
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

    void Window::setCursor(const CursorIcon shape) noexcept {
        if (!mp_window) {
            return;
        }

        CursorIcon resolvedShape = normalizedCursorShape(shape);
        if (resolvedShape == m_cursorShape) {
            return;
        }

        GLFWcursor *cursor = nullptr;
        if (resolvedShape != CursorIcon::arrow) {
            const auto index = static_cast<std::size_t>(resolvedShape);
            auto &cachedCursor = m_standardCursors[index];
            if (!cachedCursor && !m_cursorCreationFailed[index]) {
                cachedCursor.reset(
                    glfwCreateStandardCursor(glfwCursorShape(resolvedShape)));
                m_cursorCreationFailed[index] = !cachedCursor;
            }

            if (cachedCursor) {
                cursor = cachedCursor.get();
            } else {
                // A null GLFW cursor is the platform's default arrow. Cache
                // the failure so an unsupported shape does not trigger an
                // allocation attempt on every frame.
                resolvedShape = CursorIcon::arrow;
            }
        }

        if (resolvedShape == m_cursorShape) {
            return;
        }

        glfwSetCursor(mp_window.get(), cursor);
        m_cursorShape = resolvedShape;
    }

    void Window::dispatchInputEvent(Input::Event event) {
        auto inputSubSystem =
            GAppContext::getInstance().getSubSystem<InputSubSystem>();
        event = inputSubSystem->processEvent(std::move(event));

        std::visit(
            [this](auto &inputEvent) {
                using EventType = std::remove_cvref_t<decltype(inputEvent)>;
                if constexpr (std::same_as<EventType, Input::MouseMoveEvent> ||
                              std::same_as<EventType, Input::MouseWheelEvent> ||
                              std::same_as<EventType,
                                           Input::MouseButtonEvent>) {
                    inputEvent.pos =
                        windowToUITargetPos(inputEvent.pos.x, inputEvent.pos.y);
                }
            },
            event.data);

        m_uiTarget.enqueueEvent(std::move(event));
    }

    Input::Modifiers
    Window::glfwModifiersToInput(const int glfwModifiers) const {
        return {
            .control = (glfwModifiers & GLFW_MOD_CONTROL) != 0,
            .shift = (glfwModifiers & GLFW_MOD_SHIFT) != 0,
            .alt = (glfwModifiers & GLFW_MOD_ALT) != 0,
            .super = (glfwModifiers & GLFW_MOD_SUPER) != 0,
            .capsLock = (glfwModifiers & GLFW_MOD_CAPS_LOCK) != 0,
            .numLock = (glfwModifiers & GLFW_MOD_NUM_LOCK) != 0,
        };
    }

    Input::Modifiers Window::currentInputModifiers() const {
        auto modifiers = m_inputModifiers;
        const auto isPressed = [this](const int key) {
            return glfwGetKey(mp_window.get(), key) == GLFW_PRESS;
        };

        modifiers.control = isPressed(GLFW_KEY_LEFT_CONTROL) ||
                            isPressed(GLFW_KEY_RIGHT_CONTROL);
        modifiers.shift =
            isPressed(GLFW_KEY_LEFT_SHIFT) || isPressed(GLFW_KEY_RIGHT_SHIFT);
        modifiers.alt =
            isPressed(GLFW_KEY_LEFT_ALT) || isPressed(GLFW_KEY_RIGHT_ALT);
        modifiers.super =
            isPressed(GLFW_KEY_LEFT_SUPER) || isPressed(GLFW_KEY_RIGHT_SUPER);
        return modifiers;
    }

    glm::vec2 Window::windowToUITargetPos(const double x,
                                          const double y) const {
        int windowWidth = 0;
        int windowHeight = 0;
        int framebufferWidth = 0;
        int framebufferHeight = 0;
        glfwGetWindowSize(mp_window.get(), &windowWidth, &windowHeight);
        glfwGetFramebufferSize(
            mp_window.get(), &framebufferWidth, &framebufferHeight);

        const float scaleX = windowWidth > 0
                                 ? static_cast<float>(framebufferWidth) /
                                       static_cast<float>(windowWidth)
                                 : 1.f;
        const float scaleY = windowHeight > 0
                                 ? static_cast<float>(framebufferHeight) /
                                       static_cast<float>(windowHeight)
                                 : 1.f;
        const auto framebufferPos = glm::vec2{
            static_cast<float>(x) * scaleX,
            static_cast<float>(y) * scaleY,
        };
        return framebufferPos - m_uiTarget.getRect().pos;
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
