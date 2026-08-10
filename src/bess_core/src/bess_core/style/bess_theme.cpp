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

    const TextStyle &BessTheme::getLabelSize() const {
        return m_labelStyle;
    }

    const TextStyle &BessTheme::getHeaderSize() const {
        return m_headerStyle;
    }

    const ElementStyle &BessTheme::getTextInputStyle() const {
        return m_textInputStyle;
    }

    const ElementStyle &BessTheme::generalElementStyle() const {
        return m_generalElementStyle;
    }

    const ElementStyle &BessTheme::getButtonStyle() const {
        return m_buttonStyle;
    }

} // namespace Bess::Core::Style
