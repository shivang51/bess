#include "non_sim_scene_component.h"
#include "bess_core/renderer/renderer_2d.h"
#include "bess_core/scene/scene_draw_context.h"
#include "bess_core/scene/scene_draw_helpers.h"
#include "bess_core/scene/scene_state/components/styles/comp_style.h"
#include "bess_core/scene/widgets/scene_widgets.h"
#include "bess_core/settings/viewport_theme.h"
#include "pages/main_page/comp_edit.h"
#include "ui/icons/FontAwesomeIcons_Remapped.h"
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

    WidgetsTestComponent::WidgetsTestComponent() {
        m_name = "Widgets Test";
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
        auto before = toEditJson();

        const auto backgroundId = PickingId{m_runtimeId, 0};
        const auto pos =
            getAbsolutePosition(*context.sceneState, context.isSchematicMode);
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
        (void)Edit::trackComp(*this, std::move(before), "widget-value");
    }
} // namespace Bess::Canvas
