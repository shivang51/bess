#include "application.h"
#include "bess_core/animator/animator.h"
#include "bess_core/asset_manager/asset_manager.h"
#include "pages/main_page/services/copy_paste_service.h"
#include "bess_core/g_app_context.h"
#include "bess_core/sub_systems/input_sub_system.h"
#include "common/bess_assert.h"
#include "common/logger.h"
#include "common/types.h"
#include "event_dispatcher.h"
#include "math_sim_driver.h"
#include "pages/main_page/services/connection_service.h"
#include "pages/main_page/project_model.h"
#include "project_session/project_session.h"
#include "services/plugin_service/plugin_service.h"
#include "services/window_drop_service/window_drop_service.h"
#include "sub_systems/renderer_context.h"
#include <chrono>
#include <ratio>

#include "bess_core/settings/settings.h"

namespace Bess {
    Application::Application() = default;

    Application::~Application() {
        shutdown();
    }

    void Application::run() {
        BESS_ASSERT(m_mainWindow, "Main window is not initialized or set");

        auto previousTime = std::chrono::steady_clock::now();

        TimeMs accumulatedTime(0.0);

        auto &appCtx = GAppContext::getInstance();
        const auto &settings = appCtx.getSubSystem<Config::Settings>();

        while (!m_mainWindow->isClosed()) {
            auto currentTime = std::chrono::steady_clock::now();
            TimeMs deltaTime = currentTime - previousTime;
            previousTime = currentTime;

            accumulatedTime += deltaTime;

            const auto &frameTS = settings->getFrameTimeStep();

            if (accumulatedTime < frameTS) {
                std::this_thread::sleep_for(frameTS - accumulatedTime);
                accumulatedTime += frameTS - accumulatedTime;
            }

            appCtx.beginFrame();

            appCtx.preUpdate();
            appCtx.update(accumulatedTime);

            appCtx.preDraw();
            appCtx.draw();

            appCtx.postDraw();

            appCtx.endFrame();

            accumulatedTime = std::chrono::duration<double, std::milli>(0.0);
        }
    }

    void Application::quit() const {
        m_mainWindow->close();
    }

    void Application::init(const std::string &path, AppStartupFlags flags) {
#ifdef DISABLE_PLUGINS
        flags |= AppStartupFlag::disablePlugins;
#endif
        BESS_INFO(
            "[Application] Initializing application, with project path: {}",
            path.empty() ? "None" : path);

        SimEngine::Drivers::Math::registerMathSimDriver();

        auto &appCtx = GAppContext::getInstance();

        appCtx.addSubSystem<Config::Settings>();
        appCtx.addSubSystem<InputSubSystem>();
        appCtx.addSubSystem<EventSystem::EventDispatcher>();
        appCtx.addSubSystem<Svc::WindowDropService>();
        m_mainWindow = appCtx.addSubSystem<Window>(800, 660, "Bess");
        appCtx.addSubSystem<RendererContext>();
        appCtx.addSubSystem<Assets::AssetManager>();
        appCtx.addSubSystem<Core::Animator>();

        if (flags & AppStartupFlag::disablePlugins) {
            BESS_WARN("[Application] Plugin support is disabled");
        } else {
            appCtx.addSubSystem<Svc::PluginService>();
        }

        auto session = appCtx.addSubSystem<ProjectSession>();
        session->addSubSystem<Svc::CopyPaste::Context>();
        session->addSubSystem<Svc::SvcConnection>();
        Pages::initProjectModel(*session);

        appCtx.init();

        if (!path.empty()) {
            const auto status = session->load(path);
            if (!status) {
                BESS_ERROR("Could not open startup project: {}",
                           status.msg());
            }
        } else {
            const auto status = session->newProj();
            if (!status) {
                BESS_ERROR("Could not create startup project: {}",
                           status.msg());
            }
        }

        BESS_INFO("[Application] Application initialized successfully\n");
    }

    void Application::shutdown() {
        BESS_INFO("[Application] Shutting down application");

        GAppContext::getInstance().destroy();

        BESS_INFO("[Application] Application shutdown complete");
    }
} // namespace Bess
