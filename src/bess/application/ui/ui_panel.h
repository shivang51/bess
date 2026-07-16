#pragma once

#include "common/bess_api.h"

#include "common/class_helpers.h"
#include "common/types.h"
#include "ui/dock_ids.h"
#include "imgui.h"
#include <string>

namespace Bess::UI {
    class BESS_API Panel {
      public:
        Panel(const std::string &name);
        virtual ~Panel() = default;
        virtual void init();
        virtual void update(TimeMs ts);
        virtual void destroy();

        void render();

        virtual void onShow();
        virtual void onHide();

        void toggleVisibility();

        void hide();
        void show();

        MAKE_GETTER_SETTER(Dock, DefaultDock, m_defaultDock)
        MAKE_GETTER_SETTER(std::string, Name, m_name)
        MAKE_GETTER_SETTER(bool, Visible, m_visible)
        MAKE_GETTER_SETTER(bool, ShowInMenuBar, m_showInMenuBar)
        MAKE_GETTER_SETTER(ImGuiWindowFlags, Flags, m_flags)

      protected:
        virtual void onBeforeDraw() {};
        virtual void onDraw() = 0;
        virtual void onAfterDraw() {};

      protected:
        std::string m_name;
        bool m_visible = false;
        bool m_showInMenuBar = true;
        Dock m_defaultDock = Dock::bottom;
        ImGuiWindowFlags m_flags = ImGuiWindowFlags_NoFocusOnAppearing;
        bool m_isFocused = false;
        bool m_isHovered = false;
    };
} // namespace Bess::UI
