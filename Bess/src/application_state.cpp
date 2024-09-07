#include "application_state.h"

namespace Bess {
    std::shared_ptr<Pages::Page> ApplicationState::m_currentPage;
    std::shared_ptr<Window> ApplicationState::m_parentWindow;

    void ApplicationState::setCurrentPage(std::shared_ptr<Pages::Page> page) {
        m_currentPage = page;
    }

    std::shared_ptr<Pages::Page> ApplicationState::getCurrentPage() {
        return m_currentPage;
    }

    void ApplicationState::setParentWindow(Window *parentWindow) {
        m_parentWindow = std::shared_ptr<Window>(parentWindow);
    }

    std::shared_ptr<Window> ApplicationState::getParentWindow() {
        return m_parentWindow;
    }

} // namespace Bess
