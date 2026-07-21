#include "ui/ui.h"
#include "bess_core/g_app_context.h"
#include "bess_wgpu/wgpu_renderer_2d.h"
#include "common/logger.h"
#include "imgui_impl_wgpu.h"
#include "pages/main_page/main_page.h"
#include "sub_systems/renderer_context.h"
#include "bess_core/ui/icons/cod_icons.h"
#include "bess_core/ui/icons/component_icons.h"
#include "bess_core/ui/icons/font_awesome_icons.h"
#include "window.h"

#include "assets.h"
#include "bess_core/settings/settings.h"
#include "bess_core/ui/icons/material_icons.h"

#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "implot.h"

namespace Bess {

    void UIHandle::init(const std::shared_ptr<Window> &window) {
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

        ImGui_ImplGlfw_InitForOther(window->getGLFWHandle(), true);
        ImGui_ImplWGPU_InitInfo initInfo{};
        initInfo.Device = renderer2D->getDevice().Get();
        initInfo.NumFramesInFlight = 1;
        initInfo.RenderTargetFormat =
            static_cast<WGPUTextureFormat>(renderer2D->getSurfaceFormat());
        ImGui_ImplWGPU_Init(&initInfo);

        loadFontAndSetScale(settings->getFontSize(), settings->getScale());

        BESS_INFO("[UI] ImGui initialized successfully");

        m_currentPage = Pages::MainPage::getInstance(window);
    }

    void UIHandle::shutdown() {
        m_currentPage.reset();
        Pages::MainPage::getInstance().reset();

        BESS_INFO("[UI] Destroying");
        ImGui_ImplWGPU_Shutdown();
        ImGui_ImplGlfw_Shutdown();
        ImPlot::DestroyContext();
        ImGui::DestroyContext();
    }

    void UIHandle::begin() {
        const auto &settings =
            GAppContext::getInstance().getSubSystem<Config::Settings>();

        if (settings->shouldFontRebuild()) {
            loadFontAndSetScale(settings->getFontSize(), settings->getScale());
            settings->setFontRebuild(true);
        }

        ImGui_ImplWGPU_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        // if (g_window != nullptr) {
        //     int windowWidth = 0;
        //     int windowHeight = 0;
        //     int framebufferWidth = 0;
        //     int framebufferHeight = 0;
        //     glfwGetWindowSize(g_window, &windowWidth, &windowHeight);
        //     glfwGetFramebufferSize(g_window, &framebufferWidth,
        //                            &framebufferHeight);
        //     ImGuiIO &io = ImGui::GetIO();
        //     io.DisplaySize = ImVec2(static_cast<float>(framebufferWidth),
        //                             static_cast<float>(framebufferHeight));
        //     io.DisplayFramebufferScale = ImVec2(
        //         windowWidth > 0 ? static_cast<float>(framebufferWidth) /
        //                               static_cast<float>(windowWidth)
        //                         : 1.0f,
        //         windowHeight > 0 ? static_cast<float>(framebufferHeight) /
        //                                static_cast<float>(windowHeight)
        //                          : 1.0f);
        // }
        ImGui::NewFrame();

        switch (m_currentCursorType) {
        case CursorType::pointer:
            ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
            break;
        case CursorType::move:
            ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeAll);
            break;
        case CursorType::text:
            ImGui::SetMouseCursor(ImGuiMouseCursor_TextInput);
            break;
        case CursorType::resizeHorizontal:
            ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeEW);
            break;
        case CursorType::resizeVertical:
            ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeNS);
            break;
        case CursorType::resizeDiagonalNWSE:
            ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeNWSE);
            break;
        case CursorType::resizeDiagonalNESW:
            ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeNESW);
            break;
        case CursorType::normal:
            ImGui::SetMouseCursor(ImGuiMouseCursor_Arrow);
            break;
        }

        const ImGuiViewport *viewport = ImGui::GetMainViewport();

        ImGui::SetNextWindowPos(viewport->WorkPos);
        ImGui::SetNextWindowSize(viewport->WorkSize);
        ImGui::SetNextWindowViewport(viewport->ID);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0F);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0F);
        ImGuiWindowFlags windowFlags = ImGuiWindowFlags_NoDocking;
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

    void UIHandle::end() {
        ImGui::End();
        ImGui::Render();

        ImGui::UpdatePlatformWindows();
        ImGui::RenderPlatformWindowsDefault();
    }

    void UIHandle::draw() {
        const auto &appCtx = GAppContext::getInstance();
        const auto &settings = appCtx.getSubSystem<Config::Settings>();

        if (settings->getShowStatsWindow()) {
            drawStats(m_currentFps);
        }

        m_currentPage->draw();
    }

    ImFont *UIHandle::Fonts::largeFont = nullptr;
    ImFont *UIHandle::Fonts::mediumFont = nullptr;
    void UIHandle::loadFontAndSetScale(const float fontSize,
                                       const float scale) {
        ImGuiIO &io = ImGui::GetIO();

        constexpr auto robotoPath =
            Assets::Fonts::Paths::roboto.paths[0].data();

        io.Fonts->Clear();
        io.Fonts->AddFontFromFileTTF(robotoPath, fontSize);
        // Fonts::largeFont = io.Fonts->AddFontFromFileTTF(robotoPath,
        // fontSize
        // * 2.0F); Fonts::mediumFont =
        // io.Fonts->AddFontFromFileTTF(robotoPath, fontSize * 1.5F);
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
            UI::Icons::ComponentIcons::SIZE_MIN_CI,
            UI::Icons::ComponentIcons::SIZE_MAX_CI,
            0};
        io.Fonts->AddFontFromFileTTF(
            compIconsPath, fontSize * r, &config, compIconRanges.data());

        static const std::array<ImWchar, 3> codiconIconRanges = {
            UI::Icons::CodIcons::ICON_MIN_CI,
            UI::Icons::CodIcons::ICON_MAX_CI,
            0};
        config.GlyphOffset.y = fontSize / 5.0F;
        io.Fonts->AddFontFromFileTTF(
            codeIconsPath, fontSize, &config, codiconIconRanges.data());

        static const std::array<ImWchar, 3> faIconRangesR = {
            UI::Icons::FontAwesomeIcons::SIZE_MIN_FA,
            UI::Icons::FontAwesomeIcons::SIZE_MAX_FA,
            0};
        config.GlyphOffset.y = -r;
        io.Fonts->AddFontFromFileTTF(
            fontAwesomeIconsPath, fontSize * r, &config, faIconRangesR.data());

        config.GlyphOffset.y = r;
        static const std::array<ImWchar, 3> matIconRanges = {
            UI::Icons::MaterialIcons::ICON_MIN_MD,
            UI::Icons::MaterialIcons::ICON_MAX_MD,
            0};
        io.Fonts->AddFontFromFileTTF(
            materialIconsPath, fontSize * r, &config, matIconRanges.data());

        io.FontGlobalScale = scale;
    }

    void UIHandle::setCursorPointer() {
        m_currentCursorType = CursorType::pointer;
    }

    void UIHandle::setCursorMove() {
        m_currentCursorType = CursorType::move;
    }

    void UIHandle::setCursorText() {
        m_currentCursorType = CursorType::text;
    }

    void UIHandle::setCursorResizeHorizontal() {
        m_currentCursorType = CursorType::resizeHorizontal;
    }

    void UIHandle::setCursorResizeVertical() {
        m_currentCursorType = CursorType::resizeVertical;
    }

    void UIHandle::setCursorResizeDiagonalNWSE() {
        m_currentCursorType = CursorType::resizeDiagonalNWSE;
    }

    void UIHandle::setCursorResizeDiagonalNESW() {
        m_currentCursorType = CursorType::resizeDiagonalNESW;
    }

    void UIHandle::setCursorNormal() {
        m_currentCursorType = CursorType::normal;
    }

    void UIHandle::drawStats(const int fps) {
        ImGui::Begin(
            std::format("{}  Stats", UI::Icons::FontAwesomeIcons::FA_CHART_PIE)
                .c_str());
        ImGui::Text("FPS: %d", fps);
        ImGuiIO &io = ImGui::GetIO();
        ImGui::Text(
            "DisplaySize: %.1f x %.1f", io.DisplaySize.x, io.DisplaySize.y);
        ImGui::Text("FramebufferScale: %.2f x %.2f",
                    io.DisplayFramebufferScale.x,
                    io.DisplayFramebufferScale.y);
        ImGui::End();
    }

    void UIHandle::update(TimeMs dt) {
        m_currentPage->update(dt);
        m_currentFps = static_cast<int>(std::round(1000.0 / dt.count()));
    }

} // namespace Bess
