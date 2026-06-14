#include "bess_core/style/bess_theme.h"
#include "json/value.h"

namespace Bess::Core::Style {
    bool BessTheme::isDark() const {
        return m_colorScheme.getBrightness() == Brightness::dark;
    }

    Json::Value BessTheme::toJson() const {
        Json::Value json;
        json["name"] = m_name;
        json["colorScheme"] = m_colorScheme.toJson();
        return json;
    }

    BessTheme BessTheme::fromJson(const Json::Value &json) {
        std::string name = json["name"].asString();
        ColorScheme colorScheme = ColorScheme::fromJson(json["colorScheme"]);
        return {name, colorScheme};
    }
} // namespace Bess::Core::Style
