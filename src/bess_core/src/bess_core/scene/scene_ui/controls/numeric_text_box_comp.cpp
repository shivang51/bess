#include "bess_core/scene/scene_ui/controls/numeric_text_box_comp.h"
#include "bess_core/renderer/renderer_2d.h"
#include "bess_core/scene/scene_event.h"
#include "bess_core/scene/scene_state/scene_state.h"
#include <algorithm>
#include <array>
#include <charconv>
#include <cmath>
#include <limits>
#include <string_view>
#include <system_error>
#include <utility>

namespace Bess::Canvas::UI {
    namespace {
        constexpr int kMaximumFloatPrecision = 32;
        constexpr size_t kMaximumExpressionDepth = 64;

        [[nodiscard]] bool sameSize(const glm::vec2 &lhs,
                                    const glm::vec2 &rhs) noexcept {
            return lhs.x == rhs.x && lhs.y == rhs.y;
        }

        [[nodiscard]] bool isFinite(long double value) noexcept {
            return std::isfinite(value);
        }

        [[nodiscard]] bool sameNumericValue(long double lhs,
                                            long double rhs,
                                            bool floatingPoint) noexcept {
            if (lhs != rhs) {
                return false;
            }
            return !floatingPoint || std::signbit(lhs) == std::signbit(rhs);
        }

        [[nodiscard]] bool isIntegerEdit(std::string_view text) noexcept {
            size_t index = 0;
            if (!text.empty() && (text.front() == '+' || text.front() == '-')) {
                index = 1;
            }

            for (; index < text.size(); ++index) {
                if (text[index] < '0' || text[index] > '9') {
                    return false;
                }
            }
            return true;
        }

        [[nodiscard]] bool isFloatEdit(std::string_view text,
                                       bool allowExponent) noexcept {
            size_t index = 0;
            if (!text.empty() && (text.front() == '+' || text.front() == '-')) {
                index = 1;
            }

            bool sawMantissaDigit = false;
            bool sawDecimalPoint = false;
            for (; index < text.size(); ++index) {
                const char ch = text[index];
                if (ch >= '0' && ch <= '9') {
                    sawMantissaDigit = true;
                    continue;
                }
                if (ch == '.' && !sawDecimalPoint) {
                    sawDecimalPoint = true;
                    continue;
                }
                break;
            }

            if (index == text.size()) {
                return true;
            }
            if (!allowExponent || !sawMantissaDigit ||
                (text[index] != 'e' && text[index] != 'E')) {
                return false;
            }

            ++index;
            if (index < text.size() &&
                (text[index] == '+' || text[index] == '-')) {
                ++index;
            }
            for (; index < text.size(); ++index) {
                if (text[index] < '0' || text[index] > '9') {
                    return false;
                }
            }
            return true;
        }

        [[nodiscard]] bool isExpressionEdit(std::string_view text,
                                            bool floatingPoint,
                                            bool allowExponent) noexcept {
            for (const char ch : text) {
                if ((ch >= '0' && ch <= '9') || ch == '+' || ch == '-' ||
                    ch == '*' || ch == '/' || ch == '%' || ch == '(' ||
                    ch == ')' || ch == ' ' || ch == '\t') {
                    continue;
                }
                if (floatingPoint && ch == '.') {
                    continue;
                }
                if (floatingPoint && allowExponent &&
                    (ch == 'e' || ch == 'E')) {
                    continue;
                }
                return false;
            }
            return true;
        }

        [[nodiscard]] bool parseInteger(std::string_view text,
                                        int &value) noexcept {
            if (text.empty() || text == "+" || text == "-") {
                return false;
            }

            bool positiveSign = text.front() == '+';
            if (positiveSign) {
                text.remove_prefix(1);
            }
            if (text.empty()) {
                return false;
            }

            int parsed = 0;
            const auto result =
                std::from_chars(text.data(), text.data() + text.size(), parsed);
            if (result.ec != std::errc{} ||
                result.ptr != text.data() + text.size()) {
                return false;
            }

            value = parsed;
            return true;
        }

        [[nodiscard]] bool parseFloat(std::string_view text,
                                      float &value) noexcept {
            if (text.empty() || text == "+" || text == "-" || text == "." ||
                text == "+." || text == "-.") {
                return false;
            }

            if (text.front() == '+') {
                text.remove_prefix(1);
            }
            if (text.empty()) {
                return false;
            }

            float parsed = 0.f;
            const auto result = std::from_chars(text.data(),
                                                text.data() + text.size(),
                                                parsed,
                                                std::chars_format::general);
            if (result.ec != std::errc{} ||
                result.ptr != text.data() + text.size() ||
                !std::isfinite(parsed)) {
                return false;
            }

            value = parsed;
            return true;
        }

        [[nodiscard]] bool parseDouble(std::string_view text,
                                       double &value) noexcept {
            if (text.empty() || text == "+" || text == "-" || text == "." ||
                text == "+." || text == "-.") {
                return false;
            }

            if (text.front() == '+') {
                text.remove_prefix(1);
            }
            if (text.empty()) {
                return false;
            }

            double parsed = 0.0;
            const auto result = std::from_chars(text.data(),
                                                text.data() + text.size(),
                                                parsed,
                                                std::chars_format::general);
            if (result.ec != std::errc{} ||
                result.ptr != text.data() + text.size() ||
                !std::isfinite(parsed)) {
                return false;
            }

            value = parsed;
            return true;
        }

        class ArithmeticParser {
          public:
            explicit ArithmeticParser(std::string_view text) : m_text(text) {
            }

            [[nodiscard]] bool parse(double &value) noexcept {
                if (!parseExpression(value)) {
                    return false;
                }
                skipSpaces();
                return m_pos == m_text.size() && std::isfinite(value);
            }

          private:
            [[nodiscard]] bool parseExpression(double &value) noexcept {
                if (!parseTerm(value)) {
                    return false;
                }

                while (true) {
                    skipSpaces();
                    if (m_pos >= m_text.size() ||
                        (m_text[m_pos] != '+' && m_text[m_pos] != '-')) {
                        return true;
                    }

                    const char operation = m_text[m_pos++];
                    double rhs = 0.0;
                    if (!parseTerm(rhs)) {
                        return false;
                    }
                    value = operation == '+' ? value + rhs : value - rhs;
                    if (!std::isfinite(value)) {
                        return false;
                    }
                }
            }

            [[nodiscard]] bool parseTerm(double &value) noexcept {
                if (!parseUnary(value)) {
                    return false;
                }

                while (true) {
                    skipSpaces();
                    if (m_pos >= m_text.size() ||
                        (m_text[m_pos] != '*' && m_text[m_pos] != '/' &&
                         m_text[m_pos] != '%')) {
                        return true;
                    }

                    const char operation = m_text[m_pos++];
                    double rhs = 0.0;
                    if (!parseUnary(rhs)) {
                        return false;
                    }

                    if ((operation == '/' || operation == '%') && rhs == 0.0) {
                        return false;
                    }
                    if (operation == '*') {
                        value *= rhs;
                    } else if (operation == '/') {
                        value /= rhs;
                    } else {
                        value = std::fmod(value, rhs);
                    }
                    if (!std::isfinite(value)) {
                        return false;
                    }
                }
            }

            [[nodiscard]] bool parseUnary(double &value) noexcept {
                skipSpaces();
                bool negate = false;
                size_t signs = 0;
                while (m_pos < m_text.size() &&
                       (m_text[m_pos] == '+' || m_text[m_pos] == '-')) {
                    negate = m_text[m_pos] == '-' ? !negate : negate;
                    ++m_pos;
                    skipSpaces();
                    if (++signs > kMaximumExpressionDepth) {
                        return false;
                    }
                }

                if (!parsePrimary(value)) {
                    return false;
                }
                if (negate) {
                    value = -value;
                }
                return std::isfinite(value);
            }

            [[nodiscard]] bool parsePrimary(double &value) noexcept {
                skipSpaces();
                if (m_pos >= m_text.size()) {
                    return false;
                }

                if (m_text[m_pos] != '(') {
                    return parseNumber(value);
                }
                if (++m_depth > kMaximumExpressionDepth) {
                    return false;
                }

                ++m_pos;
                const bool parsed = parseExpression(value);
                skipSpaces();
                const bool closed =
                    m_pos < m_text.size() && m_text[m_pos] == ')';
                if (closed) {
                    ++m_pos;
                }
                --m_depth;
                return parsed && closed;
            }

            [[nodiscard]] bool parseNumber(double &value) noexcept {
                const char *begin = m_text.data() + m_pos;
                const char *end = m_text.data() + m_text.size();
                const auto result = std::from_chars(
                    begin, end, value, std::chars_format::general);
                if (result.ec != std::errc{} || result.ptr == begin ||
                    !std::isfinite(value)) {
                    return false;
                }
                m_pos = static_cast<size_t>(result.ptr - m_text.data());
                return true;
            }

            void skipSpaces() noexcept {
                while (m_pos < m_text.size() &&
                       (m_text[m_pos] == ' ' || m_text[m_pos] == '\t')) {
                    ++m_pos;
                }
            }

            std::string_view m_text;
            size_t m_pos = 0;
            size_t m_depth = 0;
        };

        [[nodiscard]] bool
        evaluateArithmeticExpression(std::string_view text,
                                     double &value) noexcept {
            return ArithmeticParser(text).parse(value);
        }

        [[nodiscard]] std::string formatInteger(int value) {
            std::array<char, 16> buffer{};
            const auto result = std::to_chars(
                buffer.data(), buffer.data() + buffer.size(), value);
            if (result.ec != std::errc{}) {
                return "0";
            }
            return {buffer.data(), result.ptr};
        }

        void trimFractionZeros(std::string &text) {
            const size_t exponent = text.find_first_of("eE");
            const size_t mantissaEnd =
                exponent == std::string::npos ? text.size() : exponent;
            const size_t decimalPoint = text.find('.');
            if (decimalPoint == std::string::npos ||
                decimalPoint >= mantissaEnd) {
                return;
            }

            size_t keepEnd = mantissaEnd;
            while (keepEnd > decimalPoint + 1 && text[keepEnd - 1] == '0') {
                --keepEnd;
            }
            if (keepEnd == decimalPoint + 1) {
                --keepEnd;
            }
            text.erase(keepEnd, mantissaEnd - keepEnd);
        }

        template <typename Value>
        [[nodiscard]] std::string formatFloating(Value value,
                                                 UIFloatTextBoxFormat format,
                                                 int precision,
                                                 bool trimTrailingZeros) {
            std::array<char, 768> buffer{};
            std::to_chars_result result{};
            switch (format) {
            case UIFloatTextBoxFormat::shortest:
                result = std::to_chars(
                    buffer.data(), buffer.data() + buffer.size(), value);
                break;
            case UIFloatTextBoxFormat::general:
                result = std::to_chars(buffer.data(),
                                       buffer.data() + buffer.size(),
                                       value,
                                       std::chars_format::general,
                                       std::max(1, precision));
                break;
            case UIFloatTextBoxFormat::fixed:
                result = std::to_chars(buffer.data(),
                                       buffer.data() + buffer.size(),
                                       value,
                                       std::chars_format::fixed,
                                       precision);
                break;
            case UIFloatTextBoxFormat::scientific:
                result = std::to_chars(buffer.data(),
                                       buffer.data() + buffer.size(),
                                       value,
                                       std::chars_format::scientific,
                                       precision);
                break;
            }

            if (result.ec != std::errc{}) {
                return "0";
            }
            std::string text{buffer.data(), result.ptr};
            if (trimTrailingZeros && format != UIFloatTextBoxFormat::shortest) {
                trimFractionZeros(text);
            }
            return text.empty() ? "0" : text;
        }

        template <typename Value>
        [[nodiscard]] std::string formatWithoutExponent(Value value) {
            std::array<char, 768> buffer{};
            const auto result = std::to_chars(buffer.data(),
                                              buffer.data() + buffer.size(),
                                              value,
                                              std::chars_format::fixed);
            if (result.ec != std::errc{}) {
                return "0";
            }
            return {buffer.data(), result.ptr};
        }
    } // namespace

    NumericTextBoxComp::NumericTextBoxComp(ValueKind kind) : m_kind(kind) {
        if (m_kind == ValueKind::integer) {
            m_minValue = std::numeric_limits<int>::lowest();
            m_maxValue = std::numeric_limits<int>::max();
            m_maxLength = 32;
        } else if (m_kind == ValueKind::floatingPoint) {
            m_minValue = std::numeric_limits<float>::lowest();
            m_maxValue = std::numeric_limits<float>::max();
            m_maxLength = 128;
        } else {
            m_minValue = 0.0;
            m_maxValue = 1.0;
            m_maxLength = 128;
            m_precision = 2;
            m_trimTrailingZeros = true;
        }
    }

    const std::string &NumericTextBoxComp::getPlaceholder() const noexcept {
        return m_placeholder;
    }

    void NumericTextBoxComp::setPlaceholder(std::string placeholder) {
        if (m_placeholder == placeholder) {
            return;
        }
        m_placeholder = std::move(placeholder);
        invalidateTextLayout();
    }

    const glm::vec2 &NumericTextBoxComp::getTextBoxSize() const noexcept {
        return m_textBoxSize;
    }

    glm::vec2 &NumericTextBoxComp::getTextBoxSize() noexcept {
        return m_textBoxSize;
    }

    void NumericTextBoxComp::setTextBoxSize(const glm::vec2 &size) {
        const glm::vec2 next{std::max(0.f, size.x), std::max(0.f, size.y)};
        if (sameSize(m_textBoxSize, next)) {
            return;
        }
        m_textBoxSize = next;
        makeUIDirty();
    }

    size_t NumericTextBoxComp::getMaxLength() const noexcept {
        return m_maxLength;
    }

    void NumericTextBoxComp::setMaxLength(size_t maxLength) {
        if (m_maxLength == maxLength) {
            return;
        }
        m_maxLength = maxLength;
        if (m_textInput.isFocused()) {
            m_textInput.replaceText(m_text, effectiveMaxLength());
        }
    }

    bool NumericTextBoxComp::getClampToRange() const noexcept {
        return m_clampToRange;
    }

    void NumericTextBoxComp::setClampToRange(bool clampToRange) {
        if (m_clampToRange == clampToRange) {
            return;
        }

        m_clampToRange = clampToRange;
        m_value = quantizedValue(normalizedValue(m_value));
        syncTextFromValue(m_textInput.isFocused());
        invalidateTextLayout();
    }

    bool NumericTextBoxComp::getSelectAllOnFocus() const noexcept {
        return m_selectAllOnFocus;
    }

    void
    NumericTextBoxComp::setSelectAllOnFocus(bool selectAllOnFocus) noexcept {
        m_selectAllOnFocus = selectAllOnFocus;
    }

    bool NumericTextBoxComp::getStepOnMouseWheel() const noexcept {
        return m_stepOnMouseWheel;
    }

    void
    NumericTextBoxComp::setStepOnMouseWheel(bool stepOnMouseWheel) noexcept {
        m_stepOnMouseWheel = stepOnMouseWheel;
    }

    double NumericTextBoxComp::getLargeStepMultiplier() const noexcept {
        return m_largeStepMultiplier;
    }

    void NumericTextBoxComp::setLargeStepMultiplier(double multiplier) {
        if (std::isfinite(multiplier) && multiplier > 0.0) {
            m_largeStepMultiplier = multiplier;
        }
    }

    bool NumericTextBoxComp::getAllowExpressions() const noexcept {
        return m_allowExpressions;
    }

    void
    NumericTextBoxComp::setAllowExpressions(bool allowExpressions) noexcept {
        m_allowExpressions = allowExpressions;
    }

    bool NumericTextBoxComp::getInputValid() const noexcept {
        return m_inputValid;
    }

    const std::string &NumericTextBoxComp::getEditText() const noexcept {
        return m_text;
    }

    const UINumericTextBoxInvalidCallback &
    NumericTextBoxComp::getInvalidCallback() const noexcept {
        return m_invalidCallback;
    }

    void NumericTextBoxComp::setInvalidCallback(
        UINumericTextBoxInvalidCallback callback) {
        m_invalidCallback = std::move(callback);
    }

    int NumericTextBoxComp::intValue() const noexcept {
        return static_cast<int>(m_value);
    }

    void NumericTextBoxComp::setIntValue(int value) {
        const long double next = quantizedValue(normalizedValue(value));
        if (m_value == next) {
            return;
        }
        m_value = next;
        syncTextFromValue(m_textInput.isFocused());
        invalidateTextLayout();
    }

    int NumericTextBoxComp::intMinValue() const noexcept {
        return static_cast<int>(m_minValue);
    }

    int NumericTextBoxComp::intMaxValue() const noexcept {
        return static_cast<int>(m_maxValue);
    }

    void NumericTextBoxComp::setIntValueRange(int minValue, int maxValue) {
        if (maxValue < minValue) {
            std::swap(minValue, maxValue);
        }
        if (m_minValue == minValue && m_maxValue == maxValue &&
            m_clampToRange) {
            return;
        }

        m_minValue = minValue;
        m_maxValue = maxValue;
        m_clampToRange = true;
        m_value = quantizedValue(normalizedValue(m_value));
        syncTextFromValue(m_textInput.isFocused());
        invalidateTextLayout();
    }

    int NumericTextBoxComp::intStep() const noexcept {
        return static_cast<int>(m_step);
    }

    void NumericTextBoxComp::setIntStep(int step) {
        if (step > 0 && m_step != step) {
            m_step = step;
        }
    }

    float NumericTextBoxComp::floatValue() const noexcept {
        return static_cast<float>(m_value);
    }

    void NumericTextBoxComp::setFloatValue(float value) {
        if (!std::isfinite(value)) {
            return;
        }
        const long double next = quantizedValue(normalizedValue(value));
        if (sameNumericValue(m_value, next, true)) {
            return;
        }
        m_value = next;
        syncTextFromValue(m_textInput.isFocused());
        invalidateTextLayout();
    }

    float NumericTextBoxComp::floatMinValue() const noexcept {
        return static_cast<float>(m_minValue);
    }

    float NumericTextBoxComp::floatMaxValue() const noexcept {
        return static_cast<float>(m_maxValue);
    }

    void NumericTextBoxComp::setFloatValueRange(float minValue,
                                                float maxValue) {
        if (!std::isfinite(minValue) || !std::isfinite(maxValue)) {
            return;
        }

        if (maxValue < minValue) {
            std::swap(minValue, maxValue);
        }
        if (m_minValue == minValue && m_maxValue == maxValue &&
            m_clampToRange) {
            return;
        }

        m_minValue = minValue;
        m_maxValue = maxValue;
        m_clampToRange = true;
        m_value = quantizedValue(normalizedValue(m_value));
        syncTextFromValue(m_textInput.isFocused());
        invalidateTextLayout();
    }

    float NumericTextBoxComp::floatStep() const noexcept {
        return static_cast<float>(m_step);
    }

    void NumericTextBoxComp::setFloatStep(float step) {
        if (std::isfinite(step) && step > 0.f && m_step != step) {
            m_step = step;
        }
    }

    int NumericTextBoxComp::floatPrecision() const noexcept {
        return m_precision;
    }

    void NumericTextBoxComp::setFloatPrecision(int precision) {
        const int next = std::clamp(precision, 0, kMaximumFloatPrecision);
        if (m_precision == next) {
            return;
        }

        m_precision = next;
        syncTextFromValue(m_textInput.isFocused());
        invalidateTextLayout();
    }

    UIFloatTextBoxFormat NumericTextBoxComp::floatFormat() const noexcept {
        return m_floatFormat;
    }

    void NumericTextBoxComp::setFloatFormat(UIFloatTextBoxFormat format) {
        if (m_floatFormat == format) {
            return;
        }

        m_floatFormat = format;
        syncTextFromValue(m_textInput.isFocused());
        invalidateTextLayout();
    }

    bool NumericTextBoxComp::allowsScientificNotation() const noexcept {
        return m_allowScientificNotation;
    }

    void NumericTextBoxComp::setAllowsScientificNotation(bool allow) {
        if (m_allowScientificNotation == allow) {
            return;
        }

        m_allowScientificNotation = allow;
        syncTextFromValue(m_textInput.isFocused());
        invalidateTextLayout();
    }

    double NumericTextBoxComp::scalarValue() const noexcept {
        return static_cast<double>(m_value);
    }

    void NumericTextBoxComp::setScalarValue(double value) {
        if (!std::isfinite(value)) {
            return;
        }
        const long double next = quantizedValue(normalizedValue(value));
        if (sameNumericValue(m_value, next, true)) {
            return;
        }
        m_value = next;
        syncTextFromValue(m_textInput.isFocused());
        invalidateTextLayout();
    }

    double NumericTextBoxComp::scalarMinValue() const noexcept {
        return static_cast<double>(m_minValue);
    }

    double NumericTextBoxComp::scalarMaxValue() const noexcept {
        return static_cast<double>(m_maxValue);
    }

    void NumericTextBoxComp::setScalarValueRange(double minValue,
                                                 double maxValue) {
        if (!std::isfinite(minValue) || !std::isfinite(maxValue)) {
            return;
        }
        if (maxValue < minValue) {
            std::swap(minValue, maxValue);
        }
        if (m_minValue == minValue && m_maxValue == maxValue &&
            m_clampToRange) {
            return;
        }

        m_minValue = minValue;
        m_maxValue = maxValue;
        m_clampToRange = true;
        m_value = quantizedValue(normalizedValue(m_value));
        syncTextFromValue(m_textInput.isFocused());
        invalidateTextLayout();
    }

    double NumericTextBoxComp::scalarStep() const noexcept {
        return static_cast<double>(m_step);
    }

    void NumericTextBoxComp::setScalarStep(double step) {
        if (std::isfinite(step) && step > 0.0 && m_step != step) {
            m_step = step;
        }
    }

    int NumericTextBoxComp::scalarPrecision() const noexcept {
        return m_precision;
    }

    void NumericTextBoxComp::setScalarPrecision(int precision) {
        const int next = std::clamp(precision, 0, kMaximumFloatPrecision);
        const bool formatChanged =
            m_floatFormat == UIFloatTextBoxFormat::shortest;
        if (m_precision == next && !formatChanged) {
            return;
        }

        m_precision = next;
        if (formatChanged) {
            m_floatFormat = UIFloatTextBoxFormat::fixed;
        }
        syncTextFromValue(m_textInput.isFocused());
        invalidateTextLayout();
    }

    UIFloatTextBoxFormat NumericTextBoxComp::scalarFormat() const noexcept {
        return m_floatFormat;
    }

    void NumericTextBoxComp::setScalarFormat(UIFloatTextBoxFormat format) {
        setFloatFormat(format);
    }

    bool NumericTextBoxComp::trimsTrailingZeros() const noexcept {
        return m_trimTrailingZeros;
    }

    void NumericTextBoxComp::setTrimsTrailingZeros(bool trimTrailingZeros) {
        if (m_trimTrailingZeros == trimTrailingZeros) {
            return;
        }
        m_trimTrailingZeros = trimTrailingZeros;
        syncTextFromValue(m_textInput.isFocused());
        invalidateTextLayout();
    }

    void NumericTextBoxComp::update(TimeMs ts, SceneState &state) {
        (void)ts;
        if (!m_clearFocus) {
            return;
        }

        m_clearFocus = false;
        if (m_focused) {
            state.clearUIFocus();
        }
    }

    void NumericTextBoxComp::onDraw(SceneDrawContext &state) {
        const auto id = PickingId{
            .runtimeId = resolveRuntimeId(),
            .info = 0,
        };

        m_textInput.syncExternalValue(m_text, effectiveMaxLength());
        const Color focusedBorder =
            m_showValidationError ? m_invalidColor : m_style.activeColor;
        const TextBoxContextDrawOptions options{
            .placeholder = m_placeholder,
            .fontSize = m_style.textStyle.fontSize,
            .padding = stylePadding(),
            .backgroundColor = m_style.backgroundColor,
            .hoverBackgroundColor = m_style.hoverColor,
            .focusedBackgroundColor = m_style.backgroundColor,
            .borderColor = m_style.borderColor,
            .focusedBorderColor = focusedBorder,
            .textColor = m_style.textStyle.textColor,
            .placeholderColor = m_style.textStyle.textColor.withAlpha(0.55f),
            .selectionColor = m_style.activeColor.withAlpha(0.45f),
            .cursorColor =
                m_showValidationError ? m_invalidColor : m_style.activeColor,
            .cursorWidth = kTextBoxCursorWidth,
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

    void NumericTextBoxComp::prepareUI(SceneUIPrepareCtx &state) {
        prepStyle(state.theme);
        initNode(state.sceneState->getUINodeRegistry());

        const auto size = resolveTextBoxSize(state);
        m_node->setWidth(size.x);
        m_node->setHeight(size.y);
        m_node->setPadding(0.f);
        m_node->setMargin(m_style.metrics.margin);
        applyCustomLayoutStyle();

        if (state.parentNode != nullptr) {
            state.parentNode->addChild(m_node);
        }

        m_isUIDirty = false;
    }

    bool NumericTextBoxComp::isFocusable() const {
        return true;
    }

    bool NumericTextBoxComp::wantsKeyboardInput() const {
        return m_focused;
    }

    void NumericTextBoxComp::onFocusGained(const Events::FocusEvent &e) {
        UISceneComponent::onFocusGained(e);
        m_focusStartValue = m_value;
        m_showValidationError = false;
        long double ignored = 0.L;
        m_inputValid = parseCompleteValue(m_text, ignored);
        m_textInput.focus(m_text, effectiveMaxLength(), m_selectAllOnFocus);
    }

    void NumericTextBoxComp::onFocusLost(const Events::FocusEvent &e) {
        UISceneComponent::onFocusLost(e);
        if (!commitText()) {
            restoreValidText();
        }
        m_textInput.blur();
        m_showValidationError = false;
    }

    bool NumericTextBoxComp::onMouseButton(const Events::MouseButtonEvent &e) {
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

    bool NumericTextBoxComp::onPointerMove(const Events::MouseMoveEvent &e) {
        m_textInput.queuePointerMove(e.mousePos);
        return true;
    }

    bool NumericTextBoxComp::onMouseWheel(const Events::MouseWheelEvent &e) {
        if (!m_focused || !m_stepOnMouseWheel || e.delta.y == 0.f) {
            return false;
        }

        prepareForStep();
        applyStep(e.delta.y > 0.f ? 1.L : -1.L, 1.L);
        return true;
    }

    bool NumericTextBoxComp::onKeyEvent(const SceneEvent &evt) {
        if (evt.type == SceneEvent::Type::key &&
            (evt.data.keyPress.action == KeyAction::press ||
             evt.data.keyPress.action == KeyAction::hold)) {
            long double direction = 0.L;
            long double multiplier = 1.L;
            switch (evt.data.keyPress.keycode) {
            case KeyCode::arrowUp:
                direction = 1.L;
                multiplier = evt.isShiftPressed ? m_largeStepMultiplier : 1.L;
                break;
            case KeyCode::arrowDown:
                direction = -1.L;
                multiplier = evt.isShiftPressed ? m_largeStepMultiplier : 1.L;
                break;
            case KeyCode::pageUp:
                direction = 1.L;
                multiplier = m_largeStepMultiplier;
                break;
            case KeyCode::pageDown:
                direction = -1.L;
                multiplier = m_largeStepMultiplier;
                break;
            default:
                break;
            }

            if (direction != 0.L) {
                if (m_kind != ValueKind::integer && evt.isAltPressed) {
                    multiplier *= 0.1L;
                }
                prepareForStep();
                applyStep(direction, multiplier);
                return true;
            }
        }

        const size_t previousCursor = m_textInput.cursorPos();
        const size_t previousSelectionAnchor = m_textInput.selectionAnchorPos();
        const auto result = m_textInput.handleEvent(evt);
        applyTextInputResult(result, previousCursor, previousSelectionAnchor);
        return result.handled;
    }

    bool NumericTextBoxComp::hasPointerCapture() const {
        return m_textInput.hasPointerCapture();
    }

    Core::Viewport::SceneCursor NumericTextBoxComp::getCursor() const {
        return Core::Viewport::SceneCursor::text;
    }

    void NumericTextBoxComp::prepStyle(
        const std::shared_ptr<Core::Style::BessTheme> &theme) {
        UISceneComponent::prepStyle(theme);
        m_invalidColor = theme->getColorScheme().getColors().error;
    }

    long double NumericTextBoxComp::normalizedValue(long double value) const {
        long double typeMin = std::numeric_limits<double>::lowest();
        long double typeMax = std::numeric_limits<double>::max();
        if (m_kind == ValueKind::integer) {
            typeMin = std::numeric_limits<int>::lowest();
            typeMax = std::numeric_limits<int>::max();
        } else if (m_kind == ValueKind::floatingPoint) {
            typeMin = std::numeric_limits<float>::lowest();
            typeMax = std::numeric_limits<float>::max();
        }

        value = std::clamp(value, typeMin, typeMax);
        if (m_clampToRange) {
            value = std::clamp(value, m_minValue, m_maxValue);
        }
        return value;
    }

    long double NumericTextBoxComp::quantizedValue(long double value) const {
        if (m_kind == ValueKind::integer) {
            return static_cast<int>(std::round(value));
        }
        if (m_kind == ValueKind::floatingPoint) {
            return static_cast<float>(value);
        }
        return static_cast<double>(value);
    }

    bool
    NumericTextBoxComp::isSyntacticallyValidEdit(std::string_view text) const {
        if (m_allowExpressions) {
            return isExpressionEdit(
                text, m_kind != ValueKind::integer, m_allowScientificNotation);
        }
        return m_kind == ValueKind::integer
                   ? isIntegerEdit(text)
                   : isFloatEdit(text, m_allowScientificNotation);
    }

    bool NumericTextBoxComp::parseCompleteValue(std::string_view text,
                                                long double &value) const {
        if (!m_allowExpressions && m_kind == ValueKind::integer) {
            int parsed = 0;
            if (!parseInteger(text, parsed)) {
                return false;
            }
            value = parsed;
            return true;
        }

        if (!m_allowExpressions && m_kind == ValueKind::floatingPoint) {
            float parsed = 0.f;
            if (!parseFloat(text, parsed)) {
                return false;
            }
            value = parsed;
            return true;
        }

        if (!m_allowExpressions) {
            double parsed = 0.0;
            if (!parseDouble(text, parsed)) {
                return false;
            }
            value = parsed;
            return true;
        }

        double evaluated = 0.0;
        if (!evaluateArithmeticExpression(text, evaluated)) {
            return false;
        }

        if (m_kind == ValueKind::integer) {
            if (evaluated < std::numeric_limits<int>::lowest() ||
                evaluated > std::numeric_limits<int>::max() ||
                std::trunc(evaluated) != evaluated) {
                return false;
            }
            value = static_cast<int>(evaluated);
            return true;
        }
        if (m_kind == ValueKind::floatingPoint) {
            const float narrowed = static_cast<float>(evaluated);
            if (!std::isfinite(narrowed) ||
                (evaluated != 0.0 && narrowed == 0.f)) {
                return false;
            }
            value = narrowed;
            return true;
        }

        value = evaluated;
        return true;
    }

    std::string NumericTextBoxComp::formatValue() const {
        if (m_kind == ValueKind::integer) {
            return formatInteger(static_cast<int>(m_value));
        }
        std::string text;
        if (m_kind == ValueKind::floatingPoint) {
            text = formatFloating(static_cast<float>(m_value),
                                  m_floatFormat,
                                  m_precision,
                                  m_trimTrailingZeros);
        } else {
            text = formatFloating(static_cast<double>(m_value),
                                  m_floatFormat,
                                  m_precision,
                                  m_trimTrailingZeros);
        }
        if (!m_allowScientificNotation &&
            (text.find('e') != std::string::npos ||
             text.find('E') != std::string::npos)) {
            text = m_kind == ValueKind::floatingPoint
                       ? formatWithoutExponent(static_cast<float>(m_value))
                       : formatWithoutExponent(static_cast<double>(m_value));
        }
        return text;
    }

    size_t NumericTextBoxComp::effectiveMaxLength() const noexcept {
        return std::max(m_maxLength, m_text.size());
    }

    glm::vec2
    NumericTextBoxComp::resolveTextBoxSize(SceneUIPrepareCtx &state) const {
        auto size = m_textBoxSize;
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

    glm::vec2 NumericTextBoxComp::stylePadding() const {
        const auto &padding = m_style.metrics.padding;
        return {
            std::max(padding.left, padding.right),
            std::max(padding.top, padding.bottom),
        };
    }

    void NumericTextBoxComp::syncTextFromValue(bool updateFocusedContext) {
        m_text = formatValue();
        m_inputValid = true;
        m_showValidationError = false;
        if (updateFocusedContext) {
            m_textInput.replaceText(m_text, effectiveMaxLength(), false);
        }
    }

    void NumericTextBoxComp::setValueFromUser(long double value,
                                              bool commitDisplayText) {
        if (!isFinite(value)) {
            return;
        }

        const long double previous = m_value;
        m_value = quantizedValue(normalizedValue(value));
        if (commitDisplayText) {
            syncTextFromValue(m_textInput.isFocused());
        }
        if (!sameNumericValue(
                previous, m_value, m_kind != ValueKind::integer)) {
            notifyValueChanged();
        }
        invalidateTextLayout();
    }

    bool NumericTextBoxComp::commitText() {
        const auto &candidate = m_textInput.text();
        long double parsed = 0.L;
        if (!parseCompleteValue(candidate, parsed)) {
            m_inputValid = false;
            reportInvalidInput();
            return false;
        }

        m_text = candidate;
        m_inputValid = true;
        m_showValidationError = false;
        setValueFromUser(parsed, true);
        return true;
    }

    void NumericTextBoxComp::restoreValidText() {
        syncTextFromValue(m_textInput.isFocused());
        invalidateTextLayout();
    }

    void NumericTextBoxComp::reportInvalidInput() {
        if (m_showValidationError) {
            return;
        }

        m_showValidationError = true;
        if (m_invalidCallback) {
            m_invalidCallback(m_textInput.text());
        }
    }

    void
    NumericTextBoxComp::applyTextInputResult(const TextBoxContextResult &result,
                                             size_t previousCursor,
                                             size_t previousSelectionAnchor) {
        if (result.canceled) {
            setValueFromUser(m_focusStartValue, true);
            m_inputValid = true;
            m_showValidationError = false;
            notifyCanceled();
            m_clearFocus = true;
            return;
        }

        if (result.changed) {
            const auto &candidate = m_textInput.text();
            if (!isSyntacticallyValidEdit(candidate)) {
                m_textInput.restoreEditState(m_text,
                                             effectiveMaxLength(),
                                             previousCursor,
                                             previousSelectionAnchor);
                return;
            }

            m_text = candidate;
            m_showValidationError = false;
            long double parsed = 0.L;
            m_inputValid = parseCompleteValue(candidate, parsed);
            if (m_inputValid) {
                setValueFromUser(parsed, false);
            } else {
                invalidateTextLayout();
            }
        }

        if (result.submitted) {
            if (commitText()) {
                notifySubmitted();
                m_clearFocus = true;
            }
        }
    }

    void NumericTextBoxComp::applyStep(long double direction,
                                       long double multiplier) {
        const long double delta = m_step * multiplier * direction;
        long double next = m_value + delta;
        if (!isFinite(next)) {
            const long double limit =
                m_kind == ValueKind::integer
                    ? static_cast<long double>(std::numeric_limits<int>::max())
                : m_kind == ValueKind::floatingPoint
                    ? static_cast<long double>(
                          std::numeric_limits<float>::max())
                    : static_cast<long double>(
                          std::numeric_limits<double>::max());
            next = direction > 0.L ? limit : -limit;
        }
        setValueFromUser(next, true);
    }

    void NumericTextBoxComp::prepareForStep() {
        long double parsed = 0.L;
        if (parseCompleteValue(m_textInput.text(), parsed)) {
            m_text = m_textInput.text();
            setValueFromUser(parsed, true);
        } else {
            restoreValidText();
        }
    }

    void NumericTextBoxComp::invalidateTextLayout() {
        if (m_textBoxSize.x <= 0.f) {
            makeUIDirty();
        }
    }
} // namespace Bess::Canvas::UI
