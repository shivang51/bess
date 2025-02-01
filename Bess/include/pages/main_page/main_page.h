#pragma once
#include "include/core/SkImage.h"
#include "include/gpu/ganesh/GrBackendSurface.h"
#include "include/gpu/ganesh/GrDirectContext.h"
#define Sk_GL
#include "camera.h"
#include "events/application_event.h"
#include "include/core/SkCanvas.h"
#include "include/core/SkRefCnt.h"
#include "include/core/SkSurface.h"
#include "include/gpu/ganesh/gl/GrGLTypes.h"
#include "pages/main_page/main_page_state.h"
#include "pages/page.h"
#include "scene/renderer/gl/framebuffer.h"
#include "scene/renderer/gl/texture.h"
#include "window.h"

#include <memory>
#include <vector>

namespace Bess::Pages {
    class MainPage : public Page {
      public:
        MainPage(std::shared_ptr<Window> parentWindow);

        static std::shared_ptr<Page> getInstance(const std::shared_ptr<Window> &parentWindow = nullptr);

        static std::shared_ptr<MainPage> getTypedInstance(std::shared_ptr<Window> parentWindow = nullptr);

        void draw() override;

        void update(const std::vector<ApplicationEvent> &events) override;

        void drawScene();
        void drawSkiaScene();

        glm::vec2 getCameraPos();

        std::shared_ptr<Window> getParentWindow();

      private:
        std::shared_ptr<Camera> m_camera;
        std::unique_ptr<Gl::FrameBuffer> m_normalFramebuffer;
        std::shared_ptr<Window> m_parentWindow;
        sk_sp<SkSurface> skiaSurface = nullptr;
        sk_sp<GrDirectContext> skiaContext = nullptr;
        void skiaInit(int width, int height);
        GrBackendTexture skiaBackendTexture;
        // event handlers
      private:
        void onMouseWheel(double x, double y);
        void onLeftMouse(bool pressed);
        void onRightMouse(bool pressed);
        void onMiddleMouse(bool pressed);
        void onMouseMove(double x, double y);

        bool isCursorInViewport();
        void finishDragging();

        glm::vec2 getViewportMousePos();
        glm::vec2 getNVPMousePos();

      private:
        bool m_leftMousePressed = false;
        bool m_rightMousePressed = false;
        bool m_middleMousePressed = false;

        std::chrono::time_point<std::chrono::steady_clock> m_lastUpdateTime;

        std::shared_ptr<MainPageState> m_state;
    };
} // namespace Bess::Pages
