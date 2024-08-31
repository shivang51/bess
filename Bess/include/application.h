#pragma once

#include "events/application_event.h"
#include "window.h"
#include <vector>

namespace Bess {

    class Application {
      public:
        Application();
        Application(const std::string &projectPath);
        ~Application();

        void run();
        void quit();

        void update();

        void loadProject(const std::string &path);
        void saveProject();

      private:
        Window m_mainWindow;
        std::vector<ApplicationEvent> m_events;

      private:
        void draw();
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
    };
} // namespace Bess
