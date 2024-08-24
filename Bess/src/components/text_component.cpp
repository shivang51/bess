#include "components/text_component.h"

#include "renderer/renderer.h"
#include "components_manager/components_manager.h"
#include "common/helpers.h"
#include "common/bind_helpers.h"
#include "settings/viewport_theme.h"

#include "imgui.h"
#include "ui/m_widgets.h"

#include "application_state.h"


namespace Bess::Simulator::Components {
    TextComponent::TextComponent(const uuids::uuid& uid, int renderId, glm::vec3 position)
        : Component(uid, renderId, position, ComponentType::text) {
        m_events[ComponentEventType::leftClick] = (OnLeftClickCB)BIND_FN_1(TextComponent::onLeftClick);
        m_name = "Text";
    }

    void TextComponent::generate(const glm::vec3& pos) {
        auto pos_ = pos;
        pos_.z = ComponentsManager::getNextZPos();
        auto uid = Common::Helpers::uuidGenerator.getUUID();
        auto rid = ComponentsManager::getNextRenderId();
        ComponentsManager::components[uid] = std::make_shared<TextComponent>(uid, rid, pos_);
        ComponentsManager::renderComponenets.push_back(uid);
        ComponentsManager::addCompIdToRId(rid, uid);
        ComponentsManager::addRenderIdToCId(rid, uid);

    }

    void TextComponent::deleteComponent() {}

    void TextComponent::render() {
            float width = Common::Helpers::calculateTextWidth(m_text, m_fontSize);
            float height = Common::Helpers::getAnyCharHeight(m_fontSize);

            auto pos = m_position;
            pos.x += width / 2.f;
            pos.y += height / 2.f;

            if (ApplicationState::getSelectedId() == m_uid) {
                pos.y -= 4.f;
                height += 8.f;
                width += 12.f;
            }

            pos.z -= ComponentsManager::zIncrement;
            Renderer2D::Renderer::quad(pos, { width, height }, ViewportTheme::backgroundColor, m_renderId, glm::vec4(8.f), ViewportTheme::componentBorderColor, glm::vec4(ApplicationState::getSelectedId() == m_uid ? 1.f: 0.f));
        
        Renderer2D::Renderer::text(m_text, m_position, m_fontSize, m_color, m_renderId);
    }

    void TextComponent::drawProperties() {
        ImGui::ColorEdit3("Color", &m_color[0]);
        ImGui::InputFloat("Font Size", &m_fontSize);
        UI::MWidgets::TextBox("Text", m_text);
    }

    void TextComponent::setText(const std::string& value) {
        m_text = value;
    }

    const std::string& TextComponent::getText() const {
        return m_text;
    }

    void TextComponent::setFontSize(float size) {
        m_fontSize = size;
    }

    float TextComponent::getFontSize() const {
        return m_fontSize;
    }

    void TextComponent::setColor(const glm::vec3& color) {
        m_color = color;
    }

    const glm::vec3& TextComponent::getColor() const {
        return m_color;
    }

    void TextComponent::fromJson(const nlohmann::json& j) {
        auto text = j.at("text").get<std::string>();
        float fontSize = j.at("fontSize").get<float>();
        auto color = Common::Helpers::DecodeVec3(j["color"]);
        auto pos = Common::Helpers::DecodeVec3(j.at("pos"));
        pos.z = ComponentsManager::getNextZPos();
        auto uid = Common::Helpers::strToUUID(j.at("uid").get<std::string>());

        auto rid = ComponentsManager::getNextRenderId();
        ComponentsManager::components[uid] = std::make_shared<TextComponent>(uid, rid, pos);
        ComponentsManager::renderComponenets.push_back(uid);
        ComponentsManager::addCompIdToRId(rid, uid);
        ComponentsManager::addRenderIdToCId(rid, uid);

        auto comp = std::dynamic_pointer_cast<TextComponent>(ComponentsManager::components[uid]);
        comp->setText(text);
        comp->setFontSize(fontSize);
        comp->setColor(color);
    }

    nlohmann::json TextComponent::toJson() const {
        nlohmann::json j;
        j["type"] = (int)m_type;
        j["uid"] = Common::Helpers::uuidToStr(m_uid);
        j["text"] = m_text;
        j["fontSize"] = m_fontSize;
        j["color"] = Common::Helpers::EncodeVec3(m_color);
        j["pos"] = Common::Helpers::EncodeVec3(m_position);
        return j;
    }

    std::string TextComponent::getName() const
    {
        return "Text";
    }

    std::string TextComponent::getRenderName() const
    {
        return "Text - " + m_text;
    }

    void TextComponent::onLeftClick(const glm::vec2& pos)
    {
        ApplicationState::setSelectedId(m_uid);
    }

} // namespace Bess::Simulator::Components
