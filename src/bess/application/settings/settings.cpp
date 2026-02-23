#include "settings/settings.h"
#include "settings/viewport_theme.h"

namespace Bess::Config {

    void Settings::init() {
        m_themes = Themes();
        m_currentTheme = "Bess Dark";
        m_scale = 1.0f;
        m_fontSize = 18.0f;
        m_fontRebuild = true;
        m_fps = 60;
        m_frameTimeStep = TFrameTime(1000.0 / m_fps);
    }

    void Settings::cleanup() {
        ViewportTheme::cleanup();
    }

    const std::string &Settings::getCurrentTheme() const {
        return m_currentTheme;
    }

    const Themes &Settings::getThemes() const {
        return m_themes;
    }

    void Settings::loadCurrentTheme() {
        m_themes.applyTheme(m_currentTheme);
    }

    void Settings::applyTheme(const std::string &theme) {
        if (m_currentTheme == theme)
            return;
        m_currentTheme = theme;
        m_themes.applyTheme(theme);
    }

    void Settings::setFontRebuild(bool rebuild) {
        m_fontRebuild = !rebuild;
    }

    TFrameTime Settings::getFrameTimeStep() const {
        return m_frameTimeStep;
    }

    Settings &Settings::instance() {
        static Settings instance;
        return instance;
    }

    bool Settings::shouldFontRebuild() const {
        return m_fontRebuild;
    }
} // namespace Bess::Config
