#include "log_window.h"
#include "common/helpers.h"
#include "common/logger.h"
#include "imgui.h"
#include "imgui_internal.h"
#include "ui/icons/CodIcons.h"
#include "ui/widgets/m_widgets.h"

namespace Bess::UI {
    static constexpr auto windowName = Common::Helpers::concat(
        Icons::CodIcons::HISTORY, "  Log Window");

    LogWindow::LogWindow() : Panel(std::string(windowName.data())) {
    }

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

    void LogWindow::onDraw() {
        const ImGuiContext &g = *ImGui::GetCurrentContext();
        const auto &uiLogSink = Logger::getUISink();

        static constexpr ImGuiTableFlags tableFlags = ImGuiTableFlags_ScrollY |
                                                      ImGuiTableFlags_ScrollX | ImGuiTableFlags_RowBg;

        if (uiLogSink) {
            drawControls();

            ImGui::Separator();

            if (ImGui::BeginTable("##logTable", 1, tableFlags)) {
                std::lock_guard<std::mutex> lock(uiLogSink->bufferMutex);

                for (const auto &log : uiLogSink->logs) {
                    ImGui::TableNextRow();
                    ImGui::TableNextColumn();
                    ImVec4 textColor = ImGui::GetStyleColorVec4(ImGuiCol_Text);
                    switch (log.level) {
                    case LogLevel::trace:
                        textColor = ImVec4(0.5f, 0.5f, 0.5f, 1.0f);
                        break;
                    case LogLevel::debug:
                        textColor = ImVec4(0.45f, 0.45f, 0.90f, 1.0f);
                        break;
                    case LogLevel::warn:
                        textColor = ImVec4(1.00f, 0.80f, 0.40f, 1.00f);
                        break;
                    case LogLevel::error:
                        textColor = ImVec4(0.90f, 0.45f, 0.45f, 1.00f);
                        break;
                    case LogLevel::critical:
                        textColor = ImVec4(1.00f, 0.33f, 0.33f, 1.00f);
                        break;
                    default:
                        break;
                    }
                    ImGui::PushStyleColor(ImGuiCol_Text, textColor);
                    ImGui::TextUnformatted(log.message.c_str());
                    ImGui::PopStyleColor();
                }

                if (m_controls.autoScroll &&
                    ImGui::GetScrollY() >= ImGui::GetScrollMaxY()) {
                    ImGui::SetScrollHereY(1.0f);
                }

                ImGui::EndTable();
            }
        }
    }

} // namespace Bess::UI
