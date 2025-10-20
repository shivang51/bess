#pragma once

#include "settings/themes.h"
#include <string>

namespace Bess::Config {
	class Settings
	{
	public:
		static void init();
		
		// theme
		static std::string getCurrentTheme();
		static const Themes& getThemes();
		static void loadCurrentTheme();
		static void applyTheme(const std::string& theme);

		// font size and scale
		static float getFontSize();
		static void setFontSize(float size);
		static float getScale();
		static void setScale(float scale);
	
		static bool shouldFontRebuild();
		static void setFontRebuild(bool rebuild);

	private:
		static std::string m_currentTheme;
		static float m_scale;
		static float m_fontSize;
		static bool m_fontRebuild;

	private:
		static Themes m_themes;
	};
}