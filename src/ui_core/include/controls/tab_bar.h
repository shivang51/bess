#pragma once

#include "behaviors/pressable.h"
#include "models/tab_model.h"
#include "ui_style.h"
#include "widget.h"

#include <memory>
#include <optional>
#include <span>
#include <vector>

namespace Bess::UI {

    struct TabStripMetrics {
        float height = 28.f;
        float minimumWidth = 72.f;
        float maximumWidth = 220.f;
        float horizontalPadding = 12.f;
        glm::vec2 stripPadding{0.f};
        float gap = 0.f;
    };

    struct TabStripRegion {
        size_t index = 0;
        WidgetBounds bounds;
        WidgetBounds labelBounds;
        WidgetBounds trailingActionBounds;
    };

    // Pure layout solver shared by standalone TabBar and DockSpace stacks.
    class BESS_API TabStripLayout {
      public:
        [[nodiscard]] static std::vector<TabStripRegion>
        calculate(WidgetBounds bounds,
                  size_t tabCount,
                  const TabStripMetrics &metrics,
                  float scrollOffset = 0.f);

        // Reserves a square action at the trailing edge and returns a copy
        // with a shortened label. This is shared by dock-tab close buttons
        // and future tab actions without coupling the pure strip solver to a
        // particular tab model.
        [[nodiscard]] static TabStripRegion
        withTrailingAction(TabStripRegion region,
                           float actionSize,
                           float gap,
                           float trailingPadding) noexcept;

        [[nodiscard]] static std::optional<size_t>
        hitTest(std::span<const TabStripRegion> regions,
                glm::vec2 position) noexcept;
    };

    struct TabBarOptions {
        std::optional<UITabStyle> style;
    };

    class BESS_API TabBar : public Widget {
      public:
        explicit TabBar(std::shared_ptr<TabModel> model,
                        TabBarOptions options = {});

        [[nodiscard]] std::string_view typeName() const noexcept override;
        [[nodiscard]] WidgetTraits traits() const noexcept override;
        void onMount(WidgetMountContext &context) override;
        void onUnmount(WidgetTree &state, WidgetId id) override;
        void updateLayout(WidgetLayoutContext &context) override;
        void paint(WidgetPaintContext &context) const override;
        UIEventReply onEvent(WidgetEventContext &context,
                             const UIEvent &event) override;

        [[nodiscard]] std::shared_ptr<TabModel> model() const noexcept;
        [[nodiscard]] TabId hoveredTab() const noexcept;

      private:
        [[nodiscard]] const UITabStyle &style(const WidgetTree &state) const;
        [[nodiscard]] std::vector<TabStripRegion>
        regions(WidgetBounds bounds, const WidgetTree &state) const;
        [[nodiscard]] TabId tabAt(WidgetBounds bounds,
                                  const WidgetTree &state,
                                  glm::vec2 position) const;
        [[nodiscard]] TabId closeAt(WidgetBounds bounds,
                                    const WidgetTree &state,
                                    glm::vec2 position) const;

        std::shared_ptr<TabModel> m_model;
        TabBarOptions m_options;
        Pressable m_pressable;
        TabId m_hoveredTab;
        TabId m_pressedTab;
        TabId m_hoveredClose;
        TabId m_pressedClose;
        TabModel::ChangedSignal::Connection m_modelConnection;
    };

} // namespace Bess::UI
