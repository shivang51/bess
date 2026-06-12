#include "non_sim_scene_component.h"
#include "bess_core/renderer/colors.h"
#include "bess_core/renderer/renderer_2d.h"
#include "gtc/type_ptr.hpp"
#include "icons/FontAwesomeIcons.h"
#include "scene/scene_draw_helpers.h"
#include "scene/scene_state/components/styles/comp_style.h"
#include "scene_draw_context.h"
#include "settings/viewport_theme.h"
#include "widgets/m_widgets.h"
#include <string>
#include <unordered_map>

namespace Bess::Canvas {
    std::vector<std::shared_ptr<SceneComponent>>
    NonSimSceneComponent::clone(const SceneState &sceneState) const {
        (void)sceneState;
        auto clonedComponent = std::make_shared<NonSimSceneComponent>(*this);
        prepareClone(*clonedComponent);
        return {clonedComponent};
    }

    std::shared_ptr<NonSimSceneComponent>
    NonSimSceneComponent::getInstance(std::type_index tIdx) {
        auto &m_contrRegistry = getContrRegistry();
        if (m_contrRegistry.contains(tIdx)) {
            return m_contrRegistry[tIdx]();
        }
        return nullptr;
    }

    void TextComponent::draw(SceneDrawContext &context) {
        if (m_isFirstDraw) {
            onFirstDraw(context);
            m_isFirstDraw = false;
        }

        auto &state = *context.sceneState;
        if (m_isScaleDirty) {
            glm::vec2 textSize = context.renderer->measureText(
                m_data, {.fontSize = (float)m_size});
            m_transform.scale.y =
                textSize.y + (Styles::componentStyles.paddingY * 2.f);
            m_transform.scale.x =
                textSize.x + (Styles::componentStyles.paddingX * 2.f);

            m_isScaleDirty = false;
        }

        const auto pickingId = PickingId{m_runtimeId, 0};
        SceneDraw::drawText(context, m_data, m_transform.position, m_size,
                            m_foregroundColor, pickingId);

        // draw background if selected
        if (m_isSelected) {
            SceneDraw::QuadStyle props;
            props.angle = m_transform.angle;
            props.borderRadius = m_style.borderRadius;
            props.borderSize = m_style.borderSize;
            props.borderColor = ViewportTheme::colors.selectedComp;
            props.shadow.enabled = true;
            props.shadow.offset = {0.f, 3.f};
            props.shadow.blur = 10.f;
            props.shadow.spread = 1.f;
            props.shadow.color =
                Core::Renderer::Color{0.f, 0.f, 0.f, 0.35f};

            const auto textOffset = context.renderer->textCenterOffsetY(
                m_data, {.fontSize = (float)m_size});

            const auto offset = glm::vec3((m_transform.scale.x / 2.f) -
                                              Styles::componentStyles.paddingX,
                                          -textOffset, -0.0001f);

            SceneDraw::drawQuad(context, m_transform.position + offset,
                                m_transform.scale, m_style.color, pickingId,
                                props);
        }
    }

    glm::vec2 TextComponent::calculateScale(const SceneState &state) {
        // This is just a fallback, we calculate correct text in draw function
        // when scale is dirty
        auto textSize = Core::Renderer::IRenderer2D::getTextRenderSize(
            m_data, {.fontSize = (float)m_size});
        textSize.y += Styles::componentStyles.paddingY * 2.f;
        textSize.x += Styles::componentStyles.paddingX * 2.f;
        return textSize;
    }
    std::type_index NonSimSceneComponent::getTypeIndex() {
        return {typeid(void)};
    }
    void NonSimSceneComponent::clearRegistry() {
        getRegistry().clear();
        getContrRegistry().clear();
    }

    std::unordered_map<std::type_index, std::string> &
    NonSimSceneComponent::getRegistry() {
        static std::unordered_map<std::type_index, std::string> registry;
        return registry;
    }

    std::unordered_map<std::type_index, NonSimSceneComponent::ContrFunc> &
    NonSimSceneComponent::getContrRegistry() {
        static std::unordered_map<std::type_index, ContrFunc> reg;
        return reg;
    }

    TextComponent::TextComponent() {
        m_name = "New Text";
        m_icon = UI::Icons::FontAwesomeIcons::FA_FONT;
        m_style.color = ViewportTheme::colors.componentBG;
        m_style.borderRadius = glm::vec4(6.f);
        m_style.borderSize = glm::vec4(1.f);
        m_style.color = ViewportTheme::colors.componentBG;
    }

    std::vector<std::shared_ptr<SceneComponent>>
    TextComponent::clone(const SceneState &sceneState) const {
        (void)sceneState;
        auto clonedComponent = std::make_shared<TextComponent>(*this);
        prepareClone(*clonedComponent);
        return {clonedComponent};
    }

    void TextComponent::drawPropertiesUI(SceneState &sceneState) {
        NonSimSceneComponent::drawPropertiesUI(sceneState);

        if (UI::Widgets::TreeNode(0, "Text Properties")) {
            if (UI::Widgets::TextBox("Text", m_data)) {
                m_isScaleDirty = true;
            }

            if (ImGui::InputScalar("Font Size", ImGuiDataType_U64, &m_size)) {
                m_isScaleDirty = true;
            }

            ImGui::ColorEdit4("Color", glm::value_ptr(m_foregroundColor));
            ImGui::TreePop();
        }
    }
} // namespace Bess::Canvas
