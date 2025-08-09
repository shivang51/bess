#include "ui/ui.h"
#include "application_state.h"
#include "settings/settings.h"
#include "ui/icons/CodIcons.h"
#include "ui/icons/ComponentIcons.h"
#include "ui/icons/FontAwesomeIcons.h"
#include "ui/icons/MaterialIcons.h"
#include "ui/ui_main/ui_main.h"
#include "assets.h"

#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"
#include "imgui_internal.h"

namespace Bess::UI {
    void init(GLFWwindow *window) {
        IMGUI_CHECKVERSION();
        ImGui::CreateContext();
        ImGuiIO &io = ImGui::GetIO();

        io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard; // Enable Keyboard Controls
        io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;     // Enable Docking
        io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;   // Enable

        io.IniFilename = "bess.ini";

        ImGui::StyleColorsDark();
        ImGuiStyle &style = ImGui::GetStyle();
        if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable) {
            style.WindowRounding = 0.0f;
            style.Colors[ImGuiCol_WindowBg].w = 1.0f;
        }

        loadFontAndSetScale(Config::Settings::getFontSize(), Config::Settings::getScale());
        Config::Settings::loadCurrentTheme();

        ImGui_ImplGlfw_InitForOpenGL(window, true);
        ImGui_ImplOpenGL3_Init("#version 410");
    }

    void shutdown() {
        ImGui_ImplOpenGL3_Shutdown();
        ImGui_ImplGlfw_Shutdown();
        ImGui::DestroyContext();
    }

    void begin() {
        if (Config::Settings::shouldFontRebuild()) {
            loadFontAndSetScale(Config::Settings::getFontSize(),
                                Config::Settings::getScale());
            Config::Settings::setFontRebuild(true);
        }

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();
        ImGuiWindowFlags window_flags = ImGuiWindowFlags_NoDocking;
        const ImGuiViewport *viewport = ImGui::GetMainViewport();

        ImGui::SetNextWindowPos(viewport->WorkPos);
        ImGui::SetNextWindowSize(viewport->WorkSize);
        ImGui::SetNextWindowViewport(viewport->ID);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
        window_flags |= ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse |
                        ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove;
        window_flags |=
            ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus;

        static bool p_open = true;
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
        ImGui::Begin("DockSpace", nullptr, window_flags);
        ImGui::PopStyleVar(3);

        auto mainDockspaceId = ImGui::GetID("MainDockspace");
        ImGui::DockSpace(mainDockspaceId);
    }

    void end() {
        ImGui::End();
        ImGuiIO &io = ImGui::GetIO();
        ImGui::Render();
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable) {
            GLFWwindow *backup_current_context = glfwGetCurrentContext();
            ImGui::UpdatePlatformWindows();
            ImGui::RenderPlatformWindowsDefault();
            glfwMakeContextCurrent(backup_current_context);
        }
    }

    ImFont *Fonts::largeFont = nullptr;
    ImFont *Fonts::mediumFont = nullptr;
    void loadFontAndSetScale(float fontSize, float scale) {
        ImGuiIO &io = ImGui::GetIO();
        ImGuiStyle &style = ImGui::GetStyle();

        const auto &robotoPath = Assets::Fonts::Paths::roboto.paths[0];

        io.Fonts->Clear();
        io.Fonts->AddFontFromFileTTF("assets/fonts/Roboto/Roboto-Regular.ttf", fontSize);
        Fonts::largeFont = io.Fonts->AddFontFromFileTTF("assets/fonts/Roboto/Roboto-Regular.ttf", fontSize * 2.f);
        Fonts::mediumFont = io.Fonts->AddFontFromFileTTF("assets/fonts/Roboto/Roboto-Regular.ttf", fontSize * 1.5f);
        io.FontDefault = io.Fonts->AddFontFromFileTTF("assets/fonts/Roboto/Roboto-Regular.ttf", fontSize);

        ImFontConfig config;
        float r = 2.2f / 3.f;
        config.MergeMode = true;
        config.PixelSnapH = true;

        static const ImWchar comp_icon_ranges[] = {Icons::ComponentIcons::SIZE_MIN_CI, Icons::ComponentIcons::SIZE_MAX_CI, 0};
        io.Fonts->AddFontFromFileTTF("assets/icons/ComponentIcons.ttf", fontSize * r, &config, comp_icon_ranges);

        static const ImWchar codicon_icon_ranges[] = {Icons::CodIcons::ICON_MIN_CI, Icons::CodIcons::ICON_MAX_CI, 0};
        config.GlyphOffset.y = fontSize / 5.f;
        io.Fonts->AddFontFromFileTTF("assets/icons/codicon.ttf", fontSize, &config, codicon_icon_ranges);

        config.GlyphOffset.y = r;
        static const ImWchar mat_icon_ranges[] = {Icons::MaterialIcons::ICON_MIN_MD, Icons::MaterialIcons::ICON_MAX_MD, 0};
        io.Fonts->AddFontFromFileTTF("assets/icons/MaterialIcons-Regular.ttf", fontSize * r, &config, mat_icon_ranges);

        static const ImWchar fa_icon_ranges_r[] = {Icons::FontAwesomeIcons::SIZE_MIN_FA, Icons::FontAwesomeIcons::SIZE_MAX_FA, 0};
        config.GlyphOffset.y = -r;
        io.Fonts->AddFontFromFileTTF("assets/icons/fa-solid-900.ttf", fontSize * r, &config, fa_icon_ranges_r);

        io.FontGlobalScale = scale;
        io.Fonts->Build();

        if (Config::Settings::shouldFontRebuild()) {
            ImGui_ImplOpenGL3_DestroyDeviceObjects();
            ImGui_ImplOpenGL3_CreateDeviceObjects();
        }
    }

    void setCursorPointer() {
        ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
    }

    void setCursorMove() {
        ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeAll);
    }

    void setCursorNormal() { ImGui::SetMouseCursor(ImGuiMouseCursor_Arrow); }

    void drawStats(int fps) {
        ImGui::Begin("Stats");
        ImGui::Text("FPS: %d", fps);
        switch (ApplicationState::getCurrentPage()->getIdentifier()) {
        case Pages::PageIdentifier::MainPage:
            UI::UIMain::drawStats(fps);
            break;
        case Pages::PageIdentifier::StartPage:
        default:
            break;
        }

        ImGui::End();
    }

} // namespace Bess::UI
