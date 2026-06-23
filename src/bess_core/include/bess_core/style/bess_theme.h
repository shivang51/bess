#pragma once

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
        }

        [[nodiscard]] bool isDark() const;

        MAKE_GETTER_SETTER(ColorScheme, ColorScheme, m_colorScheme);

        Json::Value toJson() const;
        static BessTheme fromJson(const Json::Value &json);

        virtual const ElementStyle &getButtonStyle() const;
        virtual const ElementStyle &getTextInputStyle() const;
        virtual const TextStyle &getLabelSize() const;
        virtual const TextStyle &getHeaderSize() const;

      private:
        ColorScheme m_colorScheme;
        std::string m_name;
        ElementStyle m_buttonStyle;
        ElementStyle m_textInputStyle;
        TextStyle m_labelStyle, m_headerStyle;
    };
} // namespace Bess::Core::Style
