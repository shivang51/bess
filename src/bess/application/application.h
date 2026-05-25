#pragma once

#include "application/window.h"
#include "common/types.h"
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

        void init(const std::string &path,
                  AppStartupFlags flags = AppStartupFlag::none);
        void run();
        void quit() const;
        void shutdown();

      private:
        std::shared_ptr<Window> m_mainWindow;
    };
} // namespace Bess
