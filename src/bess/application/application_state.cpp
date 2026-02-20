#include "application/application_state.h"

namespace Bess {
    std::shared_ptr<Pages::Page> ApplicationState::m_currentPage;
    std::shared_ptr<Window> ApplicationState::m_parentWindow;

    void ApplicationState::setCurrentPage(const std::shared_ptr<Pages::Page> &page) {
        m_currentPage = page;
    }

    std::shared_ptr<Pages::Page> ApplicationState::getCurrentPage() {
        return m_currentPage;
    }

    void ApplicationState::setParentWindow(const std::shared_ptr<Window> &parentWindow) {
        m_parentWindow = parentWindow;
    }

    std::shared_ptr<Window> ApplicationState::getParentWindow() {
        return m_parentWindow;
    }

    void ApplicationState::quit() {
        m_parentWindow->close();
    }

    void ApplicationState::clear() {
        m_currentPage.reset();
    }
} // namespace Bess
