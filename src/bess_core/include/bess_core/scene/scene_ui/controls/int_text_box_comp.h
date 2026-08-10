#pragma once

#include "common/bess_api.h"

#include "bess_core/scene/scene_ui/controls/numeric_text_box_comp.h"
#include <functional>
#include <memory>

namespace Bess::Canvas::UI {

    using UIIntTextBoxCallback = std::function<void(int)>;

    /** A type-safe, decimal integer variant of TextBoxComp. */
    class BESS_API IntTextBoxComp final : public NumericTextBoxComp {
      public:
        IntTextBoxComp();
        IntTextBoxComp(const IntTextBoxComp &) = default;
        IntTextBoxComp(IntTextBoxComp &&) = default;
        ~IntTextBoxComp() override = default;
        IntTextBoxComp &operator=(const IntTextBoxComp &) = default;
        IntTextBoxComp &operator=(IntTextBoxComp &&) = default;

        static std::shared_ptr<IntTextBoxComp> create(const CompConfig &config);
        static std::shared_ptr<IntTextBoxComp>
        create(int value = 0,
               const UIIntTextBoxCallback &changedCallback = nullptr,
               const CompConfig &config = CompConfig{});

        [[nodiscard]] int getValue() const noexcept;
        void setValue(int value);
        [[nodiscard]] int getMinValue() const noexcept;
        void setMinValue(int value);
        [[nodiscard]] int getMaxValue() const noexcept;
        void setMaxValue(int value);
        /** Normalizes reversed endpoints and enables range clamping. */
        void setValueRange(int minValue, int maxValue);
        [[nodiscard]] int getStep() const noexcept;
        /** Ignores non-positive steps. */
        void setStep(int step);

        [[nodiscard]] const UIIntTextBoxCallback &
        getChangedCallback() const noexcept;
        void setChangedCallback(UIIntTextBoxCallback callback);
        [[nodiscard]] const UIIntTextBoxCallback &
        getSubmittedCallback() const noexcept;
        void setSubmittedCallback(UIIntTextBoxCallback callback);
        [[nodiscard]] const UIIntTextBoxCallback &
        getCanceledCallback() const noexcept;
        void setCanceledCallback(UIIntTextBoxCallback callback);

      private:
        void notifyValueChanged() override;
        void notifySubmitted() override;
        void notifyCanceled() override;

        UIIntTextBoxCallback m_changedCallback;
        UIIntTextBoxCallback m_submittedCallback;
        UIIntTextBoxCallback m_canceledCallback;
    };
} // namespace Bess::Canvas::UI
