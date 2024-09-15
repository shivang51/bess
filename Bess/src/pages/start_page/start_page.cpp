#include "pages/start_page/start_page.h"
#include "application_state.h"
#include "pages/main_page/main_page.h"
#include "pages/page_identifier.h"
#include "ui/ui.h"

#include "imgui.h"
#include "imgui_internal.h"

namespace Bess::Pages {

    StartPage::StartPage() : Page(PageIdentifier::StartPage) {
    }

    std::shared_ptr<Page> StartPage::getInstance() {
        static std::shared_ptr<StartPage> instance = std::make_shared<StartPage>();
        return instance;
    }

    void StartPage::draw() {

        static bool firstTime = true;
        if (firstTime) {
            firstTime = false;

            auto mainDockspaceId = ImGui::GetID("MainDockspace");

            ImGui::DockBuilderRemoveNode(mainDockspaceId);
            ImGui::DockBuilderAddNode(mainDockspaceId, ImGuiDockNodeFlags_NoTabBar);

            ImGui::DockBuilderDockWindow("Application Title", mainDockspaceId);
            // ImGui::DockBuilderDockWindow("Menu", mainDockspaceId);

            ImGui::DockBuilderFinish(mainDockspaceId);
        }

        drawTitle();

        auto flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoDocking;

        ImGuiViewport *mainViewport = ImGui::GetMainViewport();

        ImGui::SetNextWindowPos(ImVec2(mainViewport->WorkPos.x + mainViewport->WorkSize.x / 2,
                                       mainViewport->WorkPos.y + mainViewport->WorkSize.y / 2),
                                ImGuiCond_Always, ImVec2(0.5f, 0.5f));
        ImGui::SetNextWindowViewport(mainViewport->ID);

        ImGui::SetNextWindowSizeConstraints(ImVec2(500, mainViewport->WorkSize.y * 0.7f), ImVec2(mainViewport->WorkSize.x, mainViewport->WorkSize.y * 0.7));

        if (ImGui::Begin("Menu", nullptr, flags)) {
            // Adjust window size dynamically if necessary
            ImVec2 windowSize = ImGui::GetWindowSize();
            ImGui::SetWindowFontScale(1.5);
            ImGui::Text("Previous Projects");
            ImGui::SetWindowFontScale(1.0);

            if (m_previousProjects.empty()) {
                std::string text = "No previous projects found";
                auto textSize = ImGui::CalcTextSize(text.c_str());
                ImGui::SetCursorPosX((windowSize.x - textSize.x) / 2);
                ImGui::SetCursorPosY((windowSize.y - textSize.y) / 2);
                ImGui::Text("No previous projects found");
            }

            ImGui::SetCursorPosY(windowSize.y - ImGui::GetFrameHeight() - 8.f);
            if (ImGui::Button("Continue with empty project")) {
                MainPage::getInstance(ApplicationState::getParentWindow())->show();
            }
        }

        ImGui::End();
    }

    void StartPage::update(const std::vector<ApplicationEvent> &events) {
    }

    void StartPage::drawTitle() {
        auto flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize;
        int width = 500, height = 200;

        ImGui::SetNextWindowSize(ImVec2(width, height));
        ImGui::SetCursorPos(ImVec2(32, 64));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
        ImGui::Begin("Application Title", nullptr, flags);
        ImGui::PushFont(UI::Fonts::largeFont);
        ImGui::Text("Welcome to Bess!");
        ImGui::PopFont();
        ImGui::PushFont(UI::Fonts::mediumFont);
        ImGui::Text("A Basic Electrical Simulation Software");
        ImGui::PopFont();
        ImGui::End();
        ImGui::PopStyleVar();
    }
} // namespace Bess::Pages
