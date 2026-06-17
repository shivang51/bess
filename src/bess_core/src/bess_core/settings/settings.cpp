#include "bess_core/settings/settings.h"
#include "common/bess_assert.h"
#include "bess_core/settings/viewport_theme.h"

namespace Bess::Config {

    void Settings::onInit() {
        m_themes = Themes();
        m_currentTheme = "Bess Minimal Dark";
        m_scale = 1.0f;
        m_fontSize = 18.0f;
        m_fontRebuild = true;
        m_fps = 60;
        m_frameTimeStep = TimeMs(1000.0 / m_fps);
    }

    void Settings::onDestroy() {
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

    TimeMs Settings::getFrameTimeStep() const {
        return m_frameTimeStep;
    }

    bool Settings::shouldFontRebuild() const {
        return m_fontRebuild;
    }

    void Settings::onFpsChange() {
#ifdef DEBUG // Unlimited FPS
        if (m_fps == 0) {
            m_frameTimeStep = TimeMs(0);
            return;
        }
#endif

        BESS_ASSERT(m_fps > 0, "FPS must be greater than 0");
        m_frameTimeStep = TimeMs(1000.0 / m_fps);
    }
} // namespace Bess::Config
