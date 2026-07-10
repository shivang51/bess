#include "bess_core/scene/scene_ui/controls/slider_comp.h"
#include "bess_core/renderer/renderer_2d.h"
#include "bess_core/scene/scene_event.h"
#include "bess_core/scene/scene_state/scene_state.h"
#include <algorithm>
#include <array>
#include <charconv>
#include <cmath>
#include <limits>

namespace Bess::Canvas::UI {
    namespace {
        constexpr uint32_t kSliderTextInfo = 0u;
        constexpr uint32_t kSliderTrackInfo = 1u;
        constexpr float kMinTrackWidth = 32.f;
        constexpr float kDefaultKeyboardSteps = 100.f;

        [[nodiscard]] bool isFinite(float value) {
            return std::isfinite(value);
        }

        [[nodiscard]] bool nearlyEqual(float lhs, float rhs, float range) {
            const float epsilon =
                std::max(std::numeric_limits<float>::epsilon() * 8.f,
                         std::max(1.f, range) * 0.000001f);
            return std::abs(lhs - rhs) <= epsilon;
        }

        [[nodiscard]] std::string formatValue(float value, int precision) {
            std::array<char, 64> buffer{};
            std::to_chars_result result{};
            if (precision <= 0) {
                result =
                    std::to_chars(buffer.data(),
                                  buffer.data() + buffer.size(),
                                  static_cast<long long>(std::llround(value)));
            } else {
                result = std::to_chars(buffer.data(),
                                       buffer.data() + buffer.size(),
                                       value,
                                       std::chars_format::fixed,
                                       precision);
            }

            if (result.ec != std::errc{}) {
                return {};
            }
            return {buffer.data(), result.ptr};
        }

        [[nodiscard]] glm::vec2 stableValueLabelSize(
            const std::shared_ptr<Core::Renderer::IRenderer2D> &renderer,
            float minValue,
            float maxValue,
            float value,
            int precision,
            float fontSize) {
            glm::vec2 size{0.f};
            const auto fontProps = Core::Renderer::FontProps{
                .fontSize = fontSize,
            };
            const auto measure = [&](float candidate) {
                const auto measured = renderer->measureText(
                    formatValue(candidate, precision), fontProps);
                size.x = std::max(size.x, measured.x);
                size.y = std::max(size.y, measured.y);
            };

            measure(minValue);
            measure(maxValue);
            measure(value);
            return size;
        }
    } // namespace

    std::shared_ptr<SliderComp>
    SliderComp::create(const std::string &label,
                       float value,
                       float minValue,
                       float maxValue,
                       const UISliderCallback &changedCallback) {
        auto slider = std::make_shared<SliderComp>();
        slider->setName(label);
        slider->setValueRange(minValue, maxValue);
        slider->setValue(value);
        slider->setChangedCallback(changedCallback);
        return slider;
    }

    float SliderComp::getValue() const {
        return m_value;
    }

    void SliderComp::setValue(float value) {
        const float range = m_maxValue - m_minValue;
        const float next = snappedValue(sanitizeValue(value, m_minValue));
        if (nearlyEqual(m_value, next, range)) {
            return;
        }

        m_value = next;
        updateCachedValueLabel();
    }

    float SliderComp::getMinValue() const {
        return m_minValue;
    }

    void SliderComp::setMinValue(float value) {
        setValueRange(value, m_maxValue);
    }

    float SliderComp::getMaxValue() const {
        return m_maxValue;
    }

    void SliderComp::setMaxValue(float value) {
        setValueRange(m_minValue, value);
    }

    void SliderComp::setValueRange(float minValue, float maxValue) {
        const float previousMin = m_minValue;
        const float previousMax = m_maxValue;
        m_minValue = sanitizeValue(minValue, 0.f);
        m_maxValue = sanitizeValue(maxValue, m_minValue + 1.f);
        normalizeRange();
        setValue(m_value);
        updateCachedValueLabel();

        if (previousMin != m_minValue || previousMax != m_maxValue) {
            makeUIDirty();
        }
    }

    float SliderComp::getStep() const {
        return m_step;
    }

    void SliderComp::setStep(float step) {
        const float next = sanitizeStep(step);
        if (m_step == next) {
            return;
        }

        m_step = next;
        setValue(m_value);
    }

    void SliderComp::onDraw(SceneDrawContext &state) {
        if (m_trackNode == nullptr || state.renderer == nullptr) {
            return;
        }

        if (m_showLabel && m_labelNode != nullptr) {
            drawText(state, m_name, m_labelNode);
        }

        if (m_showValue && m_valueNode != nullptr) {
            drawText(state, m_cachedValueLabel, m_valueNode);
        }

        const PickingId trackId{
            .runtimeId = resolveRuntimeId(),
            .info = kSliderTrackInfo,
        };

        const auto trackPos = m_trackNode->getDrawPos();
        const auto trackSize = m_trackNode->getDrawSize();
        const auto trackCenter = glm::vec2(trackPos.x, trackPos.y);
        const float trackWidth =
            std::max(1.f, trackSize.x - (m_thumbRadius * 2.f));
        const float trackLeft = trackCenter.x - (trackWidth * 0.5f);
        const float knobX = trackLeft + (trackWidth * valueToNormalized());

        Core::Renderer::QuadProps railProps;
        railProps.position = trackCenter;
        railProps.size = {trackWidth, std::max(1.f, m_trackHeight)};
        railProps.zIndex = trackPos.z;
        railProps.color = m_trackColor;
        railProps.radius = glm::vec4(std::max(1.f, m_trackHeight) * 0.5f);
        railProps.id = trackId;
        railProps.transformMode = state.transformMode;
        state.renderer->drawQuad(railProps);

        const float fillWidth = std::max(0.f, knobX - trackLeft);
        if (fillWidth > 0.f) {
            Core::Renderer::QuadProps fillProps = railProps;
            fillProps.position = {
                trackLeft + (fillWidth * 0.5f),
                trackCenter.y,
            };
            fillProps.size.x = fillWidth;
            fillProps.zIndex = trackPos.z + 0.0001f;
            fillProps.color = m_fillColor;
            state.renderer->drawQuad(fillProps);
        }

        Core::Renderer::CircleProps thumbProps;
        thumbProps.position = {knobX, trackCenter.y};
        thumbProps.radius = std::max(1.f, m_thumbRadius);
        thumbProps.zIndex = trackPos.z + 0.0002f;
        thumbProps.color = m_thumbColor;
        thumbProps.id = trackId;
        thumbProps.transformMode = state.transformMode;
        state.renderer->drawCircle(thumbProps);
    }

    void SliderComp::prepareUI(SceneUIPrepareCtx &state) {
        prepStyle(state.theme);
        initNode(state.sceneState->getUINodeRegistry());

        if (m_labelNode == nullptr) {
            m_labelNode =
                state.sceneState->getUINodeRegistry()->addNode(UUID());
        }
        if (m_trackNode == nullptr) {
            m_trackNode =
                state.sceneState->getUINodeRegistry()->addNode(UUID());
        }
        if (m_valueNode == nullptr) {
            m_valueNode =
                state.sceneState->getUINodeRegistry()->addNode(UUID());
        }

        m_node->setDirection(LayoutDirection::horizontal);
        m_node->setWidthFitContent();
        m_node->setHeightFitContent();
        m_node->setCrossAxisAlignment(LayoutAlignment::center);
        m_node->setPadding(m_style.metrics.padding);
        m_node->setMargin(m_style.metrics.margin);

        if (m_showLabel) {
            const auto labelSize = state.renderer->measureText(
                m_name,
                {
                    .fontSize = m_style.textStyle.fontSize,
                });

            m_labelNode->setWidth(labelSize.x);
            m_labelNode->setHeight(labelSize.y);
            m_labelNode->setPosMode(PosMode::relative);
            m_labelNode->setPadding(0.f);
            m_labelNode->setMargin(Core::Style::Margin::onlyRight(
                std::max(0.f, m_labelTrackSpacing)));
            m_node->addChild(m_labelNode);
        }

        const auto trackSize = resolveTrackSize();
        m_trackNode->setWidth(trackSize.x);
        m_trackNode->setHeight(trackSize.y);
        m_trackNode->setPosMode(PosMode::relative);
        m_trackNode->setPadding(0.f);
        m_trackNode->setMargin(m_showValue
                                   ? Core::Style::Margin::onlyRight(
                                         std::max(0.f, m_valueTrackSpacing))
                                   : Core::Style::Margin(0.f));
        m_node->addChild(m_trackNode);

        if (m_showValue) {
            const auto valueSize =
                stableValueLabelSize(state.renderer,
                                     m_minValue,
                                     m_maxValue,
                                     m_value,
                                     std::clamp(m_valuePrecision, 0, 6),
                                     m_style.textStyle.fontSize);

            m_valueNode->setWidth(valueSize.x);
            m_valueNode->setHeight(valueSize.y);
            m_valueNode->setPosMode(PosMode::relative);
            m_valueNode->setPadding(0.f);
            m_valueNode->setMargin(0.f);
            m_node->addChild(m_valueNode);
        }

        if (state.parentNode != nullptr) {
            state.parentNode->addChild(m_node);
        }

        m_isUIDirty = false;
    }

    void SliderComp::prepStyle(
        const std::shared_ptr<Core::Style::BessTheme> &theme) {
        UISceneComponent::prepStyle(theme);

        const auto &colors = theme->getColorScheme().getColors();
        m_trackColor = colors.secondaryContainer;
        m_fillColor = colors.primary;
        m_thumbColor = colors.primary;
    }

    bool SliderComp::onMouseButton(const Events::MouseButtonEvent &e) {
        if (e.button != Events::MouseButton::left) {
            return false;
        }

        if (e.action == Events::MouseClickAction::press &&
            e.details == kSliderTrackInfo) {
            m_dragging = true;
            setValueFromUser(pointerToValue(e.mousePos));
            return true;
        }

        if (e.action == Events::MouseClickAction::release && m_dragging) {
            setValueFromUser(pointerToValue(e.mousePos));
            m_dragging = false;
            return true;
        }

        return e.details == kSliderTextInfo;
    }

    bool SliderComp::onPointerMove(const Events::MouseMoveEvent &e) {
        if (!m_dragging) {
            return false;
        }

        setValueFromUser(pointerToValue(e.mousePos));
        return true;
    }

    bool SliderComp::hasPointerCapture() const {
        return m_dragging;
    }

    bool SliderComp::isFocusable() const {
        return true;
    }

    bool SliderComp::wantsKeyboardInput() const {
        return m_focused;
    }

    bool SliderComp::onKeyEvent(const SceneEvent &evt) {
        if (evt.type != SceneEvent::Type::key ||
            (evt.data.keyPress.action != KeyAction::press &&
             evt.data.keyPress.action != KeyAction::hold)) {
            return false;
        }

        const float range = m_maxValue - m_minValue;
        const float baseStep =
            m_step > 0.f ? m_step
                         : std::max(range / kDefaultKeyboardSteps, 0.f);
        const float step = evt.isShiftPressed ? baseStep * 10.f : baseStep;

        switch (evt.data.keyPress.keycode) {
        case KeyCode::arrowLeft:
        case KeyCode::arrowDown:
            applyKeyboardDelta(-step);
            return true;
        case KeyCode::arrowRight:
        case KeyCode::arrowUp:
            applyKeyboardDelta(step);
            return true;
        case KeyCode::pageDown:
            applyKeyboardDelta(-(baseStep * 10.f));
            return true;
        case KeyCode::pageUp:
            applyKeyboardDelta(baseStep * 10.f);
            return true;
        case KeyCode::home:
            setValueFromUser(m_minValue);
            return true;
        case KeyCode::end:
            setValueFromUser(m_maxValue);
            return true;
        default:
            return false;
        }
    }

    Core::Viewport::SceneCursor SliderComp::getCursor() const {
        return Core::Viewport::SceneCursor::pointer;
    }

    float SliderComp::sanitizeValue(float value, float fallback) const {
        return isFinite(value) ? value : fallback;
    }

    float SliderComp::sanitizeStep(float step) const {
        return isFinite(step) && step > 0.f ? step : 0.f;
    }

    float SliderComp::snappedValue(float value) const {
        value = std::clamp(value, m_minValue, m_maxValue);
        if (m_step <= 0.f) {
            return value;
        }

        const float steps = std::round((value - m_minValue) / m_step);
        return std::clamp(
            m_minValue + (steps * m_step), m_minValue, m_maxValue);
    }

    float SliderComp::valueToNormalized() const {
        if (m_maxValue <= m_minValue) {
            return 0.f;
        }
        return std::clamp(
            (m_value - m_minValue) / (m_maxValue - m_minValue), 0.f, 1.f);
    }

    float SliderComp::pointerToValue(const glm::vec2 &pointerPos) const {
        if (m_trackNode == nullptr) {
            return m_value;
        }

        const auto pos = m_trackNode->getDrawPos();
        const auto size = m_trackNode->getDrawSize();
        const float width = std::max(1.f, size.x - (m_thumbRadius * 2.f));
        const float left = pos.x - (width * 0.5f);
        const float normalized =
            std::clamp((pointerPos.x - left) / width, 0.f, 1.f);
        return m_minValue + ((m_maxValue - m_minValue) * normalized);
    }

    glm::vec2 SliderComp::resolveTrackSize() const {
        const float thumbDiameter = std::max(1.f, m_thumbRadius) * 2.f;
        const float trackHeight = std::max(1.f, m_trackHeight);
        glm::vec2 size = m_sliderSize;
        size.x = std::max(kMinTrackWidth, size.x);
        if (size.y <= 0.f) {
            size.y = std::max(thumbDiameter, trackHeight + 8.f);
        } else {
            size.y = std::max(size.y, std::max(thumbDiameter, trackHeight));
        }
        return size;
    }

    void SliderComp::setValueFromUser(float value) {
        const float previous = m_value;
        setValue(value);
        if (!nearlyEqual(previous, m_value, m_maxValue - m_minValue) &&
            m_changedCallback) {
            m_changedCallback(m_value);
        }
    }

    void SliderComp::applyKeyboardDelta(float delta) {
        setValueFromUser(m_value + delta);
    }

    void SliderComp::normalizeRange() {
        if (m_maxValue < m_minValue) {
            std::swap(m_minValue, m_maxValue);
        }
        if (m_maxValue == m_minValue) {
            m_maxValue = m_minValue + 1.f;
        }
        m_step = sanitizeStep(m_step);
    }

    void SliderComp::onValueFormatChanged() {
        m_valuePrecision = std::clamp(m_valuePrecision, 0, 6);
        updateCachedValueLabel();
        makeUIDirty();
    }

    void SliderComp::updateCachedValueLabel() {
        m_cachedValueLabel =
            formatValue(m_value, std::clamp(m_valuePrecision, 0, 6));
        if (m_cachedValueLabel.empty()) {
            m_cachedValueLabel = "0";
        }
    }
} // namespace Bess::Canvas::UI
