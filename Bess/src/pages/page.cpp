#include "pages/page.h"

#include "application_state.h"

namespace Bess::Pages {
    Page::Page(PageIdentifier identifier) : m_identifier(identifier) {
    }

    void Page::show() {
        ApplicationState::setCurrentPage(m_identifier);
    }

    PageIdentifier Page::getIdentifier() const {
        return m_identifier;
    }
} // namespace Bess::Pages
