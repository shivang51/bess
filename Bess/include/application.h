#pragma once

#include "events/application_event.h"
#include "types.h"
#include "window.h"
#include <chrono>
#include <memory>
#include <vector>

namespace Bess {

    class Application {
      public:
        Application() = default;
        ~Application();

        void init(const std::string &path);
        void run();
        void quit();

        void update(TFrameTime ts);

        void loadProject(const std::string &path);
        void saveProject();

      private:
        std::shared_ptr<Window> m_mainWindow;
        std::vector<ApplicationEvent> m_events;
        std::chrono::steady_clock m_clock;

      private:
        void draw();
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
    };
} // namespace Bess
