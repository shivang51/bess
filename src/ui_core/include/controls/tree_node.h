#pragma once

#include "behaviors/pressable.h"
#include "common/bess_api.h"
#include "controls/basic_widgets.h"
#include "widget.h"

#include <functional>
#include <string>

namespace Bess::UI {

    struct TreeNodeOptions {
        bool expanded = true;
        bool collapsible = true;
        bool stretchWidth = true;
        float headerHeight = 24.f;
        float indentation = 16.f;
        float horizontalPadding = 5.f;
        float disclosureSlotWidth = 17.f;
        float iconSlotWidth = 18.f;
        float contentGap = 0.f;
        // Optional font-atlas icon rendered between the disclosure indicator
        // and label. An empty value omits the slot entirely.
        std::string icon;
    };

    // A lightweight retained disclosure node for static/small nested trees.
    // TreeNode owns a private content host and collapses only that host, so a
    // descendant's own hidden/collapsed state is never overwritten. Large or
    // virtualized data sets should layer a model-backed TreeView over this
    // interaction vocabulary rather than instantiating every node eagerly.
    class BESS_API TreeNode final : public Widget {
      public:
        using ExpandedChanged = std::function<void(bool)>;

        explicit TreeNode(std::string label,
                          TreeNodeOptions options = {},
                          ExpandedChanged expandedChanged = {});

        [[nodiscard]] std::string_view typeName() const noexcept override;
        [[nodiscard]] WidgetTraits traits() const noexcept override;
        void onMount(WidgetMountContext &context) override;
        void onUnmount(WidgetTree &state, WidgetId id) override;
        void updateLayout(WidgetLayoutContext &context) override;
        void paint(WidgetPaintContext &context) const override;
        [[nodiscard]] bool hitTest(WidgetBounds bounds,
                                   glm::vec2 position) const noexcept override;
        [[nodiscard]] CursorIcon
        cursor(const WidgetCursorContext &context) const noexcept override;
        UIEventReply onEvent(WidgetEventContext &context,
                             const UIEvent &event) override;

        [[nodiscard]] const std::string &label() const noexcept;
        void setLabel(std::string label);
        [[nodiscard]] const std::string &icon() const noexcept;
        void setIcon(std::string icon);
        [[nodiscard]] bool isExpanded() const noexcept;
        bool setExpanded(bool expanded);
        bool toggle();
        void setExpandedChanged(ExpandedChanged changed);

        // Declarative builders should compose node children beneath this
        // private layout host rather than directly beneath TreeNode.
        [[nodiscard]] WidgetId contentRoot() const noexcept;

      private:
        [[nodiscard]] WidgetBounds
        headerBounds(WidgetBounds bounds) const noexcept;
        bool changeExpanded(bool expanded, bool notify);
        void syncContentVisibility();

        std::string m_label;
        TreeNodeOptions m_options;
        ExpandedChanged m_expandedChanged;
        Pressable m_pressable;
        WidgetTree *m_state = nullptr;
        WidgetId m_id;
        WidgetId m_contentRoot;
    };

} // namespace Bess::UI
