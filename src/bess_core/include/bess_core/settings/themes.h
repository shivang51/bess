#pragma once

#include "bess_core/style/bess_theme.h"
#include "bess_core/style/color_scheme.h"
#include <functional>
#include <string>
#include <unordered_map>

namespace Bess::Config {
    class Themes {
      public:
        Themes();

        void applyTheme(const std::string &themeName);
        void addTheme(const std::string &name,
                      const std::function<bool()> &callback);
        const std::unordered_map<std::string, std::function<bool()>> &
        getThemes() const;

        static std::shared_ptr<Core::Style::BessTheme> &getCurrentThemeRef();

        static std::shared_ptr<Core::Style::BessTheme> getCurrentTheme();

      private:
        void setDarkThemeColors();
        void setCatpuccinMochaColors();
        void setFluentUIColors();
        void setBessDarkColors();
        void setBessMinimalColors();
        void setModernDarkColors();

        void setBessLightColors();

        void setMaterialColors(Core::Style::Brightness brightness);

        void setGeometry();

        // theme name and a callback
        std::unordered_map<std::string, std::function<bool()>> m_themes;
    };
} // namespace Bess::Config
