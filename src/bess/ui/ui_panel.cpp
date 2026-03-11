#include "ui_panel.h"

namespace Bess::UI {
    Panel::Panel(const std::string &name)
        : m_name(name) {}

    void Panel::hide() {
        if (m_visible) {
            m_visible = false;
            onHide();
        }
    }

    void Panel::show() {
        if (!m_visible) {
            m_visible = true;
            onShow();
        }
    }

    void Panel::toggleVisibility() {
        if (getVisible()) {
            hide();
        } else {
            show();
        }
    }

    void Panel::render() {
        onBeforeDraw();
        ImGui::Begin(m_name.c_str(), &m_visible, m_flags);
        onDraw();
        m_isFocused = ImGui::IsWindowFocused(
            ImGuiFocusedFlags_RootAndChildWindows);
        m_isHovered = ImGui::IsWindowHovered(
            ImGuiHoveredFlags_RootAndChildWindows);
        ImGui::End();
        onAfterDraw();
    }

    void Panel::onShow() {}

    void Panel::onHide() {}

    void Panel::update(TimeMs ts, const std::vector<ApplicationEvent> &events) {}

    void Panel::init() {}

    void Panel::destroy() {}

} // namespace Bess::UI
