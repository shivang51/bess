#include "ui/ui.h"
#include "bess_core/g_app_context.h"
#include "bess_wgpu/wgpu_renderer_2d.h"
#include "common/logger.h"
#include "imgui_impl_wgpu.h"
#include "sub_systems/renderer_context.h"
#include "ui/icons/CodIcons.h"
#include "ui/icons/ComponentIcons.h"
#include "ui/icons/FontAwesomeIcons.h"

#include "application/assets.h"
#include "application/settings/settings.h"
#include "ui/icons/MaterialIcons.h"

#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "implot.h"
#include <vulkan/vulkan_core.h>

namespace Bess::UI {
    void init(GLFWwindow *window) {
        IMGUI_CHECKVERSION();
        ImGui::CreateContext();
        ImPlot::CreateContext();
        ImGuiIO &io = ImGui::GetIO();

        io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
        io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
        io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;

        io.IniFilename = "bess.ini";

        ImGui::StyleColorsDark();

        const auto &settings =
            GAppContext::getInstance().getSubSystem<Config::Settings>();
        settings->loadCurrentTheme();

        const auto &renderer2D = GAppContext::getInstance()
                                     .getSubSystem<RendererContext>()
                                     ->getRenderer<Wgpu::WgpuRenderer2D>();

        ImGui_ImplGlfw_InitForOther(window, true);
        ImGui_ImplWGPU_InitInfo initInfo{};
        initInfo.Device = renderer2D->getDevice().Get();
        initInfo.NumFramesInFlight = 1;
        initInfo.RenderTargetFormat = WGPUTextureFormat_BGRA8Unorm;
        ImGui_ImplWGPU_Init(&initInfo);

        loadFontAndSetScale(settings->getFontSize(), settings->getScale());

        BESS_INFO("[UI] ImGui initialized successfully");
    }

    void shutdown() {
        BESS_INFO("[UI] Destroying");
        ImGui_ImplGlfw_Shutdown();
        ImPlot::DestroyContext();
        ImGui::DestroyContext();
    }

    void begin() {
        const auto &settings =
            GAppContext::getInstance().getSubSystem<Config::Settings>();

        if (settings->shouldFontRebuild()) {
            loadFontAndSetScale(settings->getFontSize(), settings->getScale());
            settings->setFontRebuild(true);
        }

        ImGui_ImplWGPU_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        switch (currentCursorType) {
        case CursorType::pointer:
            ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
            break;
        case CursorType::move:
            ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeAll);
            break;
        case CursorType::normal:
            ImGui::SetMouseCursor(ImGuiMouseCursor_Arrow);
            break;
        }

        ImGuiWindowFlags windowFlags = ImGuiWindowFlags_NoDocking;
        const ImGuiViewport *viewport = ImGui::GetMainViewport();

        ImGui::SetNextWindowPos(viewport->WorkPos);
        ImGui::SetNextWindowSize(viewport->WorkSize);
        ImGui::SetNextWindowViewport(viewport->ID);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0F);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0F);
        windowFlags |= ImGuiWindowFlags_NoTitleBar |
                       ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize |
                       ImGuiWindowFlags_NoMove;
        windowFlags |= ImGuiWindowFlags_NoBringToFrontOnFocus |
                       ImGuiWindowFlags_NoNavFocus;

        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0F, 0.0F));
        ImGui::Begin("DockSpace", nullptr, windowFlags);
        ImGui::PopStyleVar(3);

        const auto mainDockspaceId = ImGui::GetID("MainDockspace");
        ImGui::DockSpace(mainDockspaceId);
    }

    void end() {
        ImGui::End();
        ImGui::Render();

        ImGui::UpdatePlatformWindows();
        ImGui::RenderPlatformWindowsDefault();
    }

    ImFont *Fonts::largeFont = nullptr;
    ImFont *Fonts::mediumFont = nullptr;
    void loadFontAndSetScale(const float fontSize, const float scale) {
        ImGuiIO &io = ImGui::GetIO();

        constexpr auto robotoPath =
            Assets::Fonts::Paths::roboto.paths[0].data();

        io.Fonts->Clear();
        io.Fonts->AddFontFromFileTTF(robotoPath, fontSize);
        // Fonts::largeFont = io.Fonts->AddFontFromFileTTF(robotoPath, fontSize
        // * 2.0F); Fonts::mediumFont = io.Fonts->AddFontFromFileTTF(robotoPath,
        // fontSize * 1.5F);
        io.FontDefault = io.Fonts->AddFontFromFileTTF(robotoPath, fontSize);

        ImFontConfig config;
        const float r = 2.2F / 3.0F;
        config.MergeMode = true;
        config.PixelSnapH = true;

        constexpr auto compIconsPath =
            Assets::Fonts::Paths::componentIcons.paths[0].data();
        constexpr auto codeIconsPath =
            Assets::Fonts::Paths::codeIcons.paths[0].data();
        constexpr auto materialIconsPath =
            Assets::Fonts::Paths::materialIcons.paths[0].data();
        constexpr auto fontAwesomeIconsPath =
            Assets::Fonts::Paths::fontAwesomeIcons.paths[0].data();

        static const std::array<ImWchar, 3> compIconRanges = {
            Icons::ComponentIcons::SIZE_MIN_CI,
            Icons::ComponentIcons::SIZE_MAX_CI, 0};
        io.Fonts->AddFontFromFileTTF(compIconsPath, fontSize * r, &config,
                                     compIconRanges.data());

        static const std::array<ImWchar, 3> codiconIconRanges = {
            Icons::CodIcons::ICON_MIN_CI, Icons::CodIcons::ICON_MAX_CI, 0};
        config.GlyphOffset.y = fontSize / 5.0F;
        io.Fonts->AddFontFromFileTTF(codeIconsPath, fontSize, &config,
                                     codiconIconRanges.data());

        static const std::array<ImWchar, 3> faIconRangesR = {
            Icons::FontAwesomeIcons::SIZE_MIN_FA,
            Icons::FontAwesomeIcons::SIZE_MAX_FA, 0};
        config.GlyphOffset.y = -r;
        io.Fonts->AddFontFromFileTTF(fontAwesomeIconsPath, fontSize * r,
                                     &config, faIconRangesR.data());

        config.GlyphOffset.y = r;
        static const std::array<ImWchar, 3> matIconRanges = {
            Icons::MaterialIcons::ICON_MIN_MD,
            Icons::MaterialIcons::ICON_MAX_MD, 0};
        io.Fonts->AddFontFromFileTTF(materialIconsPath, fontSize * r, &config,
                                     matIconRanges.data());

        io.FontGlobalScale = scale;
    }

    void setCursorPointer() { currentCursorType = CursorType::pointer; }

    void setCursorMove() { currentCursorType = CursorType::move; }

    void setCursorNormal() { currentCursorType = CursorType::normal; }

    void drawStats(const int fps) {
        ImGui::Begin(
            std::format("{}  Stats", Icons::FontAwesomeIcons::FA_CHART_PIE)
                .c_str());
        ImGui::Text("FPS: %d", fps);
        ImGuiIO &io = ImGui::GetIO();
        ImGui::Text("DisplaySize: %.1f x %.1f", io.DisplaySize.x,
                    io.DisplaySize.y);
        ImGui::Text("FramebufferScale: %.2f x %.2f",
                    io.DisplayFramebufferScale.x, io.DisplayFramebufferScale.y);
        ImGui::End();
    }

} // namespace Bess::UI
