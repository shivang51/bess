#pragma once

#include "behaviors/pressable.h"
#include "common/bess_api.h"
#include "controls/basic_widgets.h"
#include "ui_style.h"
#include "widget.h"

#include <functional>
#include <optional>
#include <string>

namespace Bess::UI {

    struct TreeNodeOptions {
        bool expanded = true;
        bool collapsible = true;
        bool stretchWidth = true;
        float headerHeight = 24.f;
        // Extra left inset for children beyond the label text start. Content is
        // always aligned with the label (after disclosure/icon slots); this
        // value only adds further indent when nesting needs more separation.
        float indentation = 0.f;
        float horizontalPadding = 5.f;
        float disclosureSlotWidth = 17.f;
        float iconSlotWidth = 18.f;
        float contentGap = 0.f;
        // Label typography. When unset, matches theme.button.text (same default
        // size as Button / TextButton labels).
        std::optional<float> fontSize;
        std::optional<float> letterSpacing;
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
        [[nodiscard]] float fontSize(const UITheme &theme) const noexcept;
        void setFontSize(std::optional<float> fontSize);
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
        // Left edge of the painted label, measured from the node content box.
        // Children indent to this origin so nested rows align with the title.
        [[nodiscard]] float labelTextStart() const noexcept;
        [[nodiscard]] Core::Style::Padding contentPadding() const noexcept;
        [[nodiscard]] float resolvedFontSize(const UITheme &theme) const noexcept;
        [[nodiscard]] float
        resolvedLetterSpacing(const UITheme &theme) const noexcept;
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
