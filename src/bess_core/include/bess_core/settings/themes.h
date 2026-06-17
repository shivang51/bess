#pragma once

#include "bess_core/style/color_scheme.h"
#include <functional>
#include <string>
#include <unordered_map>

namespace Bess::Config {
    class Themes {
      public:
        Themes();

        void applyTheme(const std::string &theme);
        void addTheme(const std::string &name,
                      const std::function<bool()> &callback);
        const std::unordered_map<std::string, std::function<bool()>> &
        getThemes() const;

      private:
        static void setDarkThemeColors();
        static void setCatpuccinMochaColors();
        static void setFluentUIColors();
        static void setBessDarkColors();
        static void setBessMinimalColors();
        static void setModernDarkColors();

        static void setBessLightColors();

        static void setMaterialColors(Core::Style::Brightness brightness);

        static void setGeometry();

        // theme name and a void callback
        std::unordered_map<std::string, std::function<bool()>> m_themes;
    };
} // namespace Bess::Config
