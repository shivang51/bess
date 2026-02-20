#include "log_window.h"
#include "common/logger.h"
#include "imgui.h"
#include "imgui_internal.h"
#include "ui/widgets/m_widgets.h"

namespace Bess::UI {
    bool LogWindow::isShown = false;

    LogWindow::Controls LogWindow::m_controls{};

    void LogWindow::drawControls() {
        const auto &uiLogSink = Logger::getUISink();

        Widgets::CheckboxWithLabel("Auto Scroll ", &m_controls.autoScroll, false, true);

        ImGui::SameLine();
        ImGui::AlignTextToFramePadding();
        ImGui::SeparatorEx(ImGuiSeparatorFlags_Vertical);
        ImGui::SameLine();

        if (ImGui::Button("Clear")) {
            std::lock_guard<std::mutex> lock(uiLogSink->bufferMutex);
            uiLogSink->logs.clear();
        }
    }

    void LogWindow::draw() {
        if (!isShown)
            return;

        const auto &uiLogSink = Logger::getUISink();
        ImGui::Begin(windowName.data());
        if (uiLogSink) {
            drawControls();
            ImGui::Separator();

            ImGui::BeginChild("scrolling");
            {
                std::lock_guard<std::mutex> lock(uiLogSink->bufferMutex);
                for (const auto &msg : uiLogSink->logs) {
                    ImGui::TextUnformatted(msg.message.c_str());
                }
                if (m_controls.autoScroll &&
                    ImGui::GetScrollY() >= ImGui::GetScrollMaxY()) {
                    ImGui::SetScrollHereY(1.0f);
                }
            }
            ImGui::EndChild();
        }
        ImGui::End();
    }
} // namespace Bess::UI
