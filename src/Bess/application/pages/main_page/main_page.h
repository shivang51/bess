#pragma once

#include "application/app_types.h"
#include "application/events/application_event.h"
#include "application/pages/main_page/main_page_state.h"
#include "application/pages/page.h"
#include "application/window.h"
#include "component_definition.h"

#include <chrono>
#include <memory>
#include <vector>

namespace Bess::Pages {

    struct CopiedComponent {
        std::shared_ptr<SimEngine::ComponentDefinition> def;
        std::type_index nsComp = typeid(void);
        glm::vec2 pos = {0.f, 0.f};
    };

    struct LastMouseButtonEvent {
        std::chrono::time_point<std::chrono::steady_clock> timestamp;
        ApplicationEvent::MouseButtonData data;
        bool processed = true;
    };

    class MainPage : public Page {
      public:
        MainPage(const std::shared_ptr<Window> &parentWindow);
        ~MainPage() override;

        static std::shared_ptr<Page> getInstance(const std::shared_ptr<Window> &parentWindow = nullptr);

        static std::shared_ptr<MainPage> getTypedInstance(const std::shared_ptr<Window> &parentWindow = nullptr);

        void draw() override;

        void update(TFrameTime ts, std::vector<ApplicationEvent> &events) override;

        std::shared_ptr<Window> getParentWindow();

        void destory();

        MainPageState &getState();

      private:
        std::shared_ptr<Window> m_parentWindow;

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

        std::vector<CopiedComponent> m_copiedComponents;
    };
} // namespace Bess::Pages
