#include "settings/settings.h"

namespace Bess::Config {
    std::string Settings::getCurrentTheme() const {
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

    float Settings::getFontSize() const {
        return m_fontSize;
    }

    void Settings::setFontSize(float size) {
        if (m_fontRebuild)
            return;
        m_fontSize = size;
        m_fontRebuild = true;
    }

    float Settings::getScale() const {
        return m_scale;
    }

    void Settings::setScale(float scale) {
        if (m_fontRebuild)
            return;
        m_scale = scale;
        m_fontRebuild = true;
    }

    void Settings::setFontRebuild(bool rebuild) {
        m_fontRebuild = !rebuild;
    }

    void Settings::init() {
        m_themes = Themes();
        m_currentTheme = "Bess Dark";
        m_scale = 1.0f;
        m_fontSize = 18.0f;
        m_fontRebuild = true;
        m_fps = 60;
        m_frameTimeStep = TFrameTime(1000.0 / m_fps);
    }

    TFrameTime Settings::getFrameTimeStep() const {
        return m_frameTimeStep;
    }

    int Settings::getFps() const {
        return m_fps;
    }

    void Settings::setFps(int fps) {
        m_fps = fps;
        m_frameTimeStep = TFrameTime(1000.0 / m_fps);
    }

    Settings &Settings::instance() {
        static Settings instance;
        return instance;
    }

    bool Settings::shouldFontRebuild() const {
        return m_fontRebuild;
    }
} // namespace Bess::Config
