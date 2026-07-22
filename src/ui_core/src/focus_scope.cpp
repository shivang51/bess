#include "controls/focus_scope.h"

#include "widget_tree.h"

#include <utility>

namespace Bess::UI {

    FocusScope::FocusScope(FocusScopeOptions options)
        : FlexContainer(options.container),
          m_options(std::move(options)) {
    }

    std::string_view FocusScope::typeName() const noexcept {
        return "FocusScope";
    }

    void FocusScope::onMount(WidgetMountContext &context) {
        FlexContainer::onMount(context);
        m_state = &context.state;
        m_id = context.id;
        static_cast<void>(m_state->activateFocusScope(m_id, m_options.focus));
    }

    void FocusScope::onUnmount(WidgetTree &state, WidgetId id) {
        static_cast<void>(state.deactivateFocusScope(id));
        m_state = nullptr;
        m_id = {};
    }

    bool FocusScope::setDefaultFocus(WidgetId widget) {
        return m_state != nullptr && m_id &&
               m_state->setDefaultFocus(m_id, widget);
    }

    bool FocusScope::focusDefault() {
        return m_state != nullptr && m_id && m_state->focusDefault(m_id);
    }

} // namespace Bess::UI
