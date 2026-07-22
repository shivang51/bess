#pragma once

#include "common/bess_api.h"
#include "controls/basic_widgets.h"
#include "models/action_registry.h"
#include "widget.h"

#include <memory>
#include <optional>
#include <string>

namespace Bess::UI {

    struct ActionButtonOptions {
        ButtonOptions button;
        // Presentation-only override. Enabled/visible state and activation
        // always remain owned by the action.
        std::optional<std::string> label;
    };

    // A layout-transparent action binding which owns an ordinary Button child.
    // Keeping the action state on the child means action visibility composes
    // with caller visibility on this wrapper instead of overwriting it.
    class BESS_API ActionButton : public Widget {
      public:
        ActionButton(std::shared_ptr<ActionRegistry> registry,
                     ActionId action,
                     ActionButtonOptions options = {});

        [[nodiscard]] std::string_view typeName() const noexcept override;
        [[nodiscard]] WidgetTraits traits() const noexcept override;
        void onMount(WidgetMountContext &context) override;
        void onUnmount(WidgetTree &state, WidgetId id) override;

        [[nodiscard]] const ActionId &action() const noexcept;
        [[nodiscard]] WidgetId buttonId() const noexcept;
        [[nodiscard]] bool isAvailable() const noexcept;

      private:
        void refresh();
        void invoke(const Input::Modifiers &modifiers = {});

        std::shared_ptr<ActionRegistry> m_registry;
        ActionId m_action;
        ActionButtonOptions m_options;
        ActionRegistry::ChangedSignal::Connection m_connection;
        WidgetTree *m_state = nullptr;
        WidgetId m_id;
        WidgetId m_button;
        bool m_available = false;
    };

} // namespace Bess::UI
