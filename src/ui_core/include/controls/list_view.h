#pragma once

#include "controls/basic_widgets.h"
#include "controls/scroll_view.h"
#include "layout.h"
#include "ui_style.h"
#include "widget.h"
#include "widget_tree.h"

#include <concepts>
#include <memory>
#include <optional>
#include <span>
#include <utility>

namespace Bess::UI {

    // Retained linear list of arbitrary widgets. Children live under a private
    // content host that can be mutated after composition through
    // WidgetRef::mutate / WidgetTree::mutateWidget (emplaceItem, removeItem,
    // clearItems, reparent). Large data sets should prefer a virtualized
    // model-backed list; this control is the industry-standard small/medium
    // retained ListView.
    struct ListViewOptions {
        LayoutDirection direction = LayoutDirection::vertical;
        LayoutAlignment mainAxisAlignment = LayoutAlignment::start;
        LayoutAlignment crossAxisAlignment = LayoutAlignment::start;
        Core::Style::Padding padding{};
        float gap = 0.f;
        bool stretchWidth = true;
        bool stretchHeight = true;
        bool clipChildren = true;
        bool hitTestVisible = true;
        bool horizontalScroll = false;
        bool verticalScroll = true;
        std::optional<UIBoxStyle> style;
        std::optional<UIScrollStyle> scrollStyle;
    };

    class BESS_API ListView final : public Widget {
      public:
        explicit ListView(ListViewOptions options = {});

        [[nodiscard]] std::string_view typeName() const noexcept override;
        [[nodiscard]] WidgetTraits traits() const noexcept override;
        void onMount(WidgetMountContext &context) override;
        void onUnmount(WidgetTree &state, WidgetId id) override;
        void updateLayout(WidgetLayoutContext &context) override;
        void paint(WidgetPaintContext &context) const override;

        // Private composition host for list items. Declarative builders and
        // post-mount mutations both target this node.
        [[nodiscard]] WidgetId contentRoot() const noexcept;
        [[nodiscard]] WidgetId scrollRoot() const noexcept;
        [[nodiscard]] std::span<const WidgetId> items() const noexcept;
        [[nodiscard]] size_t itemCount() const noexcept;

        // Mutative item API intended for use inside WidgetRef::mutate /
        // WidgetTree::mutateWidget so invalidation and deferred removal stay
        // correct during event callbacks.
        template <typename T, typename... Args>
            requires std::derived_from<T, Widget>
        WidgetId emplaceItem(Args &&...args) {
            if (m_state == nullptr || !m_contentRoot ||
                !m_state->contains(m_contentRoot)) {
                return {};
            }
            return m_state->emplaceChild<T>(m_contentRoot,
                                            std::forward<Args>(args)...);
        }

        WidgetId addItem(std::unique_ptr<Widget> widget, size_t index = WidgetTree::append);
        bool removeItem(WidgetId id);
        bool clearItems();
        bool moveItem(WidgetId id, size_t index);

        [[nodiscard]] const ListViewOptions &options() const noexcept;
        void setStyle(std::optional<UIBoxStyle> style);
        void setGap(float gap) noexcept;
        void setPadding(Core::Style::Padding padding);

      private:
        void applyContentLayout() const;

        ListViewOptions m_options;
        WidgetTree *m_state = nullptr;
        WidgetId m_id;
        WidgetId m_scrollRoot;
        WidgetId m_contentRoot;
    };

} // namespace Bess::UI
