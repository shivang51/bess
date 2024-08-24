#pragma once

#include <string>
#include <unordered_map>
#include <functional>

namespace Bess::Config {
    class Themes {
    public:
        Themes();

        void applyTheme(const std::string& theme);
        void addTheme(const std::string& name, const std::function<void()>& callback);
        const std::unordered_map<std::string, std::function<void()>>& getThemes() const;
    private:
        static void setModernColors();
        static void setMaterialYouColors();
        static void setDarkThemeColors();
        static void setBessDarkThemeColors();
        static void setCatpuccinMochaColors();

        // theme name and a void callback
        std::unordered_map<std::string, std::function<void()>> m_themes = {};
    };
}