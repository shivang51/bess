#pragma once
#include "glm.hpp"
#include "pages/page.h"
#include "window.h"
#include <memory>

namespace Bess {
    class ApplicationState {
      public:
        static void setCurrentPage(std::shared_ptr<Pages::Page> page);
        static std::shared_ptr<Pages::Page> getCurrentPage();

        static void setParentWindow(std::shared_ptr<Window> parentWindow);

        static std::shared_ptr<Window> getParentWindow();

        static void quit();

        static void clear();

      private:
        static std::shared_ptr<Pages::Page> m_currentPage;
        static std::shared_ptr<Window> m_parentWindow;
    };

} // namespace Bess
