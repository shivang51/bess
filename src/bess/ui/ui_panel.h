#pragma once

#include "common/class_helpers.h"
#include "component_definition.h"
#include "imgui.h"
#include "dock_ids.h"
#include <string>

namespace Bess::UI {
    class Panel {
      public:
        Panel(const std::string &name);
        virtual ~Panel() = default;
        virtual void init();
        virtual void update();
        virtual void destroy();

        virtual void render() = 0;

        virtual void onShow();
        virtual void onHide();

        void hide();
        void show();

        MAKE_GETTER_SETTER(Dock, DefaultDock, m_defaultDock)
        MAKE_GETTER_SETTER(std::string, Name, m_name)
        MAKE_GETTER_SETTER(bool, Visible, m_visible)

      protected:
        std::string m_name;
        bool m_visible = false;
        Dock m_defaultDock = Dock::bottom;
    };
} // namespace Bess::UI
