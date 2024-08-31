#include "pages/start_page/start_page.h"
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

            ImGui::DockBuilderFinish(mainDockspaceId);
        }

        drawTitle();

        auto disSize = ImGui::GetIO().DisplaySize;
        int width = 500, height = std::max(disSize.y * 0.6, 300.0);

        float x = (disSize.x - width) / 2;
        float y = std::max((disSize.y - height) / 2, 280.f);

        auto flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize;

        ImGui::SetNextWindowSize(ImVec2(width, height));
        ImGui::SetNextWindowPos(ImVec2(x, y));
        ImGui::Begin("Menu", nullptr, flags);
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
            MainPage::getInstance()->show();
        }
        ImGui::End();
    }

    void StartPage::update(const std::vector<ApplicationEvent> &events) {
    }

    void StartPage::drawTitle() {
        auto flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize;
        int width = 500, height = 200;

        ImGui::SetNextWindowSize(ImVec2(width, height));
        ImGui::SetNextWindowPos(ImVec2(32, 64));
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
