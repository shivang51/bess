#include "bess_core/scene/scene_ui/controls/scalar_input_comp.h"
#include "bess_core/renderer/renderer_2d.h"
#include "bess_core/scene/scene_event.h"
#include "bess_core/scene/scene_state/scene_state.h"
#include <algorithm>
#include <array>
#include <cerrno>
#include <charconv>
#include <cmath>
#include <cstdlib>
#include <limits>

namespace Bess::Canvas::UI {
    namespace {
        [[nodiscard]] bool isFinite(double value) {
            return std::isfinite(value);
        }

        [[nodiscard]] bool nearlyEqual(double lhs, double rhs, double range) {
            const double epsilon =
                std::max(std::numeric_limits<double>::epsilon() * 16.0,
                         std::max(1.0, range) * 0.000000001);
            return std::abs(lhs - rhs) <= epsilon;
        }

        [[nodiscard]] int clampedPrecision(int precision) {
            return std::clamp(precision, 0, 9);
        }

        void trimTrailingZeros(std::string &text) {
            const auto dot = text.find('.');
            if (dot == std::string::npos) {
                return;
            }

            while (!text.empty() && text.back() == '0') {
                text.pop_back();
            }
            if (!text.empty() && text.back() == '.') {
                text.pop_back();
            }
            if (text == "-0") {
                text = "0";
            }
        }

        [[nodiscard]] std::string formatValue(double value, int precision) {
            std::array<char, 96> buffer{};
            const int digits = clampedPrecision(precision);
            auto result = std::to_chars(buffer.data(),
                                        buffer.data() + buffer.size(),
                                        value,
                                        std::chars_format::fixed,
                                        digits);
            if (result.ec != std::errc{}) {
                result = std::to_chars(buffer.data(),
                                       buffer.data() + buffer.size(),
                                       value,
                                       std::chars_format::general,
                                       std::max(1, digits + 1));
            }
            if (result.ec != std::errc{}) {
                return "0";
            }

            std::string text{buffer.data(), result.ptr};
            trimTrailingZeros(text);
            return text.empty() ? "0" : text;
        }

        [[nodiscard]] bool isValidNumericEdit(std::string_view text) {
            bool sawDot = false;
            bool sawDigit = false;
            bool sawExponent = false;
            for (size_t index = 0; index < text.size(); ++index) {
                const char ch = text[index];
                if (ch >= '0' && ch <= '9') {
                    sawDigit = true;
                    continue;
                }
                if ((ch == '-' || ch == '+') &&
                    (index == 0 || (sawExponent && (text[index - 1] == 'e' ||
                                                    text[index - 1] == 'E')))) {
                    continue;
                }
                if (ch == '.' && !sawDot && !sawExponent) {
                    sawDot = true;
                    continue;
                }
                if ((ch == 'e' || ch == 'E') && sawDigit && !sawExponent) {
                    sawExponent = true;
                    continue;
                }
                return false;
            }
            return true;
        }

        [[nodiscard]] bool parseNumber(std::string_view text, double &value) {
            if (text.empty()) {
                return false;
            }

            std::string owned{text};
            char *end = nullptr;
            errno = 0;
            const double parsed = std::strtod(owned.c_str(), &end);
            if (end != owned.c_str() + owned.size() || errno == ERANGE ||
                !isFinite(parsed)) {
                return false;
            }

            value = parsed;
            return true;
        }
    } // namespace

    std::shared_ptr<ScalarInputComp>
    ScalarInputComp::create(double value,
                            const UIScalarInputCallback &changedCallback) {
        auto input = std::make_shared<ScalarInputComp>();
        input->setValue(value);
        input->setChangedCallback(changedCallback);
        return input;
    }

    double ScalarInputComp::getValue() const {
        return m_value;
    }

    void ScalarInputComp::setValue(double value) {
        const double previous = m_value;
        m_value = normalizedValue(sanitizeValue(value, m_value));
        syncTextFromValue(m_textInput.isFocused());
        if (!nearlyEqual(previous, m_value, m_maxValue - m_minValue)) {
            invalidateTextLayout();
        }
    }

    double ScalarInputComp::getMinValue() const {
        return m_minValue;
    }

    void ScalarInputComp::setMinValue(double value) {
        setValueRange(value, m_maxValue);
    }

    double ScalarInputComp::getMaxValue() const {
        return m_maxValue;
    }

    void ScalarInputComp::setMaxValue(double value) {
        setValueRange(m_minValue, value);
    }

    void ScalarInputComp::setValueRange(double minValue, double maxValue) {
        m_minValue = sanitizeValue(minValue, 0.0);
        m_maxValue = sanitizeValue(maxValue, m_minValue + 1.0);
        normalizeRange();
        m_clampToRange = true;
        setValue(m_value);
        invalidateTextLayout();
    }

    double ScalarInputComp::getStep() const {
        return m_step;
    }

    void ScalarInputComp::setStep(double step) {
        m_step = sanitizeStep(step);
    }

    int ScalarInputComp::getPrecision() const {
        return m_precision;
    }

    void ScalarInputComp::setPrecision(int precision) {
        const int next = clampedPrecision(precision);
        if (m_precision == next) {
            return;
        }

        m_precision = next;
        if (!m_textInput.isFocused()) {
            syncTextFromValue(false);
        }
        invalidateTextLayout();
    }

    bool ScalarInputComp::getClampToRange() const {
        return m_clampToRange;
    }

    void ScalarInputComp::setClampToRange(bool clampToRange) {
        if (m_clampToRange == clampToRange) {
            return;
        }

        m_clampToRange = clampToRange;
        setValue(m_value);
    }

    void ScalarInputComp::update(TimeMs ts, SceneState &state) {
        (void)ts;
        if (m_clearFocus) {
            m_clearFocus = false;
            if (m_focused) {
                state.clearUIFocus();
            }
        }
    }

    void ScalarInputComp::onDraw(SceneDrawContext &state) {
        const auto id = PickingId{
            .runtimeId = resolveRuntimeId(),
            .info = 0,
        };

        m_textInput.syncExternalValue(m_text, m_maxLength);
        const TextBoxContextDrawOptions options{
            .placeholder = m_placeholder,
            .fontSize = m_style.textStyle.fontSize,
            .padding = stylePadding(),
            .backgroundColor = m_style.backgroundColor,
            .hoverBackgroundColor = m_style.hoverColor,
            .focusedBackgroundColor = m_style.backgroundColor,
            .borderColor = m_style.borderColor,
            .focusedBorderColor = m_style.activeColor,
            .textColor = m_style.textStyle.textColor,
            .placeholderColor = m_style.textStyle.textColor.withAlpha(0.55f),
            .selectionColor = m_style.activeColor.withAlpha(0.45f),
            .cursorColor = m_style.activeColor,
            .cursorWidth = 1.f,
            .cursorHeight = std::max(10.f, m_node->getDrawSize().y - 6.f),
            .hovered = m_hovered,
        };

        drawTextBoxContext(id,
                           m_textInput,
                           m_node->getDrawPos(),
                           m_node->getDrawSize(),
                           state,
                           options);
    }

    void ScalarInputComp::prepareUI(SceneUIPrepareCtx &state) {
        prepStyle(state.theme);
        initNode(state.sceneState->getUINodeRegistry());

        const auto size = resolveInputSize(state);
        m_node->setWidth(size.x);
        m_node->setHeight(size.y);
        m_node->setPadding(0.f);
        m_node->setMargin(m_style.metrics.margin);

        if (state.parentNode != nullptr) {
            state.parentNode->addChild(m_node);
        }

        m_isUIDirty = false;
    }

    bool ScalarInputComp::isFocusable() const {
        return true;
    }

    bool ScalarInputComp::wantsKeyboardInput() const {
        return m_focused;
    }

    void ScalarInputComp::onFocusGained(const Events::FocusEvent &e) {
        UISceneComponent::onFocusGained(e);
        m_textInput.focus(m_text, m_maxLength, true);
    }

    void ScalarInputComp::onFocusLost(const Events::FocusEvent &e) {
        UISceneComponent::onFocusLost(e);
        commitText();
        m_textInput.blur();
    }

    bool ScalarInputComp::onMouseButton(const Events::MouseButtonEvent &e) {
        if (e.button != Events::MouseButton::left) {
            return false;
        }

        if (e.action == Events::MouseClickAction::press ||
            e.action == Events::MouseClickAction::doubleClick) {
            m_textInput.queuePointerPress(e.mousePos);
            return true;
        }

        if (e.action == Events::MouseClickAction::release) {
            m_textInput.queuePointerRelease(e.mousePos);
            return true;
        }

        return false;
    }

    bool ScalarInputComp::onPointerMove(const Events::MouseMoveEvent &e) {
        m_textInput.queuePointerMove(e.mousePos);
        return true;
    }

    bool ScalarInputComp::onKeyEvent(const SceneEvent &evt) {
        if (evt.type == SceneEvent::Type::key &&
            (evt.data.keyPress.action == KeyAction::press ||
             evt.data.keyPress.action == KeyAction::hold)) {
            const double baseStep = m_step > 0.0 ? m_step : 1.0;
            const double step = evt.isShiftPressed ? baseStep * 10.0 : baseStep;
            switch (evt.data.keyPress.keycode) {
            case KeyCode::arrowUp:
                commitText();
                setValueFromUser(m_value + step, true);
                return true;
            case KeyCode::arrowDown:
                commitText();
                setValueFromUser(m_value - step, true);
                return true;
            default:
                break;
            }
        }

        const auto result = m_textInput.handleEvent(evt);
        applyTextInputResult(result);
        return result.handled;
    }

    bool ScalarInputComp::hasPointerCapture() const {
        return m_textInput.hasPointerCapture();
    }

    Core::Viewport::SceneCursor ScalarInputComp::getCursor() const {
        return Core::Viewport::SceneCursor::text;
    }

    double ScalarInputComp::sanitizeValue(double value, double fallback) const {
        return isFinite(value) ? value : fallback;
    }

    double ScalarInputComp::sanitizeStep(double step) const {
        return isFinite(step) && step > 0.0 ? step : 1.0;
    }

    double ScalarInputComp::normalizedValue(double value) const {
        value = sanitizeValue(value, m_value);
        if (!m_clampToRange) {
            return value;
        }
        return std::clamp(value, m_minValue, m_maxValue);
    }

    glm::vec2
    ScalarInputComp::resolveInputSize(SceneUIPrepareCtx &state) const {
        auto size = m_inputSize;
        const auto padding = stylePadding();
        const auto fontProps = Core::Renderer::FontProps{
            .fontSize = m_style.textStyle.fontSize,
        };
        const auto referenceTextSize =
            state.renderer->measureText("M", fontProps);

        if (size.y == 0.f) {
            size.y = referenceTextSize.y + (padding.y * 2.f);
        }

        if (size.x == 0.f) {
            const auto &displayText = m_text.empty() ? m_placeholder : m_text;
            const auto measuredText =
                state.renderer->measureText(displayText, fontProps);
            size.x = std::max(48.f, measuredText.x + (padding.x * 2.f));
        }

        return size;
    }

    glm::vec2 ScalarInputComp::stylePadding() const {
        const auto &padding = m_style.metrics.padding;
        return {
            std::max(padding.left, padding.right),
            std::max(padding.top, padding.bottom),
        };
    }

    void ScalarInputComp::normalizeRange() {
        if (m_maxValue < m_minValue) {
            std::swap(m_minValue, m_maxValue);
        }
        if (m_maxValue == m_minValue) {
            m_maxValue = m_minValue + 1.0;
        }
    }

    void ScalarInputComp::syncTextFromValue(bool updateFocusedContext) {
        m_text = formatValue(m_value, m_precision);
        if (updateFocusedContext) {
            m_textInput.replaceText(m_text, m_maxLength, false);
        }
    }

    void ScalarInputComp::setValueFromUser(double value,
                                           bool commitDisplayText) {
        const double previous = m_value;
        m_value = normalizedValue(value);

        if (commitDisplayText) {
            syncTextFromValue(m_textInput.isFocused());
        }

        if (!nearlyEqual(previous, m_value, m_maxValue - m_minValue) &&
            m_changedCallback) {
            m_changedCallback(m_value);
        }
        invalidateTextLayout();
    }

    void ScalarInputComp::commitText() {
        double parsed = 0.0;
        if (parseNumber(m_textInput.text(), parsed)) {
            setValueFromUser(parsed, true);
            return;
        }

        restoreValidText();
    }

    void ScalarInputComp::restoreValidText() {
        syncTextFromValue(m_textInput.isFocused());
        invalidateTextLayout();
    }

    void
    ScalarInputComp::applyTextInputResult(const TextBoxContextResult &result) {
        if (result.changed) {
            const auto &candidate = m_textInput.text();
            if (!isValidNumericEdit(candidate)) {
                m_textInput.replaceText(m_text, m_maxLength);
                return;
            }

            m_text = candidate;
            double parsed = 0.0;
            if (parseNumber(candidate, parsed)) {
                setValueFromUser(parsed, false);
            } else {
                invalidateTextLayout();
            }
        }

        if (result.submitted) {
            commitText();
            m_clearFocus = true;
        }

        if (result.canceled) {
            restoreValidText();
            m_clearFocus = true;
        }
    }

    void ScalarInputComp::invalidateTextLayout() {
        if (m_inputSize.x <= 0.f) {
            makeUIDirty();
        }
    }
} // namespace Bess::Canvas::UI
