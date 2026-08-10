#include "bess_core/scene/scene_ui/controls/float_text_box_comp.h"
#include <utility>

namespace Bess::Canvas::UI {
    FloatTextBoxComp::FloatTextBoxComp()
        : NumericTextBoxComp(ValueKind::floatingPoint) {
    }

    std::shared_ptr<FloatTextBoxComp>
    FloatTextBoxComp::create(const CompConfig &config) {
        return create(0.f, nullptr, config);
    }

    std::shared_ptr<FloatTextBoxComp>
    FloatTextBoxComp::create(float value,
                             const UIFloatTextBoxCallback &changedCallback,
                             const CompConfig &config) {
        auto input = std::make_shared<FloatTextBoxComp>();
        input->setValue(value);
        input->setChangedCallback(changedCallback);
        applyCompConfig(input, config);
        return input;
    }

    float FloatTextBoxComp::getValue() const noexcept {
        return floatValue();
    }

    void FloatTextBoxComp::setValue(float value) {
        setFloatValue(value);
    }

    float FloatTextBoxComp::getMinValue() const noexcept {
        return floatMinValue();
    }

    void FloatTextBoxComp::setMinValue(float value) {
        setFloatValueRange(value, getMaxValue());
    }

    float FloatTextBoxComp::getMaxValue() const noexcept {
        return floatMaxValue();
    }

    void FloatTextBoxComp::setMaxValue(float value) {
        setFloatValueRange(getMinValue(), value);
    }

    void FloatTextBoxComp::setValueRange(float minValue, float maxValue) {
        setFloatValueRange(minValue, maxValue);
    }

    float FloatTextBoxComp::getStep() const noexcept {
        return floatStep();
    }

    void FloatTextBoxComp::setStep(float step) {
        setFloatStep(step);
    }

    int FloatTextBoxComp::getPrecision() const noexcept {
        return floatPrecision();
    }

    void FloatTextBoxComp::setPrecision(int precision) {
        setFloatPrecision(precision);
    }

    UIFloatTextBoxFormat FloatTextBoxComp::getFormat() const noexcept {
        return floatFormat();
    }

    void FloatTextBoxComp::setFormat(UIFloatTextBoxFormat format) {
        setFloatFormat(format);
    }

    bool FloatTextBoxComp::getAllowScientificNotation() const noexcept {
        return allowsScientificNotation();
    }

    void FloatTextBoxComp::setAllowScientificNotation(bool allow) {
        setAllowsScientificNotation(allow);
    }

    const UIFloatTextBoxCallback &
    FloatTextBoxComp::getChangedCallback() const noexcept {
        return m_changedCallback;
    }

    void FloatTextBoxComp::setChangedCallback(UIFloatTextBoxCallback callback) {
        m_changedCallback = std::move(callback);
    }

    const UIFloatTextBoxCallback &
    FloatTextBoxComp::getSubmittedCallback() const noexcept {
        return m_submittedCallback;
    }

    void
    FloatTextBoxComp::setSubmittedCallback(UIFloatTextBoxCallback callback) {
        m_submittedCallback = std::move(callback);
    }

    const UIFloatTextBoxCallback &
    FloatTextBoxComp::getCanceledCallback() const noexcept {
        return m_canceledCallback;
    }

    void
    FloatTextBoxComp::setCanceledCallback(UIFloatTextBoxCallback callback) {
        m_canceledCallback = std::move(callback);
    }

    void FloatTextBoxComp::notifyValueChanged() {
        if (m_changedCallback) {
            m_changedCallback(getValue());
        }
    }

    void FloatTextBoxComp::notifySubmitted() {
        if (m_submittedCallback) {
            m_submittedCallback(getValue());
        }
    }

    void FloatTextBoxComp::notifyCanceled() {
        if (m_canceledCallback) {
            m_canceledCallback(getValue());
        }
    }
} // namespace Bess::Canvas::UI
