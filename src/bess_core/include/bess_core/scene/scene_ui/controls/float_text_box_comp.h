#pragma once

#include "common/bess_api.h"

#include "bess_core/scene/scene_ui/controls/numeric_text_box_comp.h"
#include <functional>
#include <memory>

namespace Bess::Canvas::UI {

    using UIFloatTextBoxCallback = std::function<void(float)>;

    /** A finite, type-safe floating-point variant of TextBoxComp. */
    class BESS_API FloatTextBoxComp final : public NumericTextBoxComp {
      public:
        FloatTextBoxComp();
        FloatTextBoxComp(const FloatTextBoxComp &) = default;
        FloatTextBoxComp(FloatTextBoxComp &&) = default;
        ~FloatTextBoxComp() override = default;
        FloatTextBoxComp &operator=(const FloatTextBoxComp &) = default;
        FloatTextBoxComp &operator=(FloatTextBoxComp &&) = default;

        static std::shared_ptr<FloatTextBoxComp>
        create(const CompConfig &config);
        static std::shared_ptr<FloatTextBoxComp>
        create(float value = 0.f,
               const UIFloatTextBoxCallback &changedCallback = nullptr,
               const CompConfig &config = CompConfig{});

        [[nodiscard]] float getValue() const noexcept;
        /** Ignores NaN and infinity. */
        void setValue(float value);
        [[nodiscard]] float getMinValue() const noexcept;
        void setMinValue(float value);
        [[nodiscard]] float getMaxValue() const noexcept;
        void setMaxValue(float value);
        /** Ignores non-finite endpoints, normalizes order, and clamps. */
        void setValueRange(float minValue, float maxValue);
        [[nodiscard]] float getStep() const noexcept;
        /** Ignores non-finite and non-positive steps. */
        void setStep(float step);

        /**
         * Precision is ignored by shortest mode. In general mode it is the
         * significant digit count; in fixed/scientific it is the number of
         * digits after the decimal point. Values are clamped to [0, 32].
         */
        [[nodiscard]] int getPrecision() const noexcept;
        void setPrecision(int precision);
        [[nodiscard]] UIFloatTextBoxFormat getFormat() const noexcept;
        void setFormat(UIFloatTextBoxFormat format);
        /** Also suppresses exponent notation in the formatted edit text. */
        [[nodiscard]] bool getAllowScientificNotation() const noexcept;
        void setAllowScientificNotation(bool allow);

        [[nodiscard]] const UIFloatTextBoxCallback &
        getChangedCallback() const noexcept;
        void setChangedCallback(UIFloatTextBoxCallback callback);
        [[nodiscard]] const UIFloatTextBoxCallback &
        getSubmittedCallback() const noexcept;
        void setSubmittedCallback(UIFloatTextBoxCallback callback);
        [[nodiscard]] const UIFloatTextBoxCallback &
        getCanceledCallback() const noexcept;
        void setCanceledCallback(UIFloatTextBoxCallback callback);

      private:
        void notifyValueChanged() override;
        void notifySubmitted() override;
        void notifyCanceled() override;

        UIFloatTextBoxCallback m_changedCallback;
        UIFloatTextBoxCallback m_submittedCallback;
        UIFloatTextBoxCallback m_canceledCallback;
    };
} // namespace Bess::Canvas::UI
