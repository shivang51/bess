#pragma once

#include "common/bess_api.h"

#include "bess_core/scene/scene_ui/controls/numeric_text_box_comp.h"
#include <functional>
#include <memory>
#include <string>

namespace Bess::Canvas::UI {

    using UIScalarInputCallback = std::function<void(double)>;
    using UIScalarInputInvalidCallback = UINumericTextBoxInvalidCallback;
    using UIScalarInputFormat = UIFloatTextBoxFormat;

    /** A finite, double-precision numeric text box with expression support. */
    class BESS_API ScalarInputComp final : public NumericTextBoxComp {
      public:
        ScalarInputComp();
        ScalarInputComp(const ScalarInputComp &) = default;
        ScalarInputComp(ScalarInputComp &&) = default;
        ~ScalarInputComp() override = default;
        ScalarInputComp &operator=(const ScalarInputComp &) = default;
        ScalarInputComp &operator=(ScalarInputComp &&) = default;

        static std::shared_ptr<ScalarInputComp>
        create(const CompConfig &config);
        static std::shared_ptr<ScalarInputComp>
        create(double value = 0.0,
               const UIScalarInputCallback &changedCallback = nullptr,
               const CompConfig &config = CompConfig{});

        [[nodiscard]] double getValue() const noexcept;
        void setValue(double value);
        [[nodiscard]] double getMinValue() const noexcept;
        void setMinValue(double value);
        [[nodiscard]] double getMaxValue() const noexcept;
        void setMaxValue(double value);
        /** Normalizes reversed endpoints and enables range clamping. */
        void setValueRange(double minValue, double maxValue);
        [[nodiscard]] double getStep() const noexcept;
        /** Ignores non-finite and non-positive steps. */
        void setStep(double step);
        [[nodiscard]] int getPrecision() const noexcept;
        /**
         * Sets precision in [0, 32]. For backward compatibility, calling this
         * while using shortest formatting switches to fixed formatting.
         */
        void setPrecision(int precision);
        [[nodiscard]] UIScalarInputFormat getFormat() const noexcept;
        void setFormat(UIScalarInputFormat format);
        [[nodiscard]] bool getTrimTrailingZeros() const noexcept;
        void setTrimTrailingZeros(bool trimTrailingZeros);
        [[nodiscard]] bool getAllowScientificNotation() const noexcept;
        void setAllowScientificNotation(bool allow);

        [[nodiscard]] const glm::vec2 &getInputSize() const noexcept;
        [[nodiscard]] glm::vec2 &getInputSize() noexcept;
        void setInputSize(const glm::vec2 &size);

        [[nodiscard]] const UIScalarInputCallback &
        getChangedCallback() const noexcept;
        void setChangedCallback(UIScalarInputCallback callback);
        [[nodiscard]] const UIScalarInputCallback &
        getSubmittedCallback() const noexcept;
        void setSubmittedCallback(UIScalarInputCallback callback);
        [[nodiscard]] const UIScalarInputCallback &
        getCanceledCallback() const noexcept;
        void setCanceledCallback(UIScalarInputCallback callback);

      private:
        void notifyValueChanged() override;
        void notifySubmitted() override;
        void notifyCanceled() override;

        UIScalarInputCallback m_changedCallback;
        UIScalarInputCallback m_submittedCallback;
        UIScalarInputCallback m_canceledCallback;
    };
} // namespace Bess::Canvas::UI
