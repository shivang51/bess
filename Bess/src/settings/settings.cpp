#include "settings/settings.h"
#include "ui/ui.h"

namespace Bess::Config {
    std::string Settings::getCurrentTheme() {
        return m_currentTheme;
    }

    const Themes& Settings::getThemes()
    {
        return m_themes;
    }

    void Settings::loadCurrentTheme()
    {
        m_themes.applyTheme(m_currentTheme);
    }

    void Settings::applyTheme(const std::string& theme)
    {
        if(m_currentTheme == theme) return;
        m_currentTheme = theme;
        m_themes.applyTheme(theme);
    }

    float Settings::getFontSize()
    {
        return m_fontSize;
    }

    void Settings::setFontSize(float size)
    {
        if(m_fontRebuild) return;
        m_fontSize = size;
        m_fontRebuild = true;
    }

    float Settings::getScale()
    {
        return m_scale;
    }

    void Settings::setScale(float scale)
    {
        if (m_fontRebuild) return;
        m_scale = scale;
        m_fontRebuild = true;
    }

    bool Settings::shouldFontRebuild()
    {
        return m_fontRebuild;
    }

    void Settings::setFontRebuild(bool rebuild)
    {
        m_fontRebuild = !rebuild;
    }

    void Settings::init()
    {
        m_themes = Themes();
        m_currentTheme = "Bess Dark";
    }

    std::string Settings::m_currentTheme;
    Themes Settings::m_themes;
    float Settings::m_scale = 1.1f;
    float Settings::m_fontSize = 16.f;
    bool Settings::m_fontRebuild = false;
}