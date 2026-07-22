#pragma once

#include "behaviors/pressable.h"
#include "layout.h"
#include "ui_painter.h"
#include "ui_style.h"
#include "widget.h"

#include <functional>
#include <optional>
#include <string>

namespace Bess::UI {

    struct FlexContainerOptions {
        LayoutDirection direction = LayoutDirection::horizontal;
        LayoutAlignment mainAxisAlignment = LayoutAlignment::start;
        LayoutAlignment crossAxisAlignment = LayoutAlignment::center;
        Core::Style::Padding padding{};
        float gap = 0.f;
        bool stretchWidth = true;
        bool stretchHeight = true;
        bool clipChildren = false;
        bool hitTestVisible = false;
    };

    class BESS_API FlexContainer : public Widget {
      public:
        explicit FlexContainer(FlexContainerOptions options = {});

        [[nodiscard]] std::string_view typeName() const noexcept override;
        [[nodiscard]] WidgetTraits traits() const noexcept override;
        void onMount(WidgetMountContext &context) override;

      private:
        FlexContainerOptions m_options;
    };

    enum class StackAlignment : uint8_t { start, center, end, stretch };

    struct StackContainerOptions {
        StackAlignment horizontalAlignment = StackAlignment::stretch;
        StackAlignment verticalAlignment = StackAlignment::stretch;
        Core::Style::Padding padding{};
        bool stretchWidth = true;
        bool stretchHeight = true;
        bool clipChildren = true;
        bool hitTestVisible = false;
    };

    // Overlay container: every child occupies the same padded content slot.
    // Children paint in declaration order, so later siblings are naturally
    // above earlier siblings unless an explicit layout Z value says otherwise.
    class BESS_API StackContainer : public Widget {
      public:
        explicit StackContainer(StackContainerOptions options = {});

        [[nodiscard]] std::string_view typeName() const noexcept override;
        [[nodiscard]] WidgetTraits traits() const noexcept override;
        void onMount(WidgetMountContext &context) override;
        void arrange(WidgetArrangeContext &context) override;

        [[nodiscard]] StackAlignment horizontalAlignment() const noexcept;
        [[nodiscard]] StackAlignment verticalAlignment() const noexcept;
        void setHorizontalAlignment(StackAlignment alignment) noexcept;
        void setVerticalAlignment(StackAlignment alignment) noexcept;
        void setAlignment(StackAlignment horizontal,
                          StackAlignment vertical) noexcept;

      private:
        StackContainerOptions m_options;
    };

    struct SurfaceOptions {
        std::optional<UIBoxStyle> style;
        bool clipChildren = true;
        bool hitTestVisible = false;
    };

    class BESS_API Surface : public Widget {
      public:
        explicit Surface(SurfaceOptions options = {});

        [[nodiscard]] std::string_view typeName() const noexcept override;
        [[nodiscard]] WidgetTraits traits() const noexcept override;
        void paint(WidgetPaintContext &context) const override;

      private:
        SurfaceOptions m_options;
    };

    struct LabelOptions {
        std::optional<UITextStyle> style;
        // Typography can be adjusted without replacing the theme-owned text
        // color. An explicit style still remains available for intentional
        // fully custom rendering.
        std::optional<float> fontSize;
        HorizontalTextAlignment horizontal = HorizontalTextAlignment::start;
        VerticalTextAlignment vertical = VerticalTextAlignment::center;
        bool autoSize = true;
    };

    class BESS_API Label : public Widget {
      public:
        explicit Label(std::string text, LabelOptions options = {});

        [[nodiscard]] std::string_view typeName() const noexcept override;
        [[nodiscard]] WidgetTraits traits() const noexcept override;
        void onMount(WidgetMountContext &context) override;
        void updateLayout(WidgetLayoutContext &context) override;
        void paint(WidgetPaintContext &context) const override;

        [[nodiscard]] const std::string &text() const noexcept;
        void setText(std::string text);

      private:
        std::string m_text;
        LabelOptions m_options;
        bool m_intrinsicSizeDirty = true;
    };

    struct ButtonOptions {
        std::optional<UIInteractiveStyle> style;
        bool autoSize = true;
    };

    class BESS_API Button : public Widget {
      public:
        using Activated = std::function<void()>;
        using ActivatedWithEvent = std::function<void(const UIEvent &)>;

        explicit Button(std::string label,
                        Activated activated = {},
                        ButtonOptions options = {});

        [[nodiscard]] std::string_view typeName() const noexcept override;
        [[nodiscard]] WidgetTraits traits() const noexcept override;
        void onMount(WidgetMountContext &context) override;
        void updateLayout(WidgetLayoutContext &context) override;
        void paint(WidgetPaintContext &context) const override;
        UIEventReply onEvent(WidgetEventContext &context,
                             const UIEvent &event) override;

        [[nodiscard]] const std::string &label() const noexcept;
        void setLabel(std::string label);
        void setActivated(Activated activated);
        // Event-aware activation is useful for command bindings which need
        // modifier state. It coexists with the simple callback so ordinary
        // buttons keep their compact API.
        void setActivatedWithEvent(ActivatedWithEvent activated);
        [[nodiscard]] bool isPressed() const noexcept;

      private:
        std::string m_label;
        Activated m_activated;
        ActivatedWithEvent m_activatedWithEvent;
        ButtonOptions m_options;
        Pressable m_pressable;
        bool m_intrinsicSizeDirty = true;
    };

    struct SpacerOptions {
        // Remaining main-axis space is distributed according to this factor.
        float flex = 1.f;
        // A flexible spacer can also establish a cross-axis minimum, e.g. a
        // minimum row height, without becoming a fixed main-axis gap.
        glm::vec2 minimumSize{0.f};
    };

    class BESS_API Spacer : public Widget {
      public:
        explicit Spacer(SpacerOptions options = {});

        [[nodiscard]] std::string_view typeName() const noexcept override;
        [[nodiscard]] WidgetTraits traits() const noexcept override;
        void onMount(WidgetMountContext &context) override;

      private:
        SpacerOptions m_options;
    };

    // Fixed empty space along the current flex parent's main axis. A Gap in a
    // row has width; a Gap in a column has height. It follows the parent when
    // reparented or when the parent's direction changes.
    class BESS_API Gap : public Widget {
      public:
        explicit Gap(float extent);

        [[nodiscard]] std::string_view typeName() const noexcept override;
        [[nodiscard]] WidgetTraits traits() const noexcept override;
        void onMount(WidgetMountContext &context) override;
        void updateLayout(WidgetLayoutContext &context) override;

      private:
        void applyLayout(WidgetTree &state, WidgetId id, LayoutNode &layout);

        float m_extent = 0.f;
        std::optional<bool> m_horizontal;
    };

} // namespace Bess::UI
