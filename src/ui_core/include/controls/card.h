#pragma once

#include "layout.h"
#include "ui_style.h"
#include "widget.h"

#include <optional>

namespace Bess::UI {

    // Flutter-style Material card: elevated surface chrome around a flex
    // content host. Children are composed under contentRoot() so padding and
    // clipping stay independent of the painted frame.
    struct CardOptions {
        std::optional<UIBoxStyle> style;
        Core::Style::Padding padding{12.f};
        float gap = 8.f;
        LayoutDirection direction = LayoutDirection::vertical;
        LayoutAlignment mainAxisAlignment = LayoutAlignment::start;
        LayoutAlignment crossAxisAlignment = LayoutAlignment::start;
        bool stretchWidth = true;
        bool stretchHeight = false;
        bool clipChildren = true;
        bool hitTestVisible = false;
    };

    class BESS_API Card final : public Widget {
      public:
        explicit Card(CardOptions options = {});

        [[nodiscard]] std::string_view typeName() const noexcept override;
        [[nodiscard]] WidgetTraits traits() const noexcept override;
        void onMount(WidgetMountContext &context) override;
        void onUnmount(WidgetTree &state, WidgetId id) override;
        void updateLayout(WidgetLayoutContext &context) override;
        void paint(WidgetPaintContext &context) const override;

        [[nodiscard]] WidgetId contentRoot() const noexcept;
        [[nodiscard]] const CardOptions &options() const noexcept;
        void setStyle(std::optional<UIBoxStyle> style);
        void setPadding(Core::Style::Padding padding);

      private:
        void applyContentLayout(LayoutNode &layout) const;

        CardOptions m_options;
        WidgetTree *m_state = nullptr;
        WidgetId m_id;
        WidgetId m_contentRoot;
    };

} // namespace Bess::UI
