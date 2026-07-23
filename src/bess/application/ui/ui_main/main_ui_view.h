#pragma once

#include "common/bess_api.h"
#include "controls/basic_widgets.h"
#include "controls/dock_space.h"
#include "controls/menu_bar.h"
#include "controls/scene_view.h"
#include "models/action_registry.h"
#include "models/menu_model.h"
#include "ui/ui_main/scene_viewport.h"
#include "ui/ui_main/scene_viewport_controller.h"
#include "ui_view.h"

#include <memory>
#include <string>
#include <vector>

namespace Bess::UI {

    class BESS_API MainUIView final : public UIView {
      public:
        void compose(UIComposer &ui) override;
        void onUnmounting(UIViewContext &context) noexcept override;

        [[nodiscard]] std::shared_ptr<SceneViewportController>
        primaryViewport() const noexcept;
        [[nodiscard]] WidgetRef<DockSpace> dockSpace() const noexcept;

      private:
        void composeMenus(const std::shared_ptr<ActionRegistry> &actions);
        void registerShellActions(ActionRegistry &actions);
        void unregisterShellActions() noexcept;
        void setStatus(std::string text);

        std::unique_ptr<Canvas::SceneViewport> m_sceneViewport;
        std::shared_ptr<MenuModel> m_menus;
        std::shared_ptr<ActionRegistry> m_actions;
        std::vector<ActionId> m_shellActions;
        WidgetRef<DockSpace> m_dockSpace;
        WidgetRef<Label> m_status;
        DockPanelHandle m_viewportPanel;
        DockPanelHandle m_explorerPanel;
        DockPanelHandle m_propertiesPanel;
        DockPanelHandle m_consolePanel;
    };

} // namespace Bess::UI
