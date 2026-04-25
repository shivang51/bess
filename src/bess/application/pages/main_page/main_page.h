#pragma once

#include "application/events/application_event.h"
#include "application/pages/main_page/main_page_state.h"
#include "application/pages/page.h"
#include "application/window.h"
#include "common/types.h"
#include "drivers/sim_driver.h"

#include <chrono>
#include <memory>
#include <vector>

namespace Bess::Pages {

    struct CopiedComponent {
        std::shared_ptr<SimEngine::Drivers::CompDef> def;
        std::type_index nsComp = typeid(void);
        glm::vec2 pos = {0.f, 0.f};
    };

    struct LastMouseButtonEvent {
        std::chrono::time_point<std::chrono::steady_clock> timestamp;
        ApplicationEvent::MouseButtonData data;
    };

    class MainPage : public Page {
      public:
        MainPage(const std::shared_ptr<Window> &parentWindow);
        ~MainPage() override;

        static bool s_headless;
        static void setHeadless(bool headless);

        static std::shared_ptr<MainPage> &getInstance(const std::shared_ptr<Window> &parentWindow = nullptr);

        void draw() override;

        void update(TimeMs ts, std::vector<ApplicationEvent> &events) override;

        std::shared_ptr<Window> getParentWindow();

        void destory();

        MainPageState &getState();

      private:
        std::shared_ptr<Window> m_parentWindow;

        void drawGhostConnection(const std::shared_ptr<PathRenderer> &pathRenderer,
                                 const glm::vec2 &startPos,
                                 const glm::vec2 &endPos);

      private:
        void handleKeyboardShortcuts();

        void copySelectedEntities();
        void pasteCopiedEntities();

      private:
        bool m_leftMousePressed = false;
        bool m_rightMousePressed = false;
        bool m_middleMousePressed = false;

        std::chrono::time_point<std::chrono::steady_clock> m_lastUpdateTime;

        LastMouseButtonEvent m_lastMouseButtonEvent;

        MainPageState m_state;

        bool m_isDestroyed = false;

        int m_clickCount = 0;

        std::vector<CopiedComponent> m_copiedComponents;
    };
} // namespace Bess::Pages
