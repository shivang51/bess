#pragma once

#include "bess_core/style/color_scheme.h"
#include "json/value.h"

namespace Bess::Core::Style {
    class Theme {
      public:
        DEFAULT_CONTRS(Theme)

        constexpr Theme(const std::string_view &name,
                        const ColorScheme &colorScheme)
            : m_name(name),
              m_colorScheme(colorScheme) {
        }

        [[nodiscard]] bool isDark() const;

        MAKE_GETTER_SETTER(ColorScheme, ColorScheme, m_colorScheme);

        Json::Value toJson() const;
        static Theme fromJson(const Json::Value &json);

      private:
        ColorScheme m_colorScheme;
        std::string m_name;
    };
} // namespace Bess::Core::Style
