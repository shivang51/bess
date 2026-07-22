#include "controls/action_button.h"

#include "widget_tree.h"

#include <stdexcept>
#include <utility>

namespace Bess::UI {

    ActionButton::ActionButton(std::shared_ptr<ActionRegistry> registry,
                               ActionId action,
                               ActionButtonOptions options)
        : m_registry(std::move(registry)),
          m_action(std::move(action)),
          m_options(std::move(options)) {
        if (m_registry == nullptr) {
            throw std::invalid_argument("ActionButton requires a registry");
        }
        if (!m_action) {
            throw std::invalid_argument("ActionButton requires an action ID");
        }
    }

    std::string_view ActionButton::typeName() const noexcept {
        return "ActionButton";
    }

    WidgetTraits ActionButton::traits() const noexcept {
        return {.focusable = false, .hitTestVisible = false};
    }

    void ActionButton::onMount(WidgetMountContext &context) {
        m_state = &context.state;
        m_id = context.id;
        context.layout.setDirection(LayoutDirection::horizontal);
        context.layout.setCrossAxisAlignment(LayoutAlignment::center);

        auto button = std::make_unique<Button>(
            std::string{}, Button::Activated{}, m_options.button);
        button->setActivatedWithEvent(
            [this](const UIEvent &event) { invoke(event.modifiers); });
        m_button = context.state.addWidget(std::move(button), context.id);
        if (!m_button) {
            m_state = nullptr;
            m_id = {};
            throw std::runtime_error(
                "ActionButton failed to create its button");
        }

        m_connection =
            m_registry->changed().connect([this](const ActionChange &change) {
                if (!m_state || (change.action && change.action != m_action)) {
                    return;
                }
                if (!change.action &&
                    change.kind == ActionChangeKind::scopeChanged) {
                    const auto *definition = m_registry->find(m_action);
                    if (definition == nullptr ||
                        definition->scope != change.scope) {
                        return;
                    }
                }
                refresh();
            });
        refresh();
    }

    void ActionButton::onUnmount(WidgetTree &, WidgetId) {
        m_connection.disconnect();
        m_state = nullptr;
        m_id = {};
        m_button = {};
        m_available = false;
    }

    const ActionId &ActionButton::action() const noexcept {
        return m_action;
    }

    WidgetId ActionButton::buttonId() const noexcept {
        return m_button;
    }

    bool ActionButton::isAvailable() const noexcept {
        return m_available;
    }

    void ActionButton::refresh() {
        if (m_state == nullptr || !m_button || !m_state->contains(m_button)) {
            m_available = false;
            return;
        }

        const auto *definition = m_registry->find(m_action);
        const bool visible = definition != nullptr && definition->state.visible;
        m_available = visible && m_registry->isAvailable(m_action);

        static_cast<void>(m_state->setVisibility(
            m_button,
            visible ? WidgetVisibility::visible : WidgetVisibility::collapsed));
        static_cast<void>(m_state->setEnabled(m_button, m_available));
        if (definition != nullptr) {
            const auto label =
                m_options.label.value_or(definition->state.label);
            const auto *button = m_state->getWidget<Button>(m_button);
            if (button != nullptr && button->label() != label) {
                static_cast<void>(m_state->mutateWidget<Button>(
                    m_button,
                    WidgetInvalidation::layout | WidgetInvalidation::paint,
                    [&label](Button &item) { item.setLabel(label); }));
            }
        }
    }

    void ActionButton::invoke(const Input::Modifiers &modifiers) {
        if (!m_available || m_registry == nullptr) {
            return;
        }
        static_cast<void>(
            m_registry->invoke(m_action,
                               {.source = ActionInvocationSource::button,
                                .sourceWidget = m_button,
                                .modifiers = modifiers}));
    }

} // namespace Bess::UI
