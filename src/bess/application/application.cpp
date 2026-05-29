#include "application.h"
#include "asset_manager/asset_manager.h"
#include "bess_core/g_app_context.h"
#include "bess_core/project_context.h"
#include "common/bess_assert.h"
#include "common/logger.h"
#include "common/types.h"
#include "event_dispatcher.h"
#include "services/plugin_service/plugin_service.h"
#include "sub_systems/input_sub_system.h"
#include "sub_systems/renderer_context.h"
#include "ui/ui_sub_system.h"
#include <chrono>
#include <vulkan/vulkan.h>
#include <vulkan/vulkan_core.h>

#include "application/window.h"
#include "settings/settings.h"

namespace Bess {
    Application::Application() = default;

    Application::~Application() { shutdown(); }

    void Application::run() {
        // Bess::Wgpu::WgpuRenderer2D renderer2D;
        // Core::Renderer::Renderer2DNativeSurfaceType surfaceType =
        //     Core::Renderer::Renderer2DNativeSurfaceType::BackendOwned;
        //
        // renderer2D.init(
        //     {.extent = {800, 600},
        //      .targetFormat =
        //      Core::Renderer::Renderer2DTargetFormat::BGRA8Unorm, .surface =
        //      {.type = surfaceType, .handle = nullptr}});
        //
        // IMGUI_CHECKVERSION();
        // ImGui::CreateContext();
        // ImGuiIO &io = ImGui::GetIO();
        //
        // io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
        // io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
        // io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;
        // io.IniFilename = "bess.ini";
        //
        // ImGui::StyleColorsDark();
        //
        // // Setup Platform/Renderer backends
        // ImGui_ImplGlfw_InitForOther(m_mainWindow->getGLFWHandle(), true);
        // ImGui_ImplWGPU_InitInfo initInfo{};
        // initInfo.Device = renderer2D.getDevice().Get();
        // initInfo.NumFramesInFlight = 3;
        // initInfo.RenderTargetFormat = WGPUTextureFormat_BGRA8Unorm;
        // ImGui_ImplWGPU_Init(&initInfo);
        //
        // auto tex = Wgpu::WgpuTexture(renderer2D,
        //                              "assets/images/7-seg-display-tilemap.png");
        // tex.init();
        // renderer2D.beginFrame({.extent = {800, 600},
        //                        .clearColor = {0.1f, 0.2f, 0.3f, 1.0f},
        //                        .shouldClear = true});
        //
        // Core::Renderer::QuadProps quadProps;
        // quadProps.position = {100.f, 100.f};
        // quadProps.size = {200.f, 150.f};
        // quadProps.color = Bess::Core::Renderer::Color{1.f, 0.f, 0.f, 1.f};
        // quadProps.rotation = 45.f;
        // renderer2D.drawQuad({.position = {100.f, 100.f},
        //                      .size = {200.f, 150.f},
        //                      .color = {1.f, 0.f, 0.f, 1.f}});
        //
        // renderer2D.endFrame();
        //
        // renderer2D.beginFrame({.extent = {800, 600}, .shouldClear = false});
        //
        // renderer2D.drawQuad({.position = {300.f, 100.f},
        //                      .size = {200.f, 150.f},
        //                      .color = {1.f, 1.f, 1.f, 1.f},
        //                      .texture = tex.getHandle()});
        // renderer2D.endFrame();
        //
        // ImGui_ImplWGPU_NewFrame();
        // ImGui_ImplGlfw_NewFrame();
        // io.DisplaySize = ImVec2(800.0f, 600.0f);
        // ImGui::NewFrame();
        //
        // static bool pOpen = true;
        // ImGui::ShowDemoWindow(&pOpen);
        //
        // ImGui::Render();
        //
        // renderer2D.drawImGui([&](void *renderPass) {
        //     ImGui_ImplWGPU_RenderDrawData(ImGui::GetDrawData(),
        //                                   ((WGPURenderPassEncoder)renderPass));
        // });
        //
        // ImGui_ImplGlfw_Shutdown();
        // ImGui_ImplWGPU_Shutdown();
        //
        // // const auto target = renderer2D.getCurrentTargetView().Get();
        // renderer2D.saveTargetToFile("output.png");
        //
        // renderer2D.destroy();
        // return;

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

            accumulatedTime = std::chrono::duration<double>(0.0);
        }
    }

    void Application::quit() const { m_mainWindow->close(); }

    void Application::init(const std::string &path, AppStartupFlags flags) {
#ifdef DISABLE_PLUGINS
        flags |= AppStartupFlag::disablePlugins;
#endif
        BESS_INFO(
            "[Application] Initializing application, with project path: {}",
            path.empty() ? "None" : path);

        auto &appCtx = GAppContext::getInstance();

        appCtx.addSubSystem<Config::Settings>();
        appCtx.addSubSystem<InputSubSystem>();
        appCtx.addSubSystem<EventSystem::EventDispatcher>();
        m_mainWindow = appCtx.addSubSystem<Window>(800, 660, "Bess");
        appCtx.addSubSystem<RendererContext>();
        appCtx.addSubSystem<Assets::AssetManager>();
        appCtx.addSubSystem<UISubSystem>();

        if (flags & AppStartupFlag::disablePlugins) {
            BESS_WARN("[Application] Plugin support is disabled");
        } else {
            appCtx.addSubSystem<Svc::PluginService>();
        }

        auto projCtx = appCtx.addSubSystem<ProjectContext>();

        appCtx.init();

        if (!path.empty()) {
            projCtx->loadProject(path);
        } else {
            projCtx->createNewProject();
        }

        BESS_INFO("[Application] Application initialized successfully\n");
    }

    void Application::shutdown() {
        BESS_INFO("[Application] Shutting down application");

        GAppContext::getInstance().destroy();

        BESS_INFO("[Application] Application shutdown complete");
    }
} // namespace Bess
