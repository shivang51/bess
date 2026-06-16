#include "non_sim_scene_component.h"
#include "bess_core/renderer/renderer_2d.h"
#include "gtc/type_ptr.hpp"
#include "icons/FontAwesomeIcons_Remapped.h"
#include "scene/scene_draw_helpers.h"
#include "scene/scene_state/components/styles/comp_style.h"
#include "scene/widgets/scene_widgets.h"
#include "scene_draw_context.h"
#include "settings/viewport_theme.h"
#include "widgets/m_widgets.h"
#include <array>
#include <string>
#include <string_view>
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
        SceneDraw::drawText(context,
                            m_data,
                            m_transform.position,
                            m_size,
                            m_foregroundColor,
                            pickingId);

        // draw background if selected
        if (m_isSelected) {
            SceneDraw::QuadStyle props;
            props.angle = m_transform.angle;
            props.borderRadius = Styles::componentStyles.borderRadius;
            props.borderSize = Styles::componentStyles.borderSize;
            props.borderColor = ViewportTheme::colors.selectedComp;
            props.shadow.enabled = true;
            props.shadow.offset = {0.f, 3.f};
            props.shadow.blur = 10.f;
            props.shadow.spread = 1.f;
            props.shadow.color = ViewportTheme::colors.componentBG;

            const auto textOffset = context.renderer->textCenterOffsetY(
                m_data, {.fontSize = (float)m_size});

            const auto offset = glm::vec3((m_transform.scale.x / 2.f) -
                                              Styles::componentStyles.paddingX,
                                          -textOffset,
                                          -0.0001f);

            SceneDraw::drawQuad(context,
                                m_transform.position + offset,
                                m_transform.scale,
                                m_style.color,
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

    WidgetsTestComponent::WidgetsTestComponent() {
        m_name = "Widgets Test";
        m_icon = UI::Icons::FontAwesomeIcons::FA_FLASK;
        m_style.color = ViewportTheme::colors.componentBG;
    }

    std::vector<std::shared_ptr<SceneComponent>>
    WidgetsTestComponent::clone(const SceneState &sceneState) const {
        (void)sceneState;
        auto clonedComponent = std::make_shared<WidgetsTestComponent>(*this);
        prepareClone(*clonedComponent);
        return {clonedComponent};
    }

    glm::vec2 WidgetsTestComponent::calculateScale(const SceneState &state) {
        (void)state;
        return {230.f, 174.f};
    }

    void WidgetsTestComponent::draw(SceneDrawContext &context) {
        if (m_isFirstDraw) {
            onFirstDraw(context);
        }

        const auto backgroundId = PickingId{m_runtimeId, 0};
        const auto pos = getAbsolutePosition(*context.sceneState);
        const auto z = pos.z;

        SceneDraw::QuadStyle backgroundStyle{
            .borderColor = m_isSelected ? ViewportTheme::colors.selectedComp
                                        : ViewportTheme::colors.componentBorder,
            .borderRadius = Styles::componentStyles.borderRadius,
            .borderSize = Styles::componentStyles.borderSize,
        };
        backgroundStyle.shadow.enabled = true;
        backgroundStyle.shadow.offset = {0.f, 4.f};
        backgroundStyle.shadow.blur = 12.f;
        backgroundStyle.shadow.spread = 1.f;
        backgroundStyle.shadow.color =
            Core::Renderer::Color{0.f, 0.f, 0.f, 0.30f};

        SceneDraw::drawQuad(context,
                            pos,
                            m_transform.scale,
                            m_style.color,
                            backgroundId,
                            backgroundStyle);

        const float left = pos.x - (m_transform.scale.x / 2.f) + 10.f;
        const float top = pos.y - (m_transform.scale.y / 2.f) + 10.f;
        constexpr float labelSize = 9.f;

        SceneDraw::drawText(context,
                            m_name,
                            {left, top + 6.f, z + 0.001f},
                            labelSize,
                            ViewportTheme::colors.text,
                            backgroundId);

        const glm::vec3 togglePos{left + 22.f, top + 32.f, z + 0.001f};
        SceneWidgets::toggleButton(PickingId{m_runtimeId, 1},
                                   &m_toggleValue,
                                   togglePos,
                                   {34.f, 16.f},
                                   context);
        SceneDraw::drawText(context,
                            m_toggleValue ? "Toggle: on" : "Toggle: off",
                            {left + 48.f, top + 35.f, z + 0.001f},
                            labelSize,
                            ViewportTheme::colors.text,
                            backgroundId);

        const std::string buttonLabel =
            std::string("Clicks: ") + std::to_string(m_buttonClicks);
        if (SceneWidgets::button(PickingId{m_runtimeId, 2},
                                 buttonLabel,
                                 {left + 52.f, top + 58.f, z + 0.001f},
                                 context)) {
            ++m_buttonClicks;
        }

        SceneWidgets::TextBoxOptions textBoxOptions{
            .placeholder = "type here",
            .maxLength = 48,
            .fontSize = 8.f,
            .padding = {5.f, 2.f},
        };
        SceneWidgets::textBox(PickingId{m_runtimeId, 3},
                              &m_name,
                              {left + 160.f, top + 58.f, z + 0.001f},
                              {100.f, 18.f},
                              context,
                              textBoxOptions);

        SceneDraw::drawText(context,
                            "Level",
                            {left, top + 87.f, z + 0.001f},
                            labelSize,
                            ViewportTheme::colors.text,
                            backgroundId);
        SceneWidgets::SliderOptions sliderOptions{
            .step = 0.01f,
            .precision = 2,
            .fontSize = 8.f,
            .trackHeight = 4.f,
            .knobRadius = 5.f,
        };
        SceneWidgets::sliderFloat(PickingId{m_runtimeId, 4},
                                  &m_sliderValue,
                                  0.f,
                                  1.f,
                                  {left + 136.f, top + 86.f, z + 0.001f},
                                  {168.f, 20.f},
                                  context,
                                  sliderOptions);

        SceneDraw::drawText(context,
                            "Steps",
                            {left, top + 113.f, z + 0.001f},
                            labelSize,
                            ViewportTheme::colors.text,
                            backgroundId);
        SceneWidgets::SliderOptions intSliderOptions = sliderOptions;
        intSliderOptions.step = 1.f;
        intSliderOptions.precision = 0;
        SceneWidgets::sliderInt(PickingId{m_runtimeId, 5},
                                &m_intSliderValue,
                                0,
                                12,
                                {left + 136.f, top + 112.f, z + 0.001f},
                                {168.f, 20.f},
                                context,
                                intSliderOptions);

        static constexpr std::array<std::string_view, 5> modeItems{
            "Inspect", "Edit", "Route", "Measure", "Debug"};
        SceneDraw::drawText(context,
                            "Mode",
                            {left, top + 140.f, z + 0.001f},
                            labelSize,
                            ViewportTheme::colors.text,
                            backgroundId);
        SceneWidgets::DropdownOptions dropdownOptions{
            .placeholder = "Mode",
            .fontSize = 8.f,
            .optionHeight = 18.f,
            .maxVisibleOptions = 5,
        };
        SceneWidgets::dropdown(PickingId{m_runtimeId, 6},
                               &m_dropdownIndex,
                               modeItems,
                               {left + 136.f, top + 138.f, z + 0.001f},
                               {168.f, 20.f},
                               context,
                               dropdownOptions);
    }
} // namespace Bess::Canvas
