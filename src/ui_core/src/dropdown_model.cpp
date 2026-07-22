#include "models/dropdown_model.h"

#include <utility>

namespace Bess::UI {

    DropdownItemId DropdownModel::add(DropdownItem item, size_t index) {
        if (!item.id) {
            item.id = DropdownItemId::generate();
        }
        if (find(item.id) != nullptr) {
            return {};
        }
        const size_t insertAt =
            index == npos ? m_items.size() : std::min(index, m_items.size());
        const DropdownItemId inserted = item.id;
        const DropdownItemId previous = m_selection;
        m_items.insert(m_items.begin() + static_cast<ptrdiff_t>(insertAt),
                       std::move(item));
        if (!m_selection && m_items[insertAt].enabled) {
            m_selection = inserted;
        }
        m_changed.emit({.kind = DropdownChangeKind::structure,
                        .item = inserted,
                        .previousSelection = previous,
                        .selection = m_selection});
        return inserted;
    }

    DropdownItemId DropdownModel::add(std::string label,
                                      std::string icon,
                                      bool enabled,
                                      size_t index) {
        return add({.icon = std::move(icon),
                    .label = std::move(label),
                    .enabled = enabled},
                   index);
    }

    bool DropdownModel::remove(DropdownItemId id) {
        const size_t index = indexOf(id);
        if (index == npos) {
            return false;
        }
        const DropdownItemId previous = m_selection;
        m_items.erase(m_items.begin() + static_cast<ptrdiff_t>(index));
        if (m_selection == id) {
            m_selection = nearestEnabled(index);
        }
        m_changed.emit({.kind = DropdownChangeKind::structure,
                        .item = id,
                        .previousSelection = previous,
                        .selection = m_selection});
        return true;
    }

    bool DropdownModel::select(DropdownItemId id) {
        const auto *item = find(id);
        if (item == nullptr || !item->enabled) {
            return false;
        }
        if (m_selection == id) {
            return false;
        }
        const DropdownItemId previous = m_selection;
        m_selection = id;
        m_changed.emit({.kind = DropdownChangeKind::selection,
                        .item = id,
                        .previousSelection = previous,
                        .selection = m_selection});
        return true;
    }

    bool DropdownModel::clearSelection() {
        if (!m_selection) {
            return false;
        }
        const DropdownItemId previous = m_selection;
        m_selection = {};
        m_changed.emit({.kind = DropdownChangeKind::selection,
                        .previousSelection = previous});
        return true;
    }

    bool DropdownModel::setEnabled(DropdownItemId id, bool enabled) {
        auto *item = findMutable(id);
        if (item == nullptr || item->enabled == enabled) {
            return item != nullptr;
        }
        const DropdownItemId previous = m_selection;
        item->enabled = enabled;
        if (!enabled && m_selection == id) {
            m_selection = nearestEnabled(indexOf(id));
        } else if (enabled && !m_selection) {
            m_selection = id;
        }
        m_changed.emit({.kind = DropdownChangeKind::itemUpdated,
                        .item = id,
                        .previousSelection = previous,
                        .selection = m_selection});
        return true;
    }

    bool DropdownModel::setLabel(DropdownItemId id, std::string label) {
        auto *item = findMutable(id);
        if (item == nullptr) {
            return false;
        }
        if (item->label == label) {
            return true;
        }
        item->label = std::move(label);
        m_changed.emit({.kind = DropdownChangeKind::itemUpdated,
                        .item = id,
                        .previousSelection = m_selection,
                        .selection = m_selection});
        return true;
    }

    std::span<const DropdownItem> DropdownModel::items() const noexcept {
        return m_items;
    }

    const DropdownItem *DropdownModel::find(DropdownItemId id) const noexcept {
        const size_t index = indexOf(id);
        return index != npos ? &m_items[index] : nullptr;
    }

    DropdownItem *DropdownModel::findMutable(DropdownItemId id) noexcept {
        const size_t index = indexOf(id);
        return index != npos ? &m_items[index] : nullptr;
    }

    size_t DropdownModel::indexOf(DropdownItemId id) const noexcept {
        const auto it = std::find_if(
            m_items.begin(), m_items.end(), [id](const DropdownItem &item) {
                return item.id == id;
            });
        return it == m_items.end() ? npos
                                   : static_cast<size_t>(it - m_items.begin());
    }

    DropdownItemId DropdownModel::selection() const noexcept {
        return m_selection;
    }

    DropdownItemId DropdownModel::nextEnabled(DropdownItemId from,
                                              int direction) const noexcept {
        if (m_items.empty() || direction == 0) {
            return {};
        }
        size_t index = indexOf(from);
        if (index == npos) {
            index = direction > 0 ? m_items.size() - 1 : 0;
        }
        for (size_t attempt = 0; attempt < m_items.size(); ++attempt) {
            index = direction > 0
                        ? (index + 1) % m_items.size()
                        : (index + m_items.size() - 1) % m_items.size();
            if (m_items[index].enabled) {
                return m_items[index].id;
            }
        }
        return {};
    }

    bool DropdownModel::validate() const noexcept {
        HashSet<DropdownItemId> ids;
        for (const auto &item : m_items) {
            if (!item.id || !ids.insert(item.id).second) {
                return false;
            }
        }
        if (m_selection) {
            const auto *selected = find(m_selection);
            return selected != nullptr && selected->enabled;
        }
        return true;
    }

    DropdownModel::ChangedSignal &DropdownModel::changed() noexcept {
        return m_changed;
    }

    DropdownItemId DropdownModel::nearestEnabled(size_t index) const noexcept {
        if (m_items.empty()) {
            return {};
        }
        index = std::min(index, m_items.size() - 1);
        for (size_t current = index; current < m_items.size(); ++current) {
            if (m_items[current].enabled) {
                return m_items[current].id;
            }
        }
        for (size_t current = index; current-- > 0;) {
            if (m_items[current].enabled) {
                return m_items[current].id;
            }
        }
        return {};
    }

} // namespace Bess::UI
