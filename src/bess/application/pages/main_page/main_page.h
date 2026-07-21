#pragma once

#include "common/bess_api.h"

#include "pages/main_page/main_page_state.h"
#include "pages/page.h"
#include "window.h"
#include "common/types.h"
#include "sim_driver/sim_driver.h"

#include <chrono>
#include <memory>

namespace Bess::Pages {

    struct BESS_API CopiedComponent {
        std::shared_ptr<SimEngine::Drivers::CompDef> def;
        std::type_index nsComp = typeid(void);
        glm::vec2 pos = {0.f, 0.f};
    };

    class BESS_API MainPage : public Page {
      public:
        MainPage(const std::shared_ptr<Window> &parentWindow);
        ~MainPage() override;

        static bool s_headless;
        static void setHeadless(bool headless);

        static std::shared_ptr<MainPage> &
        getInstance(const std::shared_ptr<Window> &parentWindow = nullptr);

        void draw() override;

        void update(TimeMs ts) override;

        std::shared_ptr<Window> getParentWindow();

        void destory();

        MainPageState &getState();

      private:
        std::shared_ptr<Window> m_parentWindow;

        void handleKeyboardShortcuts();

        void copySelectedEntities();
        void pasteCopiedEntities();

      private:
        bool m_leftMousePressed = false;
        bool m_rightMousePressed = false;
        bool m_middleMousePressed = false;

        std::chrono::time_point<std::chrono::steady_clock> m_lastUpdateTime;

        MainPageState m_state;

        bool m_isDestroyed = false;

        int m_clickCount = 0;
    };
} // namespace Bess::Pages
