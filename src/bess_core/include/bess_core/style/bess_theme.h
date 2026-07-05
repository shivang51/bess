#pragma once

#include "bess_core/renderer/colors.h"
#include "bess_core/style/color_scheme.h"
#include "json/value.h"

namespace Bess::Core::Style {
    struct TextStyle {
        Color textColor;
        float fontSize{12.f};
    };

    struct Padding {
        float top{0.f};
        float right{0.f};
        float bottom{0.f};
        float left{0.f};

        constexpr Padding() = default;

        constexpr Padding(float top, float right, float bottom, float left)
            : top(top),
              right(right),
              bottom(bottom),
              left(left) {
        }

        constexpr Padding(float val)
            : top(val),
              right(val),
              bottom(val),
              left(val) {
        }

        static constexpr Padding fromHorizontal(float horizontal) {
            return Padding{0.f, horizontal, 0.f, horizontal};
        }

        static constexpr Padding fromVertical(float vertical) {
            return Padding{vertical, 0.f, vertical, 0.f};
        }

        static constexpr Padding fromSymmetric(float vertical,
                                               float horizontal) {
            return Padding{vertical, horizontal, vertical, horizontal};
        }

        constexpr glm::vec4 toVec4() const {
            return {top, right, bottom, left};
        }

        constexpr float horizontal() const {
            return right + left;
        }

        constexpr float vertical() const {
            return top + bottom;
        }

        constexpr static Padding onlyTop(float top) {
            return Padding{top, 0.f, 0.f, 0.f};
        }

        constexpr static Padding onlyRight(float right) {
            return Padding{0.f, right, 0.f, 0.f};
        }

        constexpr static Padding onlyBottom(float bottom) {
            return Padding{0.f, 0.f, bottom, 0.f};
        }

        constexpr static Padding onlyLeft(float left) {
            return Padding{0.f, 0.f, 0.f, left};
        }

        bool operator==(const Padding &other) const {
            return top == other.top && right == other.right &&
                   bottom == other.bottom && left == other.left;
        }

        bool operator!=(const Padding &other) const {
            return !(*this == other);
        }
    };

    typedef Padding Margin;
    typedef Padding BorderSize;

    struct Metrics {
        Padding padding{0};
        glm::vec4 borderRadius{0};
        BorderSize borderSize{0};
        Margin margin{0};
    };

    struct ElementStyle {
        Color backgroundColor;
        Color hoverColor;
        Color borderColor;
        Color activeColor;
        Metrics metrics;
        TextStyle textStyle;
    };

    class BessTheme {
      public:
        DEFAULT_CONTRS_VDES(BessTheme)

        constexpr BessTheme(const std::string_view &name,
                            const ColorScheme &colorScheme)
            : m_name(name),
              m_colorScheme(colorScheme) {
            const auto &colors = m_colorScheme.getColors();
            m_generalElementStyle = {
                .backgroundColor = colors.surfaceContainerLow,
                .hoverColor = colors.surfaceContainerHigh,
                .borderColor = colors.outlineVariant,
                .activeColor = colors.primary,
                .metrics =
                    Metrics{
                        .padding = Padding(8.f, 16.f, 8.f, 16.f),
                        .borderRadius = glm::vec4(4.f),
                        .borderSize = BorderSize(1.f),
                        .margin = Margin(2.f, 4.f, 2.f, 4.f),
                    },
                .textStyle =
                    {
                        .textColor = colors.onSurface,
                        .fontSize = 12.f,
                    },
            };
        }

        static constexpr std::shared_ptr<BessTheme> defaultTheme() {
            return std::make_shared<BessTheme>(
                "Default",
                ColorScheme::fromSeed(Core::Renderer::Colors::pastelBlue));
        }

        [[nodiscard]] bool isDark() const;

        MAKE_GETTER_SETTER(ColorScheme, ColorScheme, m_colorScheme);

        Json::Value toJson() const;
        static BessTheme fromJson(const Json::Value &json);

        virtual const ElementStyle &getButtonStyle() const;
        virtual const ElementStyle &getTextInputStyle() const;
        virtual const ElementStyle &generalElementStyle() const;
        virtual const TextStyle &getLabelSize() const;
        virtual const TextStyle &getHeaderSize() const;

      private:
        ColorScheme m_colorScheme;
        std::string m_name;
        ElementStyle m_buttonStyle;
        ElementStyle m_textInputStyle;
        TextStyle m_labelStyle, m_headerStyle;
        ElementStyle m_generalElementStyle;
    };
} // namespace Bess::Core::Style
