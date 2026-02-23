#pragma once

#include "application/app_types.h"
#include "application/window.h"
#include "events/application_event.h"
#include <memory>
#include <vector>

namespace Bess {

    typedef uint8_t AppStartupFlags;

    enum AppStartupFlag : AppStartupFlags {
        none = 0,
        disablePlugins = 1 << 0,
    };

    class Application {
      public:
        Application();
        ~Application();

        void init(const std::string &path, AppStartupFlags flags = AppStartupFlag::none);
        void run();
        void quit() const;
        void shutdown();

        void update(TFrameTime ts);

        void loadProject(const std::string &path) const;
        void saveProject() const;

      private:
        std::shared_ptr<Window> m_mainWindow;
        std::vector<ApplicationEvent> m_events;

      private:
        void draw();

      private: // callbacks
        void onWindowResize(int w, int h);
        void onMouseWheel(double x, double y);
        void onKeyPress(int key);
        void onKeyRelease(int key);
        void onMouseButton(MouseButton button, MouseButtonAction action);
        void onMouseMove(double x, double y);

        int m_currentFps = 0;
    };
} // namespace Bess
