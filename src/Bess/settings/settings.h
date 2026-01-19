#pragma once

#include "application/types.h"
#include "settings/themes.h"
#include <string>

namespace Bess::Config {
    class Settings {
      public:
        static Settings &instance();

        void init();

        // theme
        std::string getCurrentTheme() const;
        const Themes &getThemes() const;
        void loadCurrentTheme();
        void applyTheme(const std::string &theme);

        // font size and scale
        float getFontSize() const;
        void setFontSize(float size);
        float getScale() const;
        void setScale(float scale);

        bool shouldFontRebuild() const;

        void setFontRebuild(bool rebuild);

        // No set function, frame time step is derived from fps
        TFrameTime getFrameTimeStep() const;

        // fps
        int getFps() const;
        void setFps(int fps);

      private:
        std::string m_currentTheme;
        float m_scale;
        float m_fontSize;
        bool m_fontRebuild;
        int m_fps;

      private:
        Themes m_themes;
        TFrameTime m_frameTimeStep;
    };
} // namespace Bess::Config
