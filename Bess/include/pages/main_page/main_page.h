#pragma once

#include "camera.h"
#include "events/application_event.h"
#include "pages/page.h"
#include "renderer/gl/framebuffer.h"

#include <memory>
#include <vector>

namespace Bess::Pages {
    class MainPage : public Page {
      public:
        MainPage();

        static std::shared_ptr<Page> getInstance();

        void draw() override;
        void update(const std::vector<ApplicationEvent> &events) override;

        void drawScene();

      private:
        std::shared_ptr<Camera> m_camera;
        std::unique_ptr<Gl::FrameBuffer> m_framebuffer;

        // event handlers
      private:
        void onMouseWheel(double x, double y);
        void onLeftMouse(bool pressed);
        void onRightMouse(bool pressed);
        void onMiddleMouse(bool pressed);
        void onMouseMove(double x, double y);

        static bool isCursorInViewport();
        static glm::vec2 getViewportMousePos();
        static glm::vec2 getNVPMousePos(const glm::vec2 &cameraPos);

      private:
        bool m_leftMousePressed = false;
        bool m_rightMousePressed = false;
        bool m_middleMousePressed = false;
    };
} // namespace Bess::Pages
