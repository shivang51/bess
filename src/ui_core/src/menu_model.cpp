#include "models/menu_model.h"

#include "common/types.h"

#include <algorithm>
#include <utility>

namespace Bess::UI {
    namespace {
        MenuItem *findNestedItem(std::vector<MenuItem> &items,
                                 MenuItemId id) noexcept {
            for (auto &item : items) {
                if (item.id == id) {
                    return &item;
                }
                if (auto *nested = findNestedItem(item.children, id)) {
                    return nested;
                }
            }
            return nullptr;
        }

        const MenuItem *findNestedItem(const std::vector<MenuItem> &items,
                                       MenuItemId id) noexcept {
            for (const auto &item : items) {
                if (item.id == id) {
                    return &item;
                }
                if (const auto *nested = findNestedItem(item.children, id)) {
                    return nested;
                }
            }
            return nullptr;
        }

        bool prepareItems(std::vector<MenuItem> &items,
                          HashSet<MenuItemId> &ids) {
            for (auto &item : items) {
                if (!item.id) {
                    do {
                        item.id = MenuItemId::generate();
                    } while (ids.contains(item.id));
                }
                if (!ids.insert(item.id).second) {
                    return false;
                }
                if (item.kind == MenuItemKind::separator) {
                    item.icon.clear();
                    item.name.clear();
                    item.shortcut.clear();
                    item.action = {};
                    item.activated = {};
                    item.enabled = false;
                    item.checked = false;
                    item.visible = true;
                    item.children.clear();
                } else if (!prepareItems(item.children, ids)) {
                    return false;
                }
            }
            return true;
        }

        bool eraseItem(std::vector<MenuItem> &items, MenuItemId id) {
            const auto direct = std::find_if(
                items.begin(), items.end(), [id](const MenuItem &item) {
                    return item.id == id;
                });
            if (direct != items.end()) {
                items.erase(direct);
                return true;
            }
            for (auto &item : items) {
                if (eraseItem(item.children, id)) {
                    return true;
                }
            }
            return false;
        }

        void collectActionItems(
            const std::vector<MenuItem> &items,
            std::unordered_map<ActionId, std::vector<MenuItemId>> &index) {
            for (const auto &item : items) {
                if (item.kind == MenuItemKind::command && item.action) {
                    index[item.action].push_back(item.id);
                }
                collectActionItems(item.children, index);
            }
        }

        [[nodiscard]] std::string
        primaryShortcutLabel(const ActionDefinition &definition) {
            if (definition.shortcuts.empty()) {
                return {};
            }
            return formatKeyChord(definition.shortcuts.front());
        }
    } // namespace

    MenuModel::~MenuModel() {
        m_actionConnection.disconnect();
    }

    MenuItem MenuItem::separator() {
        return {.kind = MenuItemKind::separator};
    }

    MenuItem MenuItem::fromAction(ActionId action) {
        return {.action = std::move(action)};
    }

    bool MenuItem::isSubmenu() const noexcept {
        return kind == MenuItemKind::command && !children.empty();
    }

    bool MenuItem::isActionBound() const noexcept {
        return kind == MenuItemKind::command && static_cast<bool>(action);
    }

    MenuId MenuModel::addMenu(MenuDefinition menu) {
        HashSet<MenuId> menuIds;
        HashSet<MenuItemId> itemIds;
        for (const auto &existing : m_menus) {
            menuIds.insert(existing.id);
            std::function<void(const std::vector<MenuItem> &)> collect =
                [&](const std::vector<MenuItem> &items) {
                    for (const auto &item : items) {
                        itemIds.insert(item.id);
                        collect(item.children);
                    }
                };
            collect(existing.items);
        }
        if (!menu.id) {
            do {
                menu.id = MenuId::generate();
            } while (menuIds.contains(menu.id));
        }
        if (!menuIds.insert(menu.id).second ||
            !prepareItems(menu.items, itemIds)) {
            return {};
        }
        const MenuId id = menu.id;
        m_menus.push_back(std::move(menu));
        reindexActions();
        syncAllActionItems();
        m_changed.emit({.kind = MenuModelChangeKind::structure, .menu = id});
        return id;
    }

    bool MenuModel::removeMenu(MenuId menu) {
        const auto it = std::find_if(
            m_menus.begin(), m_menus.end(), [menu](const auto &entry) {
                return entry.id == menu;
            });
        if (it == m_menus.end()) {
            return false;
        }
        m_menus.erase(it);
        reindexActions();
        m_changed.emit({.kind = MenuModelChangeKind::structure, .menu = menu});
        return true;
    }

    MenuItemId
    MenuModel::addItem(MenuId menuId, MenuItem item, MenuItemId parent) {
        auto *menu = findMenuMutable(menuId);
        if (menu == nullptr ||
            (item.kind == MenuItemKind::separator && !item.children.empty())) {
            return {};
        }
        HashSet<MenuItemId> ids;
        for (const auto &menuEntry : m_menus) {
            std::function<void(const std::vector<MenuItem> &)> collect =
                [&](const std::vector<MenuItem> &items) {
                    for (const auto &entry : items) {
                        ids.insert(entry.id);
                        collect(entry.children);
                    }
                };
            collect(menuEntry.items);
        }
        std::vector<MenuItem> prepared;
        prepared.push_back(std::move(item));
        if (!prepareItems(prepared, ids)) {
            return {};
        }
        const MenuItemId id = prepared.front().id;
        if (parent) {
            auto *owner = findItemMutable(parent);
            if (owner == nullptr || owner->kind == MenuItemKind::separator) {
                return {};
            }
            owner->children.push_back(std::move(prepared.front()));
        } else {
            menu->items.push_back(std::move(prepared.front()));
        }
        reindexActions();
        if (auto *added = findItemMutable(id); added != nullptr &&
                                               added->isActionBound() &&
                                               m_actions != nullptr) {
            const auto *definition = m_actions->find(added->action);
            static_cast<void>(syncItemFromAction(
                *added,
                definition,
                definition != nullptr && m_actions->isAvailable(added->action)));
        }
        m_changed.emit({.kind = MenuModelChangeKind::structure,
                        .menu = menuId,
                        .item = id});
        return id;
    }

    bool MenuModel::removeItem(MenuItemId item) {
        for (auto &menu : m_menus) {
            if (eraseItem(menu.items, item)) {
                reindexActions();
                m_changed.emit({.kind = MenuModelChangeKind::structure,
                                .menu = menu.id,
                                .item = item});
                return true;
            }
        }
        return false;
    }

    bool MenuModel::setMenuEnabled(MenuId menu, bool enabled) {
        auto *entry = findMenuMutable(menu);
        if (entry == nullptr) {
            return false;
        }
        if (entry->enabled == enabled) {
            return true;
        }
        entry->enabled = enabled;
        m_changed.emit(
            {.kind = MenuModelChangeKind::menuUpdated, .menu = menu});
        return true;
    }

    bool MenuModel::setItemEnabled(MenuItemId item, bool enabled) {
        auto *entry = findItemMutable(item);
        if (entry == nullptr || entry->kind == MenuItemKind::separator) {
            return false;
        }
        if (entry->isActionBound()) {
            return false;
        }
        if (entry->enabled == enabled) {
            return true;
        }
        entry->enabled = enabled;
        m_changed.emit(
            {.kind = MenuModelChangeKind::itemUpdated, .item = item});
        return true;
    }

    bool MenuModel::setItemChecked(MenuItemId item, bool checked) {
        auto *entry = findItemMutable(item);
        if (entry == nullptr || entry->kind == MenuItemKind::separator) {
            return false;
        }
        if (entry->isActionBound()) {
            return false;
        }
        if (entry->checked == checked) {
            return true;
        }
        entry->checked = checked;
        m_changed.emit(
            {.kind = MenuModelChangeKind::itemUpdated, .item = item});
        return true;
    }

    bool MenuModel::setItemName(MenuItemId item, std::string name) {
        auto *entry = findItemMutable(item);
        if (entry == nullptr || entry->kind == MenuItemKind::separator) {
            return false;
        }
        if (entry->isActionBound()) {
            return false;
        }
        if (entry->name == name) {
            return true;
        }
        entry->name = std::move(name);
        m_changed.emit(
            {.kind = MenuModelChangeKind::itemUpdated, .item = item});
        return true;
    }

    bool MenuModel::setItemVisible(MenuItemId item, bool visible) {
        auto *entry = findItemMutable(item);
        if (entry == nullptr || entry->kind == MenuItemKind::separator) {
            return false;
        }
        if (entry->isActionBound()) {
            return false;
        }
        if (entry->visible == visible) {
            return true;
        }
        entry->visible = visible;
        m_changed.emit(
            {.kind = MenuModelChangeKind::itemUpdated, .item = item});
        return true;
    }

    bool MenuModel::setItemAction(MenuItemId item, ActionId action) {
        auto *entry = findItemMutable(item);
        if (entry == nullptr || entry->kind == MenuItemKind::separator ||
            entry->isSubmenu()) {
            return false;
        }
        if (entry->action == action) {
            return true;
        }
        entry->action = std::move(action);
        if (entry->action) {
            entry->activated = {};
        }
        reindexActions();
        if (entry->action && m_actions != nullptr) {
            const auto *definition = m_actions->find(entry->action);
            static_cast<void>(syncItemFromAction(
                *entry,
                definition,
                definition != nullptr &&
                    m_actions->isAvailable(entry->action)));
        }
        m_changed.emit(
            {.kind = MenuModelChangeKind::itemUpdated, .item = item});
        return true;
    }

    bool MenuModel::activate(MenuItemId item) const {
        const auto *entry = findItem(item);
        if (entry == nullptr || !entry->enabled || !entry->visible ||
            entry->isSubmenu() || entry->kind == MenuItemKind::separator) {
            return false;
        }

        if (entry->isActionBound()) {
            if (m_actions == nullptr ||
                !m_actions->isAvailable(entry->action)) {
                return false;
            }
            const ActionId action = entry->action;
            return m_actions
                ->invoke(action, {.source = ActionInvocationSource::menu})
                .wasInvoked();
        }

        if (!entry->activated) {
            return false;
        }
        // The command may mutate this model and erase its own MenuItem.
        auto activated = entry->activated;
        activated();
        return true;
    }

    void MenuModel::setActionRegistry(std::shared_ptr<ActionRegistry> registry) {
        if (m_actions == registry) {
            return;
        }
        m_actionConnection.disconnect();
        m_actions = std::move(registry);
        reindexActions();
        if (m_actions != nullptr) {
            m_actionConnection = m_actions->changed().connect(
                [this](const ActionChange &change) { onActionChanged(change); });
            syncAllActionItems();
        }
    }

    const std::shared_ptr<ActionRegistry> &
    MenuModel::actionRegistry() const noexcept {
        return m_actions;
    }

    const std::vector<MenuDefinition> &MenuModel::menus() const noexcept {
        return m_menus;
    }

    const MenuDefinition *MenuModel::findMenu(MenuId id) const noexcept {
        const auto it =
            std::find_if(m_menus.begin(),
                         m_menus.end(),
                         [id](const auto &menu) { return menu.id == id; });
        return it != m_menus.end() ? &*it : nullptr;
    }

    MenuDefinition *MenuModel::findMenuMutable(MenuId id) noexcept {
        return const_cast<MenuDefinition *>(std::as_const(*this).findMenu(id));
    }

    const MenuItem *MenuModel::findItem(MenuItemId id) const noexcept {
        for (const auto &menu : m_menus) {
            if (const auto *item = findNestedItem(menu.items, id)) {
                return item;
            }
        }
        return nullptr;
    }

    MenuItem *MenuModel::findItemMutable(MenuItemId id) noexcept {
        return const_cast<MenuItem *>(std::as_const(*this).findItem(id));
    }

    bool MenuModel::validate(std::string *reason) const {
        auto fail = [reason](std::string message) {
            if (reason != nullptr) {
                *reason = std::move(message);
            }
            return false;
        };
        HashSet<MenuId> menus;
        HashSet<MenuItemId> items;
        std::function<bool(const std::vector<MenuItem> &)> validateItems =
            [&](const std::vector<MenuItem> &entries) {
                for (const auto &item : entries) {
                    if (!item.id || !items.insert(item.id).second) {
                        return false;
                    }
                    if (item.kind == MenuItemKind::separator &&
                        (!item.name.empty() || !item.icon.empty() ||
                         !item.shortcut.empty() || item.action ||
                         !item.children.empty() || item.enabled ||
                         item.checked)) {
                        return false;
                    }
                    if (item.kind == MenuItemKind::command && item.action &&
                        !item.children.empty()) {
                        return false;
                    }
                    if (!validateItems(item.children)) {
                        return false;
                    }
                }
                return true;
            };
        for (const auto &menu : m_menus) {
            if (!menu.id || !menus.insert(menu.id).second) {
                return fail("menu IDs are missing or duplicated");
            }
            if (!validateItems(menu.items)) {
                return fail("menu item hierarchy is invalid");
            }
        }
        return true;
    }

    MenuModel::ChangedSignal &MenuModel::changed() noexcept {
        return m_changed;
    }

    void MenuModel::reindexActions() {
        m_actionItems.clear();
        for (const auto &menu : m_menus) {
            collectActionItems(menu.items, m_actionItems);
        }
    }

    void MenuModel::syncAllActionItems() {
        if (m_actions == nullptr) {
            return;
        }
        for (const auto &[action, _] : m_actionItems) {
            (void)_;
            syncAction(action);
        }
    }

    void MenuModel::syncAction(const ActionId &action) {
        if (!action) {
            return;
        }
        const auto it = m_actionItems.find(action);
        if (it == m_actionItems.end()) {
            return;
        }
        const ActionDefinition *definition =
            m_actions != nullptr ? m_actions->find(action) : nullptr;
        const bool available =
            definition != nullptr && m_actions != nullptr &&
            m_actions->isAvailable(action);

        for (const MenuItemId itemId : it->second) {
            auto *item = findItemMutable(itemId);
            if (item == nullptr || !item->isActionBound()) {
                continue;
            }
            if (syncItemFromAction(*item, definition, available)) {
                m_changed.emit(
                    {.kind = MenuModelChangeKind::itemUpdated, .item = itemId});
            }
        }
    }

    bool MenuModel::syncItemFromAction(MenuItem &item,
                                       const ActionDefinition *definition,
                                       bool available) const {
        if (definition == nullptr) {
            bool changed = false;
            if (item.enabled) {
                item.enabled = false;
                changed = true;
            }
            if (item.checked) {
                item.checked = false;
                changed = true;
            }
            if (item.visible) {
                item.visible = false;
                changed = true;
            }
            return changed;
        }

        const auto &state = definition->state;
        const std::string shortcut = primaryShortcutLabel(*definition);
        bool changed = false;
        if (item.name != state.label) {
            item.name = state.label;
            changed = true;
        }
        if (item.icon != state.icon) {
            item.icon = state.icon;
            changed = true;
        }
        if (item.shortcut != shortcut) {
            item.shortcut = shortcut;
            changed = true;
        }
        if (item.enabled != available) {
            item.enabled = available;
            changed = true;
        }
        const bool checked = state.checkable && state.checked;
        if (item.checked != checked) {
            item.checked = checked;
            changed = true;
        }
        if (item.visible != state.visible) {
            item.visible = state.visible;
            changed = true;
        }
        return changed;
    }

    void MenuModel::onActionChanged(const ActionChange &change) {
        if (m_actions == nullptr) {
            return;
        }

        if (change.action) {
            syncAction(change.action);
            return;
        }

        if (change.kind == ActionChangeKind::scopeChanged ||
            change.kind == ActionChangeKind::unregistered ||
            change.kind == ActionChangeKind::registered ||
            change.kind == ActionChangeKind::stateChanged ||
            change.kind == ActionChangeKind::shortcutsChanged ||
            change.kind == ActionChangeKind::handlerChanged) {
            syncAllActionItems();
        }
    }
} // namespace Bess::UI
