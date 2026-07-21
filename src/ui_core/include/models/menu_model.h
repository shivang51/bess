#pragma once

#include "common/bess_api.h"
#include "models/signal.h"
#include "ui_types.h"

#include <cstdint>
#include <functional>
#include <string>
#include <vector>

namespace Bess::UI {

    enum class MenuItemKind : uint8_t { command, separator };

    struct MenuItem {
        MenuItemId id;
        std::string icon;
        std::string name;
        std::string shortcut;
        std::function<void()> activated;
        bool enabled = true;
        bool checked = false;
        MenuItemKind kind = MenuItemKind::command;
        std::vector<MenuItem> children;

        [[nodiscard]] static MenuItem separator();
        [[nodiscard]] bool isSubmenu() const noexcept;
    };

    struct MenuDefinition {
        MenuId id;
        std::string name;
        bool enabled = true;
        std::vector<MenuItem> items;
    };

    enum class MenuModelChangeKind : uint8_t {
        structure,
        itemUpdated,
        menuUpdated,
    };

    struct MenuModelChange {
        MenuModelChangeKind kind = MenuModelChangeKind::structure;
        MenuId menu;
        MenuItemId item;
    };

    // Renderer-neutral hierarchy shared by menu bars, context menus, and
    // command palettes. IDs remain stable while labels and enabled state are
    // changed, so an open menu can survive ordinary command-state updates.
    class BESS_API MenuModel {
      public:
        using ChangedSignal = Signal<MenuModelChange>;

        MenuId addMenu(MenuDefinition menu);
        bool removeMenu(MenuId menu);
        MenuItemId addItem(MenuId menu, MenuItem item, MenuItemId parent = {});
        bool removeItem(MenuItemId item);

        bool setMenuEnabled(MenuId menu, bool enabled);
        bool setItemEnabled(MenuItemId item, bool enabled);
        bool setItemChecked(MenuItemId item, bool checked);
        bool setItemName(MenuItemId item, std::string name);
        bool activate(MenuItemId item) const;

        [[nodiscard]] const std::vector<MenuDefinition> &menus() const noexcept;
        [[nodiscard]] const MenuDefinition *findMenu(MenuId id) const noexcept;
        [[nodiscard]] const MenuItem *findItem(MenuItemId id) const noexcept;
        [[nodiscard]] bool validate(std::string *reason = nullptr) const;
        [[nodiscard]] ChangedSignal &changed() noexcept;

      private:
        [[nodiscard]] MenuDefinition *findMenuMutable(MenuId id) noexcept;
        [[nodiscard]] MenuItem *findItemMutable(MenuItemId id) noexcept;

        std::vector<MenuDefinition> m_menus;
        ChangedSignal m_changed;
    };

} // namespace Bess::UI
