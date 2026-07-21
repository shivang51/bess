#pragma once

#include "common/bess_uuid.h"
#include "ext/vector_float2.hpp"

#include <compare>
#include <cstdint>
#include <functional>
#include <utility>

namespace Bess::UI {

    // StableId keeps identities from unrelated UI domains type-safe while
    // retaining UUID's process-independent representation. IDs are never
    // recycled, so a stale handle cannot accidentally address a new object.
    template <typename Tag> class StableId {
      public:
        constexpr StableId() noexcept = default;
        explicit constexpr StableId(UUID value) noexcept
            : m_value(static_cast<uint64_t>(value)) {
        }

        [[nodiscard]] static StableId generate() {
            return StableId{UUID{}};
        }

        [[nodiscard]] constexpr bool isValid() const noexcept {
            return m_value != 0;
        }

        [[nodiscard]] constexpr explicit operator bool() const noexcept {
            return isValid();
        }

        [[nodiscard]] constexpr UUID value() const noexcept {
            return UUID{m_value};
        }

        constexpr bool operator==(const StableId &) const noexcept = default;

        template <typename H> friend H AbslHashValue(H h, const StableId &id) {
            return H::combine(std::move(h), id.m_value);
        }

      private:
        uint64_t m_value = 0;
    };

    struct WidgetIdTag;
    struct ViewIdTag;
    struct TabIdTag;
    struct DockItemIdTag;
    struct DockNodeIdTag;
    struct DockHostIdTag;
    struct MenuIdTag;
    struct MenuItemIdTag;

    using WidgetId = StableId<WidgetIdTag>;
    using ViewId = StableId<ViewIdTag>;
    using TabId = StableId<TabIdTag>;
    using DockItemId = StableId<DockItemIdTag>;
    using DockNodeId = StableId<DockNodeIdTag>;
    using DockHostId = StableId<DockHostIdTag>;
    using MenuId = StableId<MenuIdTag>;
    using MenuItemId = StableId<MenuItemIdTag>;

    enum class WidgetVisibility : uint8_t {
        visible,
        // Hidden widgets participate in layout but are not painted or hit.
        hidden,
        // Collapsed widgets are also removed from layout.
        collapsed,
    };

    enum class UIEventPhase : uint8_t { capture, target, bubble };

    enum class WidgetInvalidation : uint8_t {
        none = 0,
        paint = 1 << 0,
        layout = 1 << 1,
    };

    constexpr WidgetInvalidation operator|(WidgetInvalidation lhs,
                                           WidgetInvalidation rhs) noexcept {
        return static_cast<WidgetInvalidation>(static_cast<uint8_t>(lhs) |
                                               static_cast<uint8_t>(rhs));
    }

    constexpr WidgetInvalidation &operator|=(WidgetInvalidation &lhs,
                                             WidgetInvalidation rhs) noexcept {
        lhs = lhs | rhs;
        return lhs;
    }

    [[nodiscard]] constexpr bool hasInvalidation(WidgetInvalidation value,
                                                 WidgetInvalidation flag) {
        return (static_cast<uint8_t>(value) & static_cast<uint8_t>(flag)) != 0;
    }

    // Widget geometry is expressed in target-local, centered screen space.
    // Input surfaces normally report top-left coordinates; WidgetTree performs
    // that conversion once before routing events.
    struct WidgetBounds {
        glm::vec2 center{0.f, 0.f};
        glm::vec2 size{0.f, 0.f};

        [[nodiscard]] glm::vec2 topLeft() const noexcept {
            return center - size * 0.5f;
        }

        [[nodiscard]] glm::vec2 bottomRight() const noexcept {
            return center + size * 0.5f;
        }

        [[nodiscard]] bool contains(glm::vec2 point) const noexcept {
            const auto min = topLeft();
            const auto max = bottomRight();
            return point.x >= min.x && point.x <= max.x && point.y >= min.y &&
                   point.y <= max.y;
        }

        [[nodiscard]] WidgetBounds inset(float amount) const noexcept {
            const auto insetSize =
                glm::max(size - glm::vec2{amount * 2.f}, glm::vec2{0.f});
            return {.center = center, .size = insetSize};
        }

        [[nodiscard]] bool empty() const noexcept {
            return size.x <= 0.f || size.y <= 0.f;
        }
    };

    struct WidgetTraits {
        bool focusable = false;
        bool hitTestVisible = true;
        bool clipChildren = false;
    };

} // namespace Bess::UI

namespace std {
    template <typename Tag> struct hash<Bess::UI::StableId<Tag>> {
        size_t operator()(const Bess::UI::StableId<Tag> &id) const noexcept {
            return hash<uint64_t>{}(static_cast<uint64_t>(id.value()));
        }
    };
} // namespace std
