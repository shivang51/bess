#include "ui_panel.h"

namespace Bess::UI {
    Panel::Panel(const std::string &name, const std::string &title)
        : m_name(name), m_title(title) {}

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

    bool Panel::isVisible() const { return m_visible; }

    bool &Panel::isVisibleRef() { return m_visible; }

    void Panel::onShow() {}

    void Panel::onHide() {}

    void Panel::update() {}

    void Panel::init() {}

    void Panel::destroy() {}
} // namespace Bess::UI
