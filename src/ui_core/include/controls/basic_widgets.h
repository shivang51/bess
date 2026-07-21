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
        LayoutAlignment crossAxisAlignment = LayoutAlignment::start;
        Core::Style::Padding padding{};
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
        [[nodiscard]] bool isPressed() const noexcept;

      private:
        std::string m_label;
        Activated m_activated;
        ButtonOptions m_options;
        Pressable m_pressable;
        bool m_intrinsicSizeDirty = true;
    };

    class BESS_API Spacer : public Widget {
      public:
        [[nodiscard]] std::string_view typeName() const noexcept override;
        [[nodiscard]] WidgetTraits traits() const noexcept override;
        void onMount(WidgetMountContext &context) override;
    };

} // namespace Bess::UI
