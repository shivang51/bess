#pragma once

#include "camera.h"
#include "events/application_event.h"
#include "pages/main_page/main_page_state.h"
#include "pages/page.h"
#include "renderer/gl/framebuffer.h"
#include "window.h"

#include <memory>
#include <vector>

namespace Bess::Pages {
    class MainPage : public Page {
      public:
        MainPage(std::shared_ptr<Window> parentWindow);

        static std::shared_ptr<Page> getInstance(const std::shared_ptr<Window>& parentWindow = nullptr);

        static std::shared_ptr<MainPage> getTypedInstance(std::shared_ptr<Window> parentWindow = nullptr);

        void draw() override;

        void update(const std::vector<ApplicationEvent> &events) override;

        void drawScene();

        glm::vec2 getCameraPos();

        std::shared_ptr<Window> getParentWindow();

      private:
        std::shared_ptr<Camera> m_camera;
        std::unique_ptr<Gl::FrameBuffer> m_multiSampledFramebuffer, m_normalFramebuffer;
        std::shared_ptr<Window> m_parentWindow;

        // event handlers
      private:
        void onMouseWheel(double x, double y);
        void onLeftMouse(bool pressed);
        void onRightMouse(bool pressed);
        void onMiddleMouse(bool pressed);
        void onMouseMove(double x, double y);

        bool isCursorInViewport();
        glm::vec2 getViewportMousePos();
        glm::vec2 getNVPMousePos();

      private:
        bool m_leftMousePressed = false;
        bool m_rightMousePressed = false;
        bool m_middleMousePressed = false;

        std::shared_ptr<MainPageState> m_state;
    };
} // namespace Bess::Pages
