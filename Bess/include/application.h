#pragma once

#include "camera.h"
#include "fwd.hpp"
#include "renderer/gl/framebuffer.h"
#include "window.h"
#include <memory>
#include "application_state.h"
#include "components/component.h"

namespace Bess
{

    class Application
    {
    public:
        Application();
        Application(const std::string& projectPath);
        ~Application();

        void run();
        void quit();

        void update();

        void loadProject(const std::string& path);
        void saveProject();

    private:
        Window m_mainWindow;
        std::unique_ptr<Gl::FrameBuffer> m_framebuffer;
        std::shared_ptr<Camera> m_camera;

    private:
        void drawUI();
        void drawScene();

        bool isCursorInViewport() const;
        glm::vec2 getViewportMousePos() const;
        glm::vec2 getNVPMousePos();

        void init();
        void shutdown();

        
    private: // callbacks
        void onWindowResize(int w, int h);
        void onMouseWheel(double x, double y);
        void onKeyPress(int key);
        void onKeyRelease(int key);
        void onLeftMouse(bool pressed);
        void onRightMouse(bool pressed);
        void onMiddleMouse(bool pressed);
        void onMouseMove(double x, double y);

    private:
        bool m_leftMousePressed = false;
        bool m_rightMousePressed = false;
        bool m_middleMousePressed = false;

        glm::vec2 m_mousePos = { 0, 0 };
    };
} // namespace Bess
