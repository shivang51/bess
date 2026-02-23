#pragma once

#include "application/app_types.h"
#include "application/settings/themes.h"
#include <string>

namespace Bess::Config {

#define MAKE_GETTER_SETTER(type, name, member) \
    type get##name() const { return member; }  \
    void set##name(type value) { (member) = value; }

    class Settings {
      public:
        static Settings &instance();

        void init();
        void cleanup();

        // theme
        const std::string &getCurrentTheme() const;
        const Themes &getThemes() const;
        void loadCurrentTheme();
        void applyTheme(const std::string &theme);

        // font size and scale
        MAKE_GETTER_SETTER(float, Scale, m_scale)
        MAKE_GETTER_SETTER(float, FontSize, m_fontSize)
        MAKE_GETTER_SETTER(int, Fps, m_fps)
        MAKE_GETTER_SETTER(bool, ShowStatsWindow, m_showStatsWindow)

        bool shouldFontRebuild() const;

        void setFontRebuild(bool rebuild);

        // No set function, frame time step is derived from fps
        TFrameTime getFrameTimeStep() const;

      private:
        std::string m_currentTheme;
        float m_scale;
        float m_fontSize;
        bool m_fontRebuild;
        int m_fps;
        bool m_showStatsWindow;

      private:
        Themes m_themes;
        TFrameTime m_frameTimeStep;
    };
} // namespace Bess::Config
