#pragma once

#include "common/bess_api.h"

#include "bess_core/scene/scene_state/scene_state.h"
#include "bess_core/scene/scene_ui/text_box_context.h"
#include "bess_core/scene/scene_ui/ui_scene_component.h"
#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <string_view>

namespace Bess::Canvas::UI {

    /** Controls how a floating-point numeric text box formats committed values.
     */
    enum class UIFloatTextBoxFormat : uint8_t {
        /** The shortest decimal representation that round-trips exactly. */
        shortest,
        /** Significant digits, switching notation when appropriate. */
        general,
        /** A fixed number of digits after the decimal point. */
        fixed,
        /** Scientific notation with a fixed number of fractional digits. */
        scientific,
    };

    using UINumericTextBoxInvalidCallback =
        std::function<void(const std::string &)>;

    /**
     * Shared text editing and presentation for typed numeric text boxes.
     *
     * This is the implementation base for IntTextBoxComp, FloatTextBoxComp,
     * and ScalarInputComp. Programmatic value changes never invoke callbacks;
     * valid user edits do. Enter commits, Escape restores the value present
     * when focus was gained, and an invalid Enter keeps focus for correction.
     */
    class BESS_API NumericTextBoxComp : public UISceneComponent {
      public:
        NumericTextBoxComp(const NumericTextBoxComp &) = default;
        NumericTextBoxComp(NumericTextBoxComp &&) = default;
        ~NumericTextBoxComp() override = default;
        NumericTextBoxComp &operator=(const NumericTextBoxComp &) = default;
        NumericTextBoxComp &operator=(NumericTextBoxComp &&) = default;

        [[nodiscard]] const std::string &getPlaceholder() const noexcept;
        void setPlaceholder(std::string placeholder);
        [[nodiscard]] const glm::vec2 &getTextBoxSize() const noexcept;
        [[nodiscard]] glm::vec2 &getTextBoxSize() noexcept;
        void setTextBoxSize(const glm::vec2 &size);
        [[nodiscard]] size_t getMaxLength() const noexcept;
        void setMaxLength(size_t maxLength);
        [[nodiscard]] bool getClampToRange() const noexcept;
        void setClampToRange(bool clampToRange);
        [[nodiscard]] bool getSelectAllOnFocus() const noexcept;
        void setSelectAllOnFocus(bool selectAllOnFocus) noexcept;
        [[nodiscard]] bool getStepOnMouseWheel() const noexcept;
        void setStepOnMouseWheel(bool stepOnMouseWheel) noexcept;
        [[nodiscard]] double getLargeStepMultiplier() const noexcept;
        void setLargeStepMultiplier(double multiplier);
        /** Enables +, -, *, /, %, unary signs, and parentheses. */
        [[nodiscard]] bool getAllowExpressions() const noexcept;
        void setAllowExpressions(bool allowExpressions) noexcept;

        /** True when the current edit is a complete, representable number. */
        [[nodiscard]] bool getInputValid() const noexcept;
        /** Current edit text; it may be a valid transient such as "-". */
        [[nodiscard]] const std::string &getEditText() const noexcept;

        [[nodiscard]] const UINumericTextBoxInvalidCallback &
        getInvalidCallback() const noexcept;
        void setInvalidCallback(UINumericTextBoxInvalidCallback callback);

        void update(TimeMs ts, SceneState &state) override;
        void onDraw(SceneDrawContext &state) override;
        void prepareUI(SceneUIPrepareCtx &state) override;
        bool isFocusable() const override;
        bool wantsKeyboardInput() const override;
        void onFocusGained(const Events::FocusEvent &e) override;
        void onFocusLost(const Events::FocusEvent &e) override;
        bool onMouseButton(const Events::MouseButtonEvent &e) override;
        bool onPointerMove(const Events::MouseMoveEvent &e) override;
        bool onMouseWheel(const Events::MouseWheelEvent &e) override;
        bool onKeyEvent(const SceneEvent &evt) override;
        bool hasPointerCapture() const override;
        Core::Viewport::SceneCursor getCursor() const override;

      protected:
        enum class ValueKind : uint8_t {
            integer,
            floatingPoint,
            doublePrecision,
        };

        explicit NumericTextBoxComp(ValueKind kind);

        [[nodiscard]] int intValue() const noexcept;
        void setIntValue(int value);
        [[nodiscard]] int intMinValue() const noexcept;
        [[nodiscard]] int intMaxValue() const noexcept;
        void setIntValueRange(int minValue, int maxValue);
        [[nodiscard]] int intStep() const noexcept;
        void setIntStep(int step);

        [[nodiscard]] float floatValue() const noexcept;
        void setFloatValue(float value);
        [[nodiscard]] float floatMinValue() const noexcept;
        [[nodiscard]] float floatMaxValue() const noexcept;
        void setFloatValueRange(float minValue, float maxValue);
        [[nodiscard]] float floatStep() const noexcept;
        void setFloatStep(float step);
        [[nodiscard]] int floatPrecision() const noexcept;
        void setFloatPrecision(int precision);
        [[nodiscard]] UIFloatTextBoxFormat floatFormat() const noexcept;
        void setFloatFormat(UIFloatTextBoxFormat format);
        [[nodiscard]] bool allowsScientificNotation() const noexcept;
        void setAllowsScientificNotation(bool allow);

        [[nodiscard]] double scalarValue() const noexcept;
        void setScalarValue(double value);
        [[nodiscard]] double scalarMinValue() const noexcept;
        [[nodiscard]] double scalarMaxValue() const noexcept;
        void setScalarValueRange(double minValue, double maxValue);
        [[nodiscard]] double scalarStep() const noexcept;
        void setScalarStep(double step);
        [[nodiscard]] int scalarPrecision() const noexcept;
        void setScalarPrecision(int precision);
        [[nodiscard]] UIFloatTextBoxFormat scalarFormat() const noexcept;
        void setScalarFormat(UIFloatTextBoxFormat format);
        [[nodiscard]] bool trimsTrailingZeros() const noexcept;
        void setTrimsTrailingZeros(bool trimTrailingZeros);

        virtual void notifyValueChanged() = 0;
        virtual void notifySubmitted() = 0;
        virtual void notifyCanceled() = 0;

      private:
        void prepStyle(
            const std::shared_ptr<Core::Style::BessTheme> &theme) override;

        [[nodiscard]] long double normalizedValue(long double value) const;
        [[nodiscard]] long double quantizedValue(long double value) const;
        [[nodiscard]] bool
        isSyntacticallyValidEdit(std::string_view text) const;
        [[nodiscard]] bool parseCompleteValue(std::string_view text,
                                              long double &value) const;
        [[nodiscard]] std::string formatValue() const;
        [[nodiscard]] size_t effectiveMaxLength() const noexcept;
        [[nodiscard]] glm::vec2
        resolveTextBoxSize(SceneUIPrepareCtx &state) const;
        [[nodiscard]] glm::vec2 stylePadding() const;

        void syncTextFromValue(bool updateFocusedContext);
        void setValueFromUser(long double value, bool commitDisplayText);
        [[nodiscard]] bool commitText();
        void restoreValidText();
        void reportInvalidInput();
        void applyTextInputResult(const TextBoxContextResult &result,
                                  size_t previousCursor,
                                  size_t previousSelectionAnchor);
        void applyStep(long double direction, long double multiplier);
        void prepareForStep();
        void invalidateTextLayout();

        ValueKind m_kind;
        long double m_value = 0.L;
        long double m_focusStartValue = 0.L;
        long double m_minValue = 0.L;
        long double m_maxValue = 1.L;
        long double m_step = 1.L;
        bool m_clampToRange = false;
        int m_precision = 6;
        UIFloatTextBoxFormat m_floatFormat = UIFloatTextBoxFormat::shortest;
        bool m_allowScientificNotation = true;
        std::string m_placeholder = "0";
        std::string m_text = "0";
        glm::vec2 m_textBoxSize{72.f, 0.f};
        size_t m_maxLength = 128;
        bool m_selectAllOnFocus = true;
        bool m_stepOnMouseWheel = true;
        double m_largeStepMultiplier = 10.0;
        bool m_allowExpressions = true;
        bool m_trimTrailingZeros = false;
        bool m_inputValid = true;
        bool m_showValidationError = false;
        UINumericTextBoxInvalidCallback m_invalidCallback;
        TextBoxContext m_textInput;
        bool m_clearFocus = false;
        Color m_invalidColor;
    };
} // namespace Bess::Canvas::UI
