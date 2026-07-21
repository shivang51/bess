#pragma once

#include "models/menu_model.h"
#include "models/tab_model.h"
#include "ui_view.h"

#include <cstddef>
#include <memory>
#include <string>

namespace Bess::UI {

    // Temporary integration showcase for the retained UI stack. It lives in
    // ui_core so the application only mounts a view; remove the mount from
    // Window when the real application view is ready.
    class BESS_API UIDemoView final : public UIView {
      public:
        void compose(UIComposer &ui) override;

        [[nodiscard]] size_t activationCount() const noexcept;
        [[nodiscard]] std::shared_ptr<TabModel> tabs() const noexcept;
        [[nodiscard]] std::shared_ptr<MenuModel> menus() const noexcept;
        [[nodiscard]] WidgetRef<DockSpace> dockSpace() const noexcept;

      private:
        void setStatus(std::string text);
        void incrementCounter();
        void selectNextTab();

        size_t m_activationCount = 0;
        std::shared_ptr<TabModel> m_tabs;
        std::shared_ptr<MenuModel> m_menus;
        WidgetRef<MenuBar> m_menuBar;
        WidgetRef<Label> m_status;
        WidgetRef<Button> m_counterButton;
        WidgetRef<TabBar> m_tabBar;
        WidgetRef<DockSpace> m_dockSpace;
        DockPanelHandle m_explorerPanel;
        DockPanelHandle m_inspectorPanel;
        DockPanelHandle m_previewPanel;
        DockPanelHandle m_consolePanel;
        DockPanelHandle m_assetsPanel;
    };

} // namespace Bess::UI
