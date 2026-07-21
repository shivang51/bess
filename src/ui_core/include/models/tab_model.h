#pragma once

#include "models/signal.h"
#include "ui_types.h"

#include <algorithm>
#include <cstddef>
#include <optional>
#include <span>
#include <string>
#include <utility>
#include <vector>

namespace Bess::UI {

    enum class TabChangeKind : uint8_t {
        inserted,
        removed,
        moved,
        activated,
        updated,
    };

    template <typename Id> struct BasicTabItem {
        Id id;
        std::string title;
        WidgetId content;
        bool closable = true;
        bool enabled = true;
    };

    template <typename Id> struct BasicTabChange {
        TabChangeKind kind = TabChangeKind::updated;
        Id item;
        size_t fromIndex = static_cast<size_t>(-1);
        size_t toIndex = static_cast<size_t>(-1);
        Id previousActive;
        Id active;
    };

    template <typename Id> class BasicDetachedTab {
      public:
        BasicDetachedTab() = default;
        BasicDetachedTab(const BasicDetachedTab &) = delete;
        BasicDetachedTab &operator=(const BasicDetachedTab &) = delete;
        BasicDetachedTab(BasicDetachedTab &&) noexcept = default;
        BasicDetachedTab &operator=(BasicDetachedTab &&) noexcept = default;

        [[nodiscard]] explicit operator bool() const noexcept {
            return m_item.has_value();
        }

        [[nodiscard]] const BasicTabItem<Id> *get() const noexcept {
            return m_item ? &*m_item : nullptr;
        }

        [[nodiscard]] size_t sourceIndex() const noexcept {
            return m_sourceIndex;
        }

        [[nodiscard]] std::optional<BasicTabItem<Id>> take() && {
            auto item = std::move(m_item);
            m_item.reset();
            return item;
        }

      private:
        template <typename> friend class BasicTabModel;
        BasicDetachedTab(BasicTabItem<Id> item, size_t sourceIndex)
            : m_item(std::move(item)),
              m_sourceIndex(sourceIndex) {
        }

        std::optional<BasicTabItem<Id>> m_item;
        size_t m_sourceIndex = static_cast<size_t>(-1);
    };

    // Ordered, single-selection tab model. Every mutation establishes all
    // invariants before emitting exactly one notification, so observers never
    // see an item list and active selection from different logical states.
    template <typename Id> class BasicTabModel {
      public:
        using Item = BasicTabItem<Id>;
        using Change = BasicTabChange<Id>;
        using Detached = BasicDetachedTab<Id>;
        using ChangedSignal = Signal<Change>;

        static constexpr size_t npos = static_cast<size_t>(-1);

        [[nodiscard]] std::span<const Item> items() const noexcept {
            return m_items;
        }

        [[nodiscard]] size_t size() const noexcept {
            return m_items.size();
        }

        [[nodiscard]] bool empty() const noexcept {
            return m_items.empty();
        }

        [[nodiscard]] Id active() const noexcept {
            return m_active;
        }

        [[nodiscard]] const Item *find(Id id) const noexcept {
            const auto index = indexOf(id);
            return index != npos ? &m_items[index] : nullptr;
        }

        [[nodiscard]] Item *find(Id id) noexcept {
            const auto index = indexOf(id);
            return index != npos ? &m_items[index] : nullptr;
        }

        [[nodiscard]] size_t indexOf(Id id) const noexcept {
            const auto it =
                std::find_if(m_items.begin(),
                             m_items.end(),
                             [id](const Item &item) { return item.id == id; });
            return it == m_items.end()
                       ? npos
                       : static_cast<size_t>(it - m_items.begin());
        }

        Id add(std::string title,
               WidgetId content = {},
               bool closable = true,
               size_t index = npos) {
            return insert(Item{.id = Id::generate(),
                               .title = std::move(title),
                               .content = content,
                               .closable = closable},
                          index);
        }

        Id insert(Item item, size_t index = npos) {
            if (!item.id) {
                item.id = Id::generate();
            }
            if (find(item.id) != nullptr) {
                return {};
            }

            const size_t insertAt = index == npos
                                        ? m_items.size()
                                        : std::min(index, m_items.size());
            const Id previous = m_active;
            const Id insertedId = item.id;
            m_items.insert(m_items.begin() + static_cast<ptrdiff_t>(insertAt),
                           std::move(item));
            if (!m_active && m_items[insertAt].enabled) {
                m_active = insertedId;
            }
            m_changed.emit({.kind = TabChangeKind::inserted,
                            .item = insertedId,
                            .toIndex = insertAt,
                            .previousActive = previous,
                            .active = m_active});
            return insertedId;
        }

        bool attach(Detached &&detached, size_t index = npos) {
            if (!detached.m_item || find(detached.m_item->id) != nullptr) {
                return false;
            }
            const Id id = detached.m_item->id;
            Item item = std::move(*detached.m_item);
            const Id inserted = insert(std::move(item), index);
            if (!inserted) {
                return false;
            }
            detached.m_item.reset();
            return inserted == id;
        }

        [[nodiscard]] Detached detach(Id id) {
            const size_t index = indexOf(id);
            if (index == npos) {
                return {};
            }

            const Id previous = m_active;
            Item item = std::move(m_items[index]);
            m_items.erase(m_items.begin() + static_cast<ptrdiff_t>(index));
            if (m_active == id) {
                m_active = nearestEnabled(index);
            }
            m_changed.emit({.kind = TabChangeKind::removed,
                            .item = id,
                            .fromIndex = index,
                            .previousActive = previous,
                            .active = m_active});
            return Detached{std::move(item), index};
        }

        bool remove(Id id) {
            auto detached = detach(id);
            return static_cast<bool>(detached);
        }

        bool move(Id id, size_t toIndex) {
            const size_t from = indexOf(id);
            if (from == npos || m_items.empty()) {
                return false;
            }
            toIndex = std::min(toIndex, m_items.size() - 1);
            if (from == toIndex) {
                return true;
            }
            Item item = std::move(m_items[from]);
            m_items.erase(m_items.begin() + static_cast<ptrdiff_t>(from));
            m_items.insert(m_items.begin() + static_cast<ptrdiff_t>(toIndex),
                           std::move(item));
            m_changed.emit({.kind = TabChangeKind::moved,
                            .item = id,
                            .fromIndex = from,
                            .toIndex = toIndex,
                            .previousActive = m_active,
                            .active = m_active});
            return true;
        }

        bool activate(Id id) {
            const auto *item = find(id);
            if (item == nullptr || !item->enabled) {
                return false;
            }
            if (m_active == id) {
                return true;
            }
            const Id previous = m_active;
            m_active = id;
            m_changed.emit({.kind = TabChangeKind::activated,
                            .item = id,
                            .previousActive = previous,
                            .active = m_active});
            return true;
        }

        bool setEnabled(Id id, bool enabled) {
            const size_t index = indexOf(id);
            if (index == npos || m_items[index].enabled == enabled) {
                return index != npos;
            }
            const Id previous = m_active;
            m_items[index].enabled = enabled;
            if (!enabled && m_active == id) {
                m_active = nearestEnabled(index);
            } else if (enabled && !m_active) {
                m_active = id;
            }
            m_changed.emit({.kind = TabChangeKind::updated,
                            .item = id,
                            .fromIndex = index,
                            .toIndex = index,
                            .previousActive = previous,
                            .active = m_active});
            return true;
        }

        bool setTitle(Id id, std::string title) {
            const size_t index = indexOf(id);
            if (index == npos) {
                return false;
            }
            if (m_items[index].title == title) {
                return true;
            }
            m_items[index].title = std::move(title);
            m_changed.emit({.kind = TabChangeKind::updated,
                            .item = id,
                            .fromIndex = index,
                            .toIndex = index,
                            .previousActive = m_active,
                            .active = m_active});
            return true;
        }

        [[nodiscard]] Id nextEnabled(Id from, int direction) const noexcept {
            if (m_items.empty() || direction == 0) {
                return {};
            }
            size_t index = indexOf(from);
            if (index == npos) {
                index = direction > 0 ? m_items.size() - 1 : 0;
            }
            for (size_t attempt = 0; attempt < m_items.size(); ++attempt) {
                if (direction > 0) {
                    index = (index + 1) % m_items.size();
                } else {
                    index = (index + m_items.size() - 1) % m_items.size();
                }
                if (m_items[index].enabled) {
                    return m_items[index].id;
                }
            }
            return {};
        }

        [[nodiscard]] bool validate() const noexcept {
            HashSet<Id> ids;
            for (const auto &item : m_items) {
                if (!item.id || !ids.insert(item.id).second) {
                    return false;
                }
            }
            if (m_active) {
                const auto *activeItem = find(m_active);
                if (activeItem == nullptr || !activeItem->enabled) {
                    return false;
                }
            }
            return true;
        }

        [[nodiscard]] ChangedSignal &changed() noexcept {
            return m_changed;
        }

      private:
        [[nodiscard]] Id nearestEnabled(size_t removedIndex) const noexcept {
            if (m_items.empty()) {
                return {};
            }
            const size_t next = std::min(removedIndex, m_items.size() - 1);
            for (size_t i = next; i < m_items.size(); ++i) {
                if (m_items[i].enabled) {
                    return m_items[i].id;
                }
            }
            for (size_t i = next; i-- > 0;) {
                if (m_items[i].enabled) {
                    return m_items[i].id;
                }
            }
            return {};
        }

        std::vector<Item> m_items;
        Id m_active;
        ChangedSignal m_changed;
    };

    using TabItem = BasicTabItem<TabId>;
    using TabChange = BasicTabChange<TabId>;
    using DetachedTab = BasicDetachedTab<TabId>;
    using TabModel = BasicTabModel<TabId>;

} // namespace Bess::UI
