#pragma once

#include "models/text_edit_model.h"
#include "models/value_models.h"
#include "ui_style.h"
#include "widget.h"

#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace Bess::UI {

    enum class NumericInputKind : uint8_t { integer, floatingPoint };

    struct NumericInputOptions {
        std::optional<UITextInputStyle> style;
        std::string placeholder;
        bool readOnly = false;
        bool selectAllOnFocus = true;
        bool autoSize = true;
        // When false, out-of-range commits are allowed and only sanitized for
        // finiteness. When true (or when a min/max is set), values clamp.
        bool clampToRange = false;
        std::optional<double> minimum;
        std::optional<double> maximum;
        // Arrow Up/Down step. Shift multiplies by 10. Zero disables stepping.
        double step = 1.0;
        // Display digits after the decimal for floatingPoint; ignored for
        // integer mode. Clamped to [0, 9] at construction and setters.
        int precision = 2;
        size_t maximumBytes = 64;
    };

    // Single-line numeric field with partial-edit filtering, commit-on-blur /
    // Enter, optional range clamping, and keyboard stepping. Integer mode
    // rejects the decimal point; floating mode accepts scientific notation
    // during edit and formats with the configured precision on commit.
    class BESS_API NumericInput final : public Widget {
      public:
        using Changed = std::function<void(double)>;
        using Submitted = std::function<void(double)>;

        NumericInput(NumericInputKind kind = NumericInputKind::floatingPoint,
                     std::shared_ptr<NumericModel> model = {},
                     Changed changed = {},
                     Submitted submitted = {},
                     NumericInputOptions options = {});

        [[nodiscard]] std::string_view typeName() const noexcept override;
        [[nodiscard]] WidgetTraits traits() const noexcept override;
        void onMount(WidgetMountContext &context) override;
        void onUnmount(WidgetTree &state, WidgetId id) override;
        void updateLayout(WidgetLayoutContext &context) override;
        void update(WidgetUpdateContext &context) override;
        void paint(WidgetPaintContext &context) const override;
        [[nodiscard]] CursorIcon
        cursor(const WidgetCursorContext &context) const noexcept override;
        UIEventReply onEvent(WidgetEventContext &context,
                             const UIEvent &event) override;

        [[nodiscard]] NumericInputKind kind() const noexcept;
        [[nodiscard]] std::shared_ptr<NumericModel> model() const noexcept;
        [[nodiscard]] double value() const noexcept;
        bool setValue(double value);
        [[nodiscard]] const std::string &text() const noexcept;
        [[nodiscard]] bool readOnly() const noexcept;
        void setReadOnly(bool readOnly) noexcept;
        void setChanged(Changed changed);
        void setSubmitted(Submitted submitted);
        void setRange(std::optional<double> minimum,
                      std::optional<double> maximum,
                      bool clamp = true);
        void setStep(double step) noexcept;
        void setPrecision(int precision);

      private:
        void reconnectModel();
        void resetCaretBlink() noexcept;
        [[nodiscard]] size_t byteOffsetAt(float x) const noexcept;
        void notifyChangedIfNeeded(double previous);
        void syncTextFromValue(bool selectAll = false);
        void commitText();
        void restoreValidText();
        bool applyEditFilter(std::string_view candidate);
        bool changeByStep(double direction, bool coarse);
        [[nodiscard]] std::string formatValue(double value) const;
        [[nodiscard]] bool parseValue(std::string_view text,
                                      double &out) const noexcept;
        [[nodiscard]] bool isValidPartialEdit(std::string_view text) const noexcept;
        [[nodiscard]] double sanitize(double value) const noexcept;
        [[nodiscard]] double normalize(double value) const noexcept;
        [[nodiscard]] double effectiveStep() const noexcept;
        [[nodiscard]] int resolvedPrecision() const noexcept;

        NumericInputKind m_kind = NumericInputKind::floatingPoint;
        std::shared_ptr<NumericModel> m_model;
        std::shared_ptr<TextEditModel> m_textModel;
        Changed m_changed;
        Submitted m_submitted;
        NumericInputOptions m_options;
        NumericModel::ChangedSignal::Connection m_connection;
        TextEditModel::ChangedSignal::Connection m_textConnection;
        WidgetTree *m_state = nullptr;
        WidgetId m_id;
        bool m_pointerSelecting = false;
        bool m_focused = false;
        bool m_editing = false;
        mutable float m_scrollX = 0.f;
        mutable float m_contentLeft = 0.f;
        mutable std::vector<std::pair<size_t, float>> m_caretPositions;
        mutable std::string m_caretMetricsText;
        mutable float m_caretMetricsFontSize = 0.f;
        mutable float m_caretMetricsLetterSpacing = 0.f;
        mutable bool m_caretMetricsValid = false;
        double m_blinkElapsedMs = 0.0;
        bool m_caretVisible = true;
    };

} // namespace Bess::UI
