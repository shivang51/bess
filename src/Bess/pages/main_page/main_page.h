#pragma once

#include "events/application_event.h"
#include "pages/main_page/main_page_state.h"
#include "pages/page.h"
#include "scene/scene.h"
#include "application/types.h"
#include "application/window.h"

#include <chrono>
#include <memory>
#include <vector>

namespace Bess::Pages {
    class MainPage : public Page {
      public:
        MainPage(std::shared_ptr<Window> parentWindow);
        ~MainPage() override;

        static std::shared_ptr<Page> getInstance(const std::shared_ptr<Window> &parentWindow = nullptr);

        static std::shared_ptr<MainPage> getTypedInstance(std::shared_ptr<Window> parentWindow = nullptr);

        void draw() override;

        void update(TFrameTime ts, const std::vector<ApplicationEvent> &events) override;

        std::shared_ptr<Window> getParentWindow();
        std::shared_ptr<Canvas::Scene> getScene() const;

        void destory();

      private:
        std::shared_ptr<Window> m_parentWindow;
        std::shared_ptr<Bess::Canvas::Scene> m_scene;

        // event handlers
      private:
        bool isCursorInViewport();
        void finishDragging();

        glm::vec2 getViewportMousePos();
        glm::vec2 getNVPMousePos();

      private:
        bool m_leftMousePressed = false;
        bool m_rightMousePressed = false;
        bool m_middleMousePressed = false;

        std::chrono::time_point<std::chrono::steady_clock> m_lastUpdateTime;

        std::shared_ptr<MainPageState> m_state;

        bool m_isDestroyed = false;
    };
} // namespace Bess::Pages
