#pragma once

#include "bess_core/renderer/colors.h"
#include "bess_core/style/color_scheme.h"
#include "json/value.h"

namespace Bess::Core::Style {
    struct TextStyle {
        Color textColor;
        float fontSize{12.f};
    };

    struct Metrics {
        glm::vec4 padding{0};
        glm::vec4 borderRadius{0};
        glm::vec4 borderSize{0};
        glm::vec4 margin{0};
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
                        .padding = glm::vec4(8.f, 16.f, 8.f, 16.f),
                        .borderRadius = glm::vec4(4.f),
                        .borderSize = glm::vec4(1.f),
                        .margin = glm::vec4(2.f, 4.f, 2.f, 4.f),
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
