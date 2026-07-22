#pragma once

#include "models/signal.h"
#include "ui_types.h"

#include <algorithm>
#include <span>
#include <string>
#include <vector>

namespace Bess::UI {

    struct DropdownItem {
        DropdownItemId id;
        std::string icon;
        std::string label;
        bool enabled = true;
    };

    enum class DropdownChangeKind : uint8_t {
        structure,
        selection,
        itemUpdated,
    };

    struct DropdownChange {
        DropdownChangeKind kind = DropdownChangeKind::structure;
        DropdownItemId item;
        DropdownItemId previousSelection;
        DropdownItemId selection;
    };

    class BESS_API DropdownModel {
      public:
        using ChangedSignal = Signal<DropdownChange>;
        static constexpr size_t npos = static_cast<size_t>(-1);

        DropdownItemId add(DropdownItem item, size_t index = npos);
        DropdownItemId add(std::string label,
                           std::string icon = {},
                           bool enabled = true,
                           size_t index = npos);
        bool remove(DropdownItemId id);
        bool select(DropdownItemId id);
        bool clearSelection();
        bool setEnabled(DropdownItemId id, bool enabled);
        bool setLabel(DropdownItemId id, std::string label);

        [[nodiscard]] std::span<const DropdownItem> items() const noexcept;
        [[nodiscard]] const DropdownItem *
        find(DropdownItemId id) const noexcept;
        [[nodiscard]] size_t indexOf(DropdownItemId id) const noexcept;
        [[nodiscard]] DropdownItemId selection() const noexcept;
        [[nodiscard]] DropdownItemId nextEnabled(DropdownItemId from,
                                                 int direction) const noexcept;
        [[nodiscard]] bool validate() const noexcept;
        [[nodiscard]] ChangedSignal &changed() noexcept;

      private:
        [[nodiscard]] DropdownItem *findMutable(DropdownItemId id) noexcept;
        [[nodiscard]] DropdownItemId
        nearestEnabled(size_t index) const noexcept;

        std::vector<DropdownItem> m_items;
        DropdownItemId m_selection;
        ChangedSignal m_changed;
    };

} // namespace Bess::UI
