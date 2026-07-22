#pragma once

#include "behaviors/pressable.h"
#include "models/value_models.h"
#include "ui_style.h"
#include "widget.h"

#include <functional>
#include <memory>
#include <optional>
#include <string>

namespace Bess::UI {

    namespace Detail {
        struct ControlTextMeasurement {
            glm::vec2 size{0.f};
            float fontSize = 0.f;
            float letterSpacing = 0.f;
            bool valid = false;
        };
    } // namespace Detail

    struct CheckBoxOptions {
        std::optional<UISelectionControlStyle> style;
        bool cycleMixed = false;
        bool autoSize = true;
    };

    class BESS_API CheckBox : public Widget {
      public:
        using Changed = std::function<void(CheckState)>;

        CheckBox(std::string label,
                 std::shared_ptr<CheckStateModel> model = {},
                 Changed changed = {},
                 CheckBoxOptions options = {});

        [[nodiscard]] std::string_view typeName() const noexcept override;
        [[nodiscard]] WidgetTraits traits() const noexcept override;
        void onMount(WidgetMountContext &context) override;
        void onUnmount(WidgetTree &state, WidgetId id) override;
        void updateLayout(WidgetLayoutContext &context) override;
        void paint(WidgetPaintContext &context) const override;
        [[nodiscard]] bool hitTest(WidgetBounds bounds,
                                   glm::vec2 position) const noexcept override;
        [[nodiscard]] CursorIcon
        cursor(const WidgetCursorContext &context) const noexcept override;
        UIEventReply onEvent(WidgetEventContext &context,
                             const UIEvent &event) override;

        [[nodiscard]] CheckState value() const noexcept;
        bool setValue(CheckState value);
        [[nodiscard]] const std::string &label() const noexcept;
        void setLabel(std::string label);
        [[nodiscard]] std::shared_ptr<CheckStateModel> model() const noexcept;

      private:
        void reconnectModel();
        [[nodiscard]] WidgetBounds
        interactionBounds(WidgetBounds bounds) const noexcept;

        std::string m_label;
        std::shared_ptr<CheckStateModel> m_model;
        Changed m_changed;
        CheckBoxOptions m_options;
        CheckStateModel::ChangedSignal::Connection m_connection;
        Pressable m_pressable;
        WidgetTree *m_state = nullptr;
        WidgetId m_id;
        mutable Detail::ControlTextMeasurement m_labelMeasurement;
        mutable bool m_intrinsicSizeDirty = true;
    };

    struct ToggleSwitchOptions {
        std::optional<UIToggleStyle> style;
        bool autoSize = true;
    };

    class BESS_API ToggleSwitch : public Widget {
      public:
        using Changed = std::function<void(bool)>;

        ToggleSwitch(std::string label = {},
                     std::shared_ptr<BoolModel> model = {},
                     Changed changed = {},
                     ToggleSwitchOptions options = {});

        [[nodiscard]] std::string_view typeName() const noexcept override;
        [[nodiscard]] WidgetTraits traits() const noexcept override;
        void onMount(WidgetMountContext &context) override;
        void onUnmount(WidgetTree &state, WidgetId id) override;
        void updateLayout(WidgetLayoutContext &context) override;
        void paint(WidgetPaintContext &context) const override;
        [[nodiscard]] bool hitTest(WidgetBounds bounds,
                                   glm::vec2 position) const noexcept override;
        [[nodiscard]] CursorIcon
        cursor(const WidgetCursorContext &context) const noexcept override;
        UIEventReply onEvent(WidgetEventContext &context,
                             const UIEvent &event) override;

        [[nodiscard]] bool value() const noexcept;
        bool setValue(bool value);
        [[nodiscard]] const std::string &label() const noexcept;
        void setLabel(std::string label);
        [[nodiscard]] std::shared_ptr<BoolModel> model() const noexcept;

      private:
        void reconnectModel();
        [[nodiscard]] WidgetBounds
        interactionBounds(WidgetBounds bounds) const noexcept;

        std::string m_label;
        std::shared_ptr<BoolModel> m_model;
        Changed m_changed;
        ToggleSwitchOptions m_options;
        BoolModel::ChangedSignal::Connection m_connection;
        Pressable m_pressable;
        WidgetTree *m_state = nullptr;
        WidgetId m_id;
        mutable Detail::ControlTextMeasurement m_labelMeasurement;
        mutable bool m_intrinsicSizeDirty = true;
    };

    struct RadioButtonOptions {
        std::optional<UISelectionControlStyle> style;
        bool autoSize = true;
    };

    class BESS_API RadioButton : public Widget {
      public:
        using Selected = std::function<void(RadioId)>;

        RadioButton(std::string label,
                    std::shared_ptr<RadioGroupModel> group,
                    RadioId id = {},
                    Selected selected = {},
                    RadioButtonOptions options = {});

        [[nodiscard]] std::string_view typeName() const noexcept override;
        [[nodiscard]] WidgetTraits traits() const noexcept override;
        void onMount(WidgetMountContext &context) override;
        void onUnmount(WidgetTree &state, WidgetId id) override;
        void updateLayout(WidgetLayoutContext &context) override;
        void paint(WidgetPaintContext &context) const override;
        [[nodiscard]] bool hitTest(WidgetBounds bounds,
                                   glm::vec2 position) const noexcept override;
        [[nodiscard]] CursorIcon
        cursor(const WidgetCursorContext &context) const noexcept override;
        UIEventReply onEvent(WidgetEventContext &context,
                             const UIEvent &event) override;

        [[nodiscard]] RadioId radioId() const noexcept;
        [[nodiscard]] bool selected() const noexcept;
        bool select();
        [[nodiscard]] const std::string &label() const noexcept;
        void setLabel(std::string label);
        [[nodiscard]] std::shared_ptr<RadioGroupModel> group() const noexcept;

      private:
        void reconnectGroup();
        [[nodiscard]] WidgetBounds
        interactionBounds(WidgetBounds bounds) const noexcept;

        std::string m_label;
        std::shared_ptr<RadioGroupModel> m_group;
        RadioId m_radioId;
        Selected m_selected;
        RadioButtonOptions m_options;
        RadioGroupModel::ChangedSignal::Connection m_connection;
        Pressable m_pressable;
        WidgetTree *m_state = nullptr;
        WidgetId m_id;
        mutable Detail::ControlTextMeasurement m_labelMeasurement;
        mutable bool m_intrinsicSizeDirty = true;
    };

    enum class SliderOrientation : uint8_t { horizontal, vertical };

    struct SliderOptions {
        std::optional<UISliderStyle> style;
        SliderOrientation orientation = SliderOrientation::horizontal;
        bool autoSize = true;
    };

    class BESS_API Slider : public Widget {
      public:
        using Changed = std::function<void(double)>;

        Slider(std::shared_ptr<RangeModel> model = {},
               Changed changed = {},
               SliderOptions options = {});

        [[nodiscard]] std::string_view typeName() const noexcept override;
        [[nodiscard]] WidgetTraits traits() const noexcept override;
        void onMount(WidgetMountContext &context) override;
        void onUnmount(WidgetTree &state, WidgetId id) override;
        void updateLayout(WidgetLayoutContext &context) override;
        void paint(WidgetPaintContext &context) const override;
        [[nodiscard]] CursorIcon
        cursor(const WidgetCursorContext &context) const noexcept override;
        UIEventReply onEvent(WidgetEventContext &context,
                             const UIEvent &event) override;

        [[nodiscard]] double value() const noexcept;
        bool setValue(double value);
        [[nodiscard]] std::shared_ptr<RangeModel> model() const noexcept;

      private:
        void reconnectModel();
        bool setFromPointer(WidgetBounds bounds, glm::vec2 pointer);
        bool changeBy(double delta);

        std::shared_ptr<RangeModel> m_model;
        Changed m_changed;
        SliderOptions m_options;
        RangeModel::ChangedSignal::Connection m_connection;
        Pressable m_pressable;
        WidgetTree *m_state = nullptr;
        WidgetId m_id;
    };

} // namespace Bess::UI
