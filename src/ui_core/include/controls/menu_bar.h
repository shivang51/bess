#pragma once

#include "models/menu_model.h"
#include "ui_style.h"
#include "widget.h"

#include <cstddef>
#include <memory>
#include <optional>
#include <span>
#include <vector>

namespace Bess::UI {

    struct MenuBarOptions {
        std::optional<UIMenuStyle> style;
        // MenuBar is content-sized and background-neutral by default so it
        // composes naturally inside application bars alongside icons,
        // spacers, and arbitrary actions. These switches retain convenient
        // standalone/full-width use without coupling the control to a
        // particular piece of window chrome.
        bool stretchWidth = false;
        bool drawBackground = false;
    };

    struct MenuHeadingLayout {
        MenuId menu;
        WidgetBounds bounds;
    };

    struct MenuPopupItemLayout {
        MenuItemId item;
        WidgetBounds bounds;
        WidgetBounds iconBounds;
        WidgetBounds labelBounds;
        WidgetBounds shortcutBounds;
        WidgetBounds submenuIndicatorBounds;
        bool separator = false;
    };

    struct MenuPopupLayout {
        size_t depth = 0;
        WidgetBounds bounds;
        std::vector<MenuPopupItemLayout> items;
        float contentHeight = 0.f;

        [[nodiscard]] const MenuPopupItemLayout *
        itemAt(glm::vec2 position) const noexcept;
    };

    // Shared popup-row geometry for menu bars and standalone context menus.
    // Columns are measured once per panel and reserved uniformly for every
    // row, keeping labels, shortcuts, and submenu indicators aligned.
    class BESS_API MenuPopupLayoutSolver {
      public:
        [[nodiscard]] static glm::vec2
        preferredSize(std::span<const MenuItem> items,
                      const UIMenuStyle &style);

        [[nodiscard]] static MenuPopupLayout
        calculate(WidgetBounds bounds,
                  std::span<const MenuItem> items,
                  const UIMenuStyle &style,
                  size_t depth = 0,
                  float scrollOffset = 0.f);
    };

    struct MenuPopupPaintOptions {
        MenuItemId hotItem;
        MenuItemId pressedItem;
        std::span<const MenuItemId> activePath;
        float panelDepth = 0.f;
        float rowChromeDepth = 0.001f;
        float contentDepth = 0.002f;
    };

    // Shared renderer-neutral popup presentation. Hosts retain ownership of
    // focus, dismissal, scrolling, and placement while row visuals remain
    // identical across menu-bar popups and context menus.
    class BESS_API MenuPopupPresenter {
      public:
        static void paint(UIPainter &painter,
                          const MenuPopupLayout &layout,
                          const MenuModel &model,
                          const UIMenuStyle &style,
                          PickingId pickingId,
                          MenuPopupPaintOptions options = {});
    };

    struct MenuBarLayout {
        WidgetBounds barBounds;
        std::vector<MenuHeadingLayout> headings;
        std::vector<MenuPopupLayout> popups;

        [[nodiscard]] const MenuHeadingLayout *
        headingAt(glm::vec2 position) const noexcept;
        [[nodiscard]] const MenuPopupItemLayout *
        itemAt(glm::vec2 position, size_t *depth = nullptr) const noexcept;
        [[nodiscard]] bool contains(glm::vec2 position) const noexcept;
    };

    // Pure placement shared by painting, input, and tests. `controlBounds`
    // includes the themed vertical margin; `MenuBarLayout::barBounds` is the
    // inset, interactive strip. Popups flip at viewport edges and submenus
    // overlap their parent by a small themed amount, avoiding dead gaps.
    class BESS_API MenuBarLayoutSolver {
      public:
        [[nodiscard]] static MenuBarLayout
        calculate(WidgetBounds controlBounds,
                  WidgetBounds viewportBounds,
                  const MenuModel &model,
                  MenuId activeMenu,
                  std::span<const MenuItemId> openSubmenus,
                  const UIMenuStyle &style);
    };

    class BESS_API MenuBar : public Widget {
      public:
        explicit MenuBar(std::shared_ptr<MenuModel> model,
                         MenuBarOptions options = {});

        [[nodiscard]] std::string_view typeName() const noexcept override;
        [[nodiscard]] WidgetTraits traits() const noexcept override;
        void onMount(WidgetMountContext &context) override;
        void onUnmount(WidgetTree &state, WidgetId id) override;
        void updateLayout(WidgetLayoutContext &context) override;
        void arrange(WidgetArrangeContext &context) override;
        void paint(WidgetPaintContext &context) const override;
        void paintOverlay(WidgetPaintContext &context) const override;
        [[nodiscard]] bool hitTest(WidgetBounds bounds,
                                   glm::vec2 position) const noexcept override;
        UIEventReply onEvent(WidgetEventContext &context,
                             const UIEvent &event) override;

        [[nodiscard]] std::shared_ptr<MenuModel> model() const noexcept;
        void setModel(std::shared_ptr<MenuModel> model);
        [[nodiscard]] bool isOpen() const noexcept;
        [[nodiscard]] MenuId activeMenu() const noexcept;
        void close();

      private:
        [[nodiscard]] const UIMenuStyle &style(const WidgetTree &state) const;
        void rebuildLayout(WidgetBounds bounds, const WidgetTree &state) const;
        void reconnectModel();
        void normalizeOpenPath();
        void openMenu(MenuId menu);
        void openSubmenu(MenuItemId item, size_t depth, bool selectChild);
        void closeFromDepth(size_t depth);
        bool moveHeading(int direction);
        bool moveSelection(int direction);
        bool activateSelection();
        [[nodiscard]] const std::vector<MenuItem> *
        itemsAtDepth(size_t depth) const noexcept;
        [[nodiscard]] const MenuItem *selectedItem() const noexcept;
        [[nodiscard]] size_t selectedDepth() const noexcept;

        std::shared_ptr<MenuModel> m_model;
        MenuBarOptions m_options;
        WidgetTree *m_state = nullptr;
        WidgetId m_id;
        MenuModel::ChangedSignal::Connection m_modelConnection;
        MenuId m_activeMenu;
        std::vector<MenuItemId> m_openSubmenus;
        MenuItemId m_hotItem;
        MenuId m_hotHeading;
        MenuItemId m_pressedItem;
        size_t m_hotDepth = 0;
        mutable MenuBarLayout m_layout;
        mutable WidgetBounds m_layoutBounds;
        mutable glm::vec2 m_layoutViewportSize{0.f};
        mutable MenuId m_layoutActiveMenu;
        mutable std::vector<MenuItemId> m_layoutOpenSubmenus;
        mutable bool m_layoutDirty = true;
    };

} // namespace Bess::UI
