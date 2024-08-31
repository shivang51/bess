#include "pages/page.h"

#include "application_state.h"
#include "pages/main_page/main_page.h"
#include "pages/start_page/start_page.h"

namespace Bess::Pages {
    Page::Page(PageIdentifier identifier) : m_identifier(identifier) {
    }

    void Page::show() {
        std::shared_ptr<Page> instance;
        switch (m_identifier) {
        case PageIdentifier::MainPage:
            instance = MainPage::getInstance();
            break;
        case PageIdentifier::StartPage:
            instance = StartPage::getInstance();
            break;
        default:
            throw std::runtime_error("Unknown page identifier");
        }
        ApplicationState::setCurrentPage(instance);
    }

    PageIdentifier Page::getIdentifier() const {
        return m_identifier;
    }
} // namespace Bess::Pages
