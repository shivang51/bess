#include "pages/main_page/scene_components/text_scene_component.h"
#include "bess_core/renderer/colors.h"
#include "bess_core/renderer/renderer_2d.h"
#include "bess_core/scene/scene_draw_context.h"
#include "bess_core/scene/scene_draw_helpers.h"
#include "bess_core/scene/scene_events.h"
#include "bess_core/scene/scene_state/components/styles/comp_style.h"
#include "bess_core/scene/widgets/scene_widgets.h"
#include "bess_core/scene/widgets/scene_widgets_internal.h"
#include "bess_core/settings/viewport_theme.h"
#include "imgui.h"
#include "ui/icons/FontAwesomeIcons_Remapped.h"
#include "ui/widgets/m_widgets.h"

namespace Icons = Bess::UI::Icons;
namespace Widgets = Bess::UI::Widgets;

namespace Bess::Canvas {
    TextComponent::TextComponent() {
        m_name = "New Text";
        m_icon = Icons::FontAwesomeIcons::FA_FONT;
        m_style.color = ViewportTheme::colors.componentBG;
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

        if (Widgets::TreeNode(0, "Text Properties")) {
            if (Widgets::TextBox("Text", m_data)) {
                m_isScaleDirty = true;
            }

            if (ImGui::InputScalar("Font Size", ImGuiDataType_U64, &m_size)) {
                m_isScaleDirty = true;
            }

            ImGui::ColorEdit4("Color", glm::value_ptr(m_foregroundColor));
            ImGui::TreePop();
        }
    }

    bool TextComponent::onMouseButton(const Events::MouseButtonEvent &e) {
        if (e.action == Events::MouseClickAction::doubleClick &&
            e.button == Events::MouseButton::left) {
            m_editMode = true;
            m_justEnteredEdit = true;
            m_editBuffer = m_data;
            return true;
        }

        return false;
    }

    void TextComponent::onJsonApplied() {
        NonSimSceneComponent::onJsonApplied();
        m_isScaleDirty = true;
    }

    void TextComponent::drawSchematic(SceneDrawContext &context) {
        draw(context);
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

        if (m_editMode) { // draw textbox in edit mode

            const auto textOffset = context.renderer->textCenterOffsetY(
                m_data, {.fontSize = (float)m_size});

            const auto offset = glm::vec3((m_transform.scale.x / 2.f) -
                                              Styles::componentStyles.paddingX,
                                          -textOffset,
                                          -0.0001f);

            const auto res = SceneWidgets::textBox(
                pickingId,
                &m_editBuffer,
                m_transform.position + offset,
                m_transform.scale,
                context,
                {
                    .backgroundColor = Core::Renderer::Colors::transparent,
                    .focusedBackgroundColor =
                        Core::Renderer::Colors::transparent,

                    .borderColor = Core::Renderer::Colors::transparent,
                    .focusedBorderColor = Core::Renderer::Colors::transparent,
                });

            if (m_justEnteredEdit) {
                m_justEnteredEdit = false;
                SceneWidgets::Detail::focusWidget(*context.sceneWidgetsState,
                                                  pickingId);
            }

            if (res.submitted) {
                m_data = m_editBuffer;
            }

            if (res.canceled || res.submitted) {
                m_editMode = false;
                m_isScaleDirty = true;
                m_editBuffer.clear();
            }

        } else {
            SceneDraw::drawText(context,
                                m_data,
                                m_transform.position,
                                m_size,
                                m_foregroundColor,
                                pickingId);
        }

        // draw background if selected
        if (m_isSelected || m_editMode) {
            SceneDraw::QuadStyle props;
            props.angle = m_transform.angle;
            props.borderRadius = Styles::componentStyles.borderRadius;
            props.borderSize = Styles::componentStyles.borderSize;
            props.borderColor = ViewportTheme::colors.selectedComp;
            props.shadow.enabled = true;

            const auto textOffset = context.renderer->textCenterOffsetY(
                m_data, {.fontSize = (float)m_size});

            const auto offset = glm::vec3((m_transform.scale.x / 2.f) -
                                              Styles::componentStyles.paddingX,
                                          -textOffset,
                                          -0.0001f);

            SceneDraw::drawQuad(context,
                                m_transform.position + offset,
                                m_transform.scale,
                                ViewportTheme::colors.componentBG,
                                pickingId,
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
} // namespace Bess::Canvas
