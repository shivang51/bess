#pragma once

#include "common/bess_api.h"
#include "models/action_registry.h"
#include "models/signal.h"
#include "ui_types.h"

#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace Bess::UI {

    enum class MenuItemKind : uint8_t { command, separator };

    struct MenuItem {
        MenuItemId id;
        std::string icon;
        std::string name;
        std::string shortcut;
        // Optional semantic action. When set and a registry is bound to the
        // owning MenuModel, presentation and activation are driven by the
        // ActionRegistry. Local activated is used only for unbound commands.
        ActionId action;
        std::function<void()> activated;
        bool enabled = true;
        bool checked = false;
        bool visible = true;
        MenuItemKind kind = MenuItemKind::command;
        std::vector<MenuItem> children;

        [[nodiscard]] static MenuItem separator();
        [[nodiscard]] static MenuItem fromAction(ActionId action);
        [[nodiscard]] bool isSubmenu() const noexcept;
        [[nodiscard]] bool isActionBound() const noexcept;
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
    //
    // Action binding: setActionRegistry() links every MenuItem with a non-empty
    // ActionId to ActionRegistry. Bound items re-sync label/icon/shortcut/
    // enabled/checked/visible from action state and invoke via the registry
    // with ActionInvocationSource::menu. Unbound items keep the legacy
    // activated callback path. One action may be referenced by many items.
    class BESS_API MenuModel {
      public:
        using ChangedSignal = Signal<MenuModelChange>;

        MenuModel() = default;
        MenuModel(const MenuModel &) = delete;
        MenuModel &operator=(const MenuModel &) = delete;
        MenuModel(MenuModel &&) = delete;
        MenuModel &operator=(MenuModel &&) = delete;
        ~MenuModel();

        MenuId addMenu(MenuDefinition menu);
        bool removeMenu(MenuId menu);
        MenuItemId addItem(MenuId menu, MenuItem item, MenuItemId parent = {});
        bool removeItem(MenuItemId item);

        bool setMenuEnabled(MenuId menu, bool enabled);
        bool setItemEnabled(MenuItemId item, bool enabled);
        bool setItemChecked(MenuItemId item, bool checked);
        bool setItemName(MenuItemId item, std::string name);
        bool setItemVisible(MenuItemId item, bool visible);
        bool setItemAction(MenuItemId item, ActionId action);
        bool activate(MenuItemId item) const;

        // Binds or rebinds the shared action registry. Passing nullptr clears
        // the binding; bound items keep last synced presentation until rebound.
        void setActionRegistry(std::shared_ptr<ActionRegistry> registry);
        [[nodiscard]] const std::shared_ptr<ActionRegistry> &
        actionRegistry() const noexcept;

        [[nodiscard]] const std::vector<MenuDefinition> &menus() const noexcept;
        [[nodiscard]] const MenuDefinition *findMenu(MenuId id) const noexcept;
        [[nodiscard]] const MenuItem *findItem(MenuItemId id) const noexcept;
        [[nodiscard]] bool validate(std::string *reason = nullptr) const;
        [[nodiscard]] ChangedSignal &changed() noexcept;

      private:
        [[nodiscard]] MenuDefinition *findMenuMutable(MenuId id) noexcept;
        [[nodiscard]] MenuItem *findItemMutable(MenuItemId id) noexcept;
        void reindexActions();
        void syncAllActionItems();
        void syncAction(const ActionId &action);
        bool syncItemFromAction(MenuItem &item,
                                const ActionDefinition *definition,
                                bool available) const;
        void onActionChanged(const ActionChange &change);

        std::vector<MenuDefinition> m_menus;
        ChangedSignal m_changed;
        std::shared_ptr<ActionRegistry> m_actions;
        ActionRegistry::ChangedSignal::Connection m_actionConnection;
        std::unordered_map<ActionId, std::vector<MenuItemId>> m_actionItems;
    };

} // namespace Bess::UI
