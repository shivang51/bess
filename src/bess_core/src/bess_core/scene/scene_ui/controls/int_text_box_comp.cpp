#include "bess_core/scene/scene_ui/controls/int_text_box_comp.h"
#include <utility>

namespace Bess::Canvas::UI {
    IntTextBoxComp::IntTextBoxComp() : NumericTextBoxComp(ValueKind::integer) {
    }

    std::shared_ptr<IntTextBoxComp>
    IntTextBoxComp::create(const CompConfig &config) {
        return create(0, nullptr, config);
    }

    std::shared_ptr<IntTextBoxComp>
    IntTextBoxComp::create(int value,
                           const UIIntTextBoxCallback &changedCallback,
                           const CompConfig &config) {
        auto input = std::make_shared<IntTextBoxComp>();
        input->setValue(value);
        input->setChangedCallback(changedCallback);
        applyCompConfig(input, config);
        return input;
    }

    int IntTextBoxComp::getValue() const noexcept {
        return intValue();
    }

    void IntTextBoxComp::setValue(int value) {
        setIntValue(value);
    }

    int IntTextBoxComp::getMinValue() const noexcept {
        return intMinValue();
    }

    void IntTextBoxComp::setMinValue(int value) {
        setIntValueRange(value, getMaxValue());
    }

    int IntTextBoxComp::getMaxValue() const noexcept {
        return intMaxValue();
    }

    void IntTextBoxComp::setMaxValue(int value) {
        setIntValueRange(getMinValue(), value);
    }

    void IntTextBoxComp::setValueRange(int minValue, int maxValue) {
        setIntValueRange(minValue, maxValue);
    }

    int IntTextBoxComp::getStep() const noexcept {
        return intStep();
    }

    void IntTextBoxComp::setStep(int step) {
        setIntStep(step);
    }

    const UIIntTextBoxCallback &
    IntTextBoxComp::getChangedCallback() const noexcept {
        return m_changedCallback;
    }

    void IntTextBoxComp::setChangedCallback(UIIntTextBoxCallback callback) {
        m_changedCallback = std::move(callback);
    }

    const UIIntTextBoxCallback &
    IntTextBoxComp::getSubmittedCallback() const noexcept {
        return m_submittedCallback;
    }

    void IntTextBoxComp::setSubmittedCallback(UIIntTextBoxCallback callback) {
        m_submittedCallback = std::move(callback);
    }

    const UIIntTextBoxCallback &
    IntTextBoxComp::getCanceledCallback() const noexcept {
        return m_canceledCallback;
    }

    void IntTextBoxComp::setCanceledCallback(UIIntTextBoxCallback callback) {
        m_canceledCallback = std::move(callback);
    }

    void IntTextBoxComp::notifyValueChanged() {
        if (m_changedCallback) {
            m_changedCallback(getValue());
        }
    }

    void IntTextBoxComp::notifySubmitted() {
        if (m_submittedCallback) {
            m_submittedCallback(getValue());
        }
    }

    void IntTextBoxComp::notifyCanceled() {
        if (m_canceledCallback) {
            m_canceledCallback(getValue());
        }
    }
} // namespace Bess::Canvas::UI
