#pragma once

#include "models/text_edit_model.h"
#include "ui_style.h"
#include "widget.h"

#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace Bess::UI {

    struct TextBoxOptions {
        std::optional<UITextInputStyle> style;
        std::string placeholder;
        bool readOnly = false;
        bool selectAllOnFocus = false;
        bool autoSize = true;
    };

    class BESS_API TextBox : public Widget {
      public:
        using Changed = std::function<void(const std::string &)>;
        using Submitted = std::function<void(const std::string &)>;

        TextBox(std::shared_ptr<TextEditModel> model = {},
                Changed changed = {},
                Submitted submitted = {},
                TextBoxOptions options = {});

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

        [[nodiscard]] std::shared_ptr<TextEditModel> model() const noexcept;
        [[nodiscard]] const std::string &text() const noexcept;
        bool setText(std::string_view text);
        [[nodiscard]] bool readOnly() const noexcept;
        void setReadOnly(bool readOnly) noexcept;
        void setChanged(Changed changed);
        void setSubmitted(Submitted submitted);

      private:
        void reconnectModel();
        void resetCaretBlink() noexcept;
        [[nodiscard]] size_t byteOffsetAt(float x) const noexcept;
        bool insertClipboardText(WidgetTree &tree);
        bool copySelection(WidgetTree &tree) const;
        bool cutSelection(WidgetTree &tree);
        void notifyChangedIfNeeded(std::string previousText);
        [[nodiscard]] std::string singleLine(std::string_view text) const;

        std::shared_ptr<TextEditModel> m_model;
        Changed m_changed;
        Submitted m_submitted;
        TextBoxOptions m_options;
        TextEditModel::ChangedSignal::Connection m_connection;
        WidgetTree *m_state = nullptr;
        WidgetId m_id;
        bool m_pointerSelecting = false;
        bool m_focused = false;
        mutable float m_scrollX = 0.f;
        mutable float m_contentLeft = 0.f;
        mutable std::vector<std::pair<size_t, float>> m_caretPositions;
        // Prefix measurement is exact but potentially quadratic in the text
        // length. Retain it across frames and rebuild only when the text or
        // the metric-affecting style changes.
        mutable std::string m_caretMetricsText;
        mutable float m_caretMetricsFontSize = 0.f;
        mutable float m_caretMetricsLetterSpacing = 0.f;
        mutable bool m_caretMetricsValid = false;
        double m_blinkElapsedMs = 0.0;
        bool m_caretVisible = true;
    };

} // namespace Bess::UI
