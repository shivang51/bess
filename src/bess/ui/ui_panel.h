#pragma once

#include <string>
namespace Bess::UI {
    class Panel {
      public:
        Panel(const std::string &name, const std::string &title = "");
        virtual ~Panel() = default;
        virtual void init();
        virtual void update();
        virtual void destroy();

        virtual void render() = 0;

        virtual void onShow();
        virtual void onHide();

        bool isVisible() const;
        bool &isVisibleRef();

        void hide();
        void show();

      protected:
        std::string m_name;
        std::string m_title;
        bool m_visible = false;
    };
} // namespace Bess::UI
