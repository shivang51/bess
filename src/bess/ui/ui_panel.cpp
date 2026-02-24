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

    void Panel::onShow() {}

    void Panel::onHide() {}

    void Panel::update() {}

    void Panel::init() {}

    void Panel::destroy() {}
} // namespace Bess::UI
