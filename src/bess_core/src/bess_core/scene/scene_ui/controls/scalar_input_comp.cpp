#include "bess_core/scene/scene_ui/controls/scalar_input_comp.h"
#include <utility>

namespace Bess::Canvas::UI {
    ScalarInputComp::ScalarInputComp()
        : NumericTextBoxComp(ValueKind::doublePrecision) {
    }

    std::shared_ptr<ScalarInputComp>
    ScalarInputComp::create(const CompConfig &config) {
        return create(0.0, nullptr, config);
    }

    std::shared_ptr<ScalarInputComp>
    ScalarInputComp::create(double value,
                            const UIScalarInputCallback &changedCallback,
                            const CompConfig &config) {
        auto input = std::make_shared<ScalarInputComp>();
        input->setValue(value);
        input->setChangedCallback(changedCallback);
        applyCompConfig(input, config);
        return input;
    }

    double ScalarInputComp::getValue() const noexcept {
        return scalarValue();
    }

    void ScalarInputComp::setValue(double value) {
        setScalarValue(value);
    }

    double ScalarInputComp::getMinValue() const noexcept {
        return scalarMinValue();
    }

    void ScalarInputComp::setMinValue(double value) {
        setScalarValueRange(value, getMaxValue());
    }

    double ScalarInputComp::getMaxValue() const noexcept {
        return scalarMaxValue();
    }

    void ScalarInputComp::setMaxValue(double value) {
        setScalarValueRange(getMinValue(), value);
    }

    void ScalarInputComp::setValueRange(double minValue, double maxValue) {
        setScalarValueRange(minValue, maxValue);
    }

    double ScalarInputComp::getStep() const noexcept {
        return scalarStep();
    }

    void ScalarInputComp::setStep(double step) {
        setScalarStep(step);
    }

    int ScalarInputComp::getPrecision() const noexcept {
        return scalarPrecision();
    }

    void ScalarInputComp::setPrecision(int precision) {
        setScalarPrecision(precision);
    }

    UIScalarInputFormat ScalarInputComp::getFormat() const noexcept {
        return scalarFormat();
    }

    void ScalarInputComp::setFormat(UIScalarInputFormat format) {
        setScalarFormat(format);
    }

    bool ScalarInputComp::getTrimTrailingZeros() const noexcept {
        return trimsTrailingZeros();
    }

    void ScalarInputComp::setTrimTrailingZeros(bool trimTrailingZeros) {
        setTrimsTrailingZeros(trimTrailingZeros);
    }

    bool ScalarInputComp::getAllowScientificNotation() const noexcept {
        return allowsScientificNotation();
    }

    void ScalarInputComp::setAllowScientificNotation(bool allow) {
        setAllowsScientificNotation(allow);
    }

    const glm::vec2 &ScalarInputComp::getInputSize() const noexcept {
        return getTextBoxSize();
    }

    glm::vec2 &ScalarInputComp::getInputSize() noexcept {
        return getTextBoxSize();
    }

    void ScalarInputComp::setInputSize(const glm::vec2 &size) {
        setTextBoxSize(size);
    }

    const UIScalarInputCallback &
    ScalarInputComp::getChangedCallback() const noexcept {
        return m_changedCallback;
    }

    void ScalarInputComp::setChangedCallback(UIScalarInputCallback callback) {
        m_changedCallback = std::move(callback);
    }

    const UIScalarInputCallback &
    ScalarInputComp::getSubmittedCallback() const noexcept {
        return m_submittedCallback;
    }

    void ScalarInputComp::setSubmittedCallback(UIScalarInputCallback callback) {
        m_submittedCallback = std::move(callback);
    }

    const UIScalarInputCallback &
    ScalarInputComp::getCanceledCallback() const noexcept {
        return m_canceledCallback;
    }

    void ScalarInputComp::setCanceledCallback(UIScalarInputCallback callback) {
        m_canceledCallback = std::move(callback);
    }

    void ScalarInputComp::notifyValueChanged() {
        if (m_changedCallback) {
            m_changedCallback(getValue());
        }
    }

    void ScalarInputComp::notifySubmitted() {
        if (m_submittedCallback) {
            m_submittedCallback(getValue());
        }
    }

    void ScalarInputComp::notifyCanceled() {
        if (m_canceledCallback) {
            m_canceledCallback(getValue());
        }
    }
} // namespace Bess::Canvas::UI
