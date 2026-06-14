#include "bess_core/style/bess_theme.h"
#include "json/value.h"

namespace Bess::Core::Style {
    bool Theme::isDark() const {
        return m_colorScheme.getBrightness() == Brightness::dark;
    }

    Json::Value Theme::toJson() const {
        Json::Value json;
        json["name"] = m_name;
        json["colorScheme"] = m_colorScheme.toJson();
        return json;
    }

    Theme Theme::fromJson(const Json::Value &json) {
    }
} // namespace Bess::Core::Style
