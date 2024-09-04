#pragma once

#include "components/component.h"
#include "glm.hpp"
#include "json.hpp"
#include "settings/viewport_theme.h"

namespace Bess::Simulator::Components {

    class TextComponent : public Component {
      public:
        TextComponent() = default;
        TextComponent(const uuids::uuid &uid, int renderId, glm::vec3 position);

        ~TextComponent() = default;

        void generate(const glm::vec3 &pos) override;
        void deleteComponent() override;
        void render() override;
        void drawProperties() override;

        void setText(const std::string &value);
        const std::string &getText() const;

        void setFontSize(float size);
        float getFontSize() const;

        void setColor(const glm::vec4 &color);
        const glm::vec4 &getColor() const;

        static void fromJson(const nlohmann::json &j);
        nlohmann::json toJson() const;

        std::string getName() const override;

        std::string getRenderName() const override;

      private: // Properties
        std::string m_text = "Dummy Text";
        float m_fontSize = 12.0f;
        glm::vec4 m_color = ViewportTheme::textColor;

      private: // Events
        void onLeftClick(const glm::vec2 &pos);
    };

} // namespace Bess::Simulator::Components
