#include "models/action_registry.h"

#include "common/types.h"

#include <algorithm>
#include <array>
#include <exception>
#include <limits>
#include <utility>

namespace Bess::UI {
    namespace {
        [[nodiscard]] constexpr KeyChordModifier
        normalizedModifiers(const Input::Modifiers &modifiers) noexcept {
            KeyChordModifier result = KeyChordModifier::none;
            if (modifiers.control) {
                result |= KeyChordModifier::control;
            }
            if (modifiers.shift) {
                result |= KeyChordModifier::shift;
            }
            if (modifiers.alt) {
                result |= KeyChordModifier::alt;
            }
            if (modifiers.super) {
                result |= KeyChordModifier::super;
            }
            return result;
        }

        [[nodiscard]] constexpr bool isModifierKey(KeyCode key) noexcept {
            switch (key) {
            case KeyCode::leftShift:
            case KeyCode::leftControl:
            case KeyCode::leftAlt:
            case KeyCode::leftSuper:
            case KeyCode::rightShift:
            case KeyCode::rightControl:
            case KeyCode::rightAlt:
            case KeyCode::rightSuper:
                return true;
            default:
                return false;
            }
        }

        [[nodiscard]] std::string_view keyName(KeyCode key) noexcept {
            if (key >= KeyCode::a && key <= KeyCode::z) {
                static constexpr std::array names{
                    std::string_view{"A"}, std::string_view{"B"},
                    std::string_view{"C"}, std::string_view{"D"},
                    std::string_view{"E"}, std::string_view{"F"},
                    std::string_view{"G"}, std::string_view{"H"},
                    std::string_view{"I"}, std::string_view{"J"},
                    std::string_view{"K"}, std::string_view{"L"},
                    std::string_view{"M"}, std::string_view{"N"},
                    std::string_view{"O"}, std::string_view{"P"},
                    std::string_view{"Q"}, std::string_view{"R"},
                    std::string_view{"S"}, std::string_view{"T"},
                    std::string_view{"U"}, std::string_view{"V"},
                    std::string_view{"W"}, std::string_view{"X"},
                    std::string_view{"Y"}, std::string_view{"Z"},
                };
                return names[static_cast<size_t>(key) -
                             static_cast<size_t>(KeyCode::a)];
            }
            if (key >= KeyCode::d0 && key <= KeyCode::d9) {
                static constexpr std::array names{
                    std::string_view{"0"},
                    std::string_view{"1"},
                    std::string_view{"2"},
                    std::string_view{"3"},
                    std::string_view{"4"},
                    std::string_view{"5"},
                    std::string_view{"6"},
                    std::string_view{"7"},
                    std::string_view{"8"},
                    std::string_view{"9"},
                };
                return names[static_cast<size_t>(key) -
                             static_cast<size_t>(KeyCode::d0)];
            }

            switch (key) {
            case KeyCode::space:
                return "Space";
            case KeyCode::apostrophe:
                return "'";
            case KeyCode::comma:
                return ",";
            case KeyCode::minus:
                return "-";
            case KeyCode::period:
                return ".";
            case KeyCode::slash:
                return "/";
            case KeyCode::semicolon:
                return ";";
            case KeyCode::equal:
                return "=";
            case KeyCode::questionMark:
                return "?";
            case KeyCode::leftBracket:
                return "[";
            case KeyCode::backslash:
                return "\\";
            case KeyCode::rightBracket:
                return "]";
            case KeyCode::graveAccent:
                return "`";
            case KeyCode::escape:
                return "Esc";
            case KeyCode::enter:
                return "Enter";
            case KeyCode::tab:
                return "Tab";
            case KeyCode::backspace:
                return "Backspace";
            case KeyCode::insert:
                return "Insert";
            case KeyCode::del:
                return "Delete";
            case KeyCode::arrowRight:
                return "Right";
            case KeyCode::arrowLeft:
                return "Left";
            case KeyCode::arrowDown:
                return "Down";
            case KeyCode::arrowUp:
                return "Up";
            case KeyCode::pageUp:
                return "Page Up";
            case KeyCode::pageDown:
                return "Page Down";
            case KeyCode::home:
                return "Home";
            case KeyCode::end:
                return "End";
            case KeyCode::capsLock:
                return "Caps Lock";
            case KeyCode::scrollLock:
                return "Scroll Lock";
            case KeyCode::numLock:
                return "Num Lock";
            case KeyCode::printScreen:
                return "Print Screen";
            case KeyCode::pause:
                return "Pause";
            case KeyCode::f1:
                return "F1";
            case KeyCode::f2:
                return "F2";
            case KeyCode::f3:
                return "F3";
            case KeyCode::f4:
                return "F4";
            case KeyCode::f5:
                return "F5";
            case KeyCode::f6:
                return "F6";
            case KeyCode::f7:
                return "F7";
            case KeyCode::f8:
                return "F8";
            case KeyCode::f9:
                return "F9";
            case KeyCode::f10:
                return "F10";
            case KeyCode::f11:
                return "F11";
            case KeyCode::f12:
                return "F12";
            case KeyCode::menu:
                return "Menu";
            default:
                return {};
            }
        }

        void appendChordPart(std::string &result, std::string_view part) {
            if (!result.empty()) {
                result.push_back('+');
            }
            result.append(part);
        }

        [[nodiscard]] constexpr uint8_t
        levelValue(ActionScopeLevel level) noexcept {
            return static_cast<uint8_t>(level);
        }
    } // namespace

    ActionId::ActionId(std::string value) : m_value(std::move(value)) {
    }

    ActionId::ActionId(std::string_view value) : m_value(value) {
    }

    ActionId::ActionId(const char *value)
        : m_value(value != nullptr ? value : "") {
    }

    bool ActionId::isValid() const noexcept {
        return !m_value.empty();
    }

    ActionId::operator bool() const noexcept {
        return isValid();
    }

    const std::string &ActionId::value() const noexcept {
        return m_value;
    }

    bool KeyChord::isValid() const noexcept {
        constexpr auto knownModifiers =
            static_cast<uint8_t>(KeyChordModifier::control) |
            static_cast<uint8_t>(KeyChordModifier::shift) |
            static_cast<uint8_t>(KeyChordModifier::alt) |
            static_cast<uint8_t>(KeyChordModifier::super);
        return key != KeyCode::unknown && !isModifierKey(key) &&
               !keyName(key).empty() &&
               (static_cast<uint8_t>(modifiers) & ~knownModifiers) == 0;
    }

    bool KeyChord::matches(
        KeyCode candidate,
        const Input::Modifiers &candidateModifiers) const noexcept {
        return isValid() && key == candidate &&
               modifiers == normalizedModifiers(candidateModifiers);
    }

    bool KeyChord::matches(
        const Input::KeyEvent &event,
        const Input::Modifiers &candidateModifiers) const noexcept {
        return matches(event.key, candidateModifiers);
    }

    std::string formatKeyChord(KeyChord chord) {
        if (!chord.isValid()) {
            return {};
        }
        std::string result;
        if (hasModifier(chord.modifiers, KeyChordModifier::control)) {
            appendChordPart(result, "Ctrl");
        }
        if (hasModifier(chord.modifiers, KeyChordModifier::shift)) {
            appendChordPart(result, "Shift");
        }
        if (hasModifier(chord.modifiers, KeyChordModifier::alt)) {
            appendChordPart(result, "Alt");
        }
        if (hasModifier(chord.modifiers, KeyChordModifier::super)) {
            appendChordPart(result, "Super");
        }
        appendChordPart(result, keyName(chord.key));
        return result;
    }

    struct ActionRegistry::Impl {
        struct ScopeRecord {
            ActionScopeSnapshot snapshot;
            uint64_t activationSequence = 0;
            HashMap<KeyChord, ActionId> shortcuts;
        };

        ActionScopeId globalScope;
        uint64_t nextActivationSequence = 1;
        NodeHashMap<ActionScopeId, ScopeRecord> scopes;
        NodeHashMap<ActionId, ActionDefinition> actions;
        ChangedSignal changed;

        [[nodiscard]] ScopeRecord *scope(ActionScopeId id) noexcept {
            const auto it = scopes.find(id);
            return it != scopes.end() ? &it->second : nullptr;
        }

        [[nodiscard]] const ScopeRecord *
        scope(ActionScopeId id) const noexcept {
            const auto it = scopes.find(id);
            return it != scopes.end() ? &it->second : nullptr;
        }

        [[nodiscard]] ActionDefinition *action(const ActionId &id) noexcept {
            const auto it = actions.find(id);
            return it != actions.end() ? &it->second : nullptr;
        }

        [[nodiscard]] const ActionDefinition *
        action(const ActionId &id) const noexcept {
            const auto it = actions.find(id);
            return it != actions.end() ? &it->second : nullptr;
        }

        [[nodiscard]] ActionRegistrationResult
        validateShortcuts(const std::vector<KeyChord> &shortcuts,
                          ActionScopeId scopeId,
                          const ActionId &self = {}) const {
            const auto *scopeRecord = scope(scopeId);
            if (scopeRecord == nullptr) {
                return {.status = ActionRegistrationStatus::unknownScope};
            }
            HashSet<KeyChord> unique;
            unique.reserve(shortcuts.size());
            for (const auto chord : shortcuts) {
                if (!chord.isValid()) {
                    return {.status =
                                ActionRegistrationStatus::invalidShortcut};
                }
                if (!unique.insert(chord).second) {
                    return {.status =
                                ActionRegistrationStatus::duplicateShortcut};
                }
                const auto conflict = scopeRecord->shortcuts.find(chord);
                if (conflict != scopeRecord->shortcuts.end() &&
                    conflict->second != self) {
                    return {
                        .status = ActionRegistrationStatus::shortcutConflict,
                        .conflictingAction = conflict->second,
                    };
                }
            }
            return {};
        }

        void indexShortcuts(const ActionDefinition &definition) {
            auto *scopeRecord = scope(definition.scope);
            if (scopeRecord == nullptr) {
                return;
            }
            for (const auto chord : definition.shortcuts) {
                scopeRecord->shortcuts.insert_or_assign(chord, definition.id);
            }
        }

        void unindexShortcuts(const ActionDefinition &definition) noexcept {
            auto *scopeRecord = scope(definition.scope);
            if (scopeRecord == nullptr) {
                return;
            }
            for (const auto chord : definition.shortcuts) {
                const auto found = scopeRecord->shortcuts.find(chord);
                if (found != scopeRecord->shortcuts.end() &&
                    found->second == definition.id) {
                    scopeRecord->shortcuts.erase(found);
                }
            }
        }
    };

    ActionRegistry::ActionRegistry() : m_impl(std::make_unique<Impl>()) {
        do {
            m_impl->globalScope = ActionScopeId::generate();
        } while (!m_impl->globalScope);
        m_impl->scopes.emplace(
            m_impl->globalScope,
            Impl::ScopeRecord{
                .snapshot = {.id = m_impl->globalScope,
                             .name = "Global",
                             .level = ActionScopeLevel::global,
                             .active = true},
                .activationSequence = 0,
            });
    }

    ActionRegistry::~ActionRegistry() = default;

    ActionScopeId ActionRegistry::globalScope() const noexcept {
        return m_impl->globalScope;
    }

    ActionScopeId
    ActionRegistry::createScope(ActionScopeDefinition definition) {
        ActionScopeId id;
        do {
            id = ActionScopeId::generate();
        } while (!id || m_impl->scopes.contains(id));
        const bool active = definition.active;
        m_impl->scopes.emplace(
            id,
            Impl::ScopeRecord{
                .snapshot = {.id = id,
                             .name = std::move(definition.name),
                             .level = definition.level,
                             .active = active},
                .activationSequence =
                    active ? m_impl->nextActivationSequence++ : 0,
            });
        m_impl->changed.emit(
            {.kind = ActionChangeKind::scopeChanged, .scope = id});
        return id;
    }

    bool ActionRegistry::removeScope(ActionScopeId scopeId) {
        if (!scopeId || scopeId == m_impl->globalScope ||
            !m_impl->scopes.contains(scopeId)) {
            return false;
        }

        std::vector<ActionId> removed;
        for (const auto &[id, definition] : m_impl->actions) {
            if (definition.scope == scopeId) {
                removed.push_back(id);
            }
        }

        // Make the scope unavailable before callbacks observe action removal.
        // Signal callbacks may register or move actions; leaving the scope in
        // the map until afterward would allow them to create an orphan whose
        // shortcut index is destroyed at the end of this operation.
        m_impl->scopes.erase(scopeId);
        std::exception_ptr firstObserverFailure;
        const auto preserveObserverFailure = [&firstObserverFailure]() {
            if (firstObserverFailure == nullptr) {
                firstObserverFailure = std::current_exception();
            }
        };
        for (const auto &id : removed) {
            // Removal callbacks may rescue another action by moving it to a
            // surviving scope, or unregister/re-register the same semantic
            // ID. Only erase the scoped action that still belongs to this
            // removal transaction.
            const auto *definition = m_impl->action(id);
            if (definition != nullptr && definition->scope == scopeId) {
                try {
                    static_cast<void>(unregisterAction(id));
                } catch (...) {
                    preserveObserverFailure();
                }
            }
        }
        try {
            m_impl->changed.emit(
                {.kind = ActionChangeKind::scopeChanged, .scope = scopeId});
        } catch (...) {
            preserveObserverFailure();
        }
        if (firstObserverFailure != nullptr) {
            std::rethrow_exception(firstObserverFailure);
        }
        return true;
    }

    bool ActionRegistry::activateScope(ActionScopeId scopeId) {
        auto *scope = m_impl->scope(scopeId);
        if (scope == nullptr) {
            return false;
        }
        scope->snapshot.active = true;
        scope->activationSequence = m_impl->nextActivationSequence++;
        m_impl->changed.emit(
            {.kind = ActionChangeKind::scopeChanged, .scope = scopeId});
        return true;
    }

    bool ActionRegistry::deactivateScope(ActionScopeId scopeId) {
        if (scopeId == m_impl->globalScope) {
            return false;
        }
        auto *scope = m_impl->scope(scopeId);
        if (scope == nullptr) {
            return false;
        }
        if (!scope->snapshot.active) {
            return true;
        }
        scope->snapshot.active = false;
        m_impl->changed.emit(
            {.kind = ActionChangeKind::scopeChanged, .scope = scopeId});
        return true;
    }

    bool ActionRegistry::isScopeActive(ActionScopeId scopeId) const noexcept {
        const auto *scope = m_impl->scope(scopeId);
        return scope != nullptr && scope->snapshot.active;
    }

    ActionScopeSnapshot
    ActionRegistry::scopeSnapshot(ActionScopeId scopeId) const {
        const auto *scope = m_impl->scope(scopeId);
        return scope != nullptr ? scope->snapshot : ActionScopeSnapshot{};
    }

    ActionRegistrationResult
    ActionRegistry::registerAction(ActionDefinition definition) {
        if (!definition.id) {
            return {.status = ActionRegistrationStatus::invalidActionId};
        }
        if (m_impl->actions.contains(definition.id)) {
            return {.status = ActionRegistrationStatus::duplicateAction,
                    .conflictingAction = definition.id};
        }
        if (!definition.scope) {
            definition.scope = m_impl->globalScope;
        }
        const auto validation =
            m_impl->validateShortcuts(definition.shortcuts, definition.scope);
        if (!validation) {
            return validation;
        }
        if (!definition.state.checkable) {
            definition.state.checked = false;
        }

        const ActionId id = definition.id;
        const ActionScopeId scopeId = definition.scope;
        auto [it, inserted] =
            m_impl->actions.emplace(id, std::move(definition));
        if (!inserted) {
            return {.status = ActionRegistrationStatus::duplicateAction,
                    .conflictingAction = id};
        }
        m_impl->indexShortcuts(it->second);
        m_impl->changed.emit({.kind = ActionChangeKind::registered,
                              .action = id,
                              .scope = scopeId});
        return {};
    }

    bool ActionRegistry::unregisterAction(const ActionId &actionId) {
        const auto it = m_impl->actions.find(actionId);
        if (it == m_impl->actions.end()) {
            return false;
        }
        const ActionId id = it->first;
        const ActionScopeId scopeId = it->second.scope;
        m_impl->unindexShortcuts(it->second);
        m_impl->actions.erase(it);
        m_impl->changed.emit({.kind = ActionChangeKind::unregistered,
                              .action = id,
                              .scope = scopeId});
        return true;
    }

    const ActionDefinition *
    ActionRegistry::find(const ActionId &actionId) const noexcept {
        return m_impl->action(actionId);
    }

    bool ActionRegistry::contains(const ActionId &actionId) const noexcept {
        return m_impl->actions.contains(actionId);
    }

    bool ActionRegistry::isAvailable(const ActionId &actionId) const noexcept {
        const auto *definition = m_impl->action(actionId);
        if (definition == nullptr || !definition->state.enabled ||
            !definition->state.visible || !definition->invoked) {
            return false;
        }
        const auto *scope = m_impl->scope(definition->scope);
        return scope != nullptr && scope->snapshot.active;
    }

    size_t ActionRegistry::size() const noexcept {
        return m_impl->actions.size();
    }

    bool ActionRegistry::setState(const ActionId &actionId, ActionState state) {
        auto *definition = m_impl->action(actionId);
        if (definition == nullptr) {
            return false;
        }
        if (!state.checkable) {
            state.checked = false;
        }
        if (definition->state == state) {
            return true;
        }
        definition->state = std::move(state);
        m_impl->changed.emit({.kind = ActionChangeKind::stateChanged,
                              .action = actionId,
                              .scope = definition->scope});
        return true;
    }

    ActionRegistrationResult
    ActionRegistry::setShortcuts(const ActionId &actionId,
                                 std::vector<KeyChord> shortcuts) {
        auto *definition = m_impl->action(actionId);
        if (definition == nullptr) {
            return {.status = ActionRegistrationStatus::invalidActionId};
        }
        const auto validation = m_impl->validateShortcuts(
            shortcuts, definition->scope, definition->id);
        if (!validation) {
            return validation;
        }
        if (definition->shortcuts == shortcuts) {
            return {};
        }

        m_impl->unindexShortcuts(*definition);
        definition->shortcuts = std::move(shortcuts);
        m_impl->indexShortcuts(*definition);
        m_impl->changed.emit({.kind = ActionChangeKind::shortcutsChanged,
                              .action = actionId,
                              .scope = definition->scope});
        return {};
    }

    bool ActionRegistry::setHandler(const ActionId &actionId,
                                    ActionHandler handler) {
        auto *definition = m_impl->action(actionId);
        if (definition == nullptr) {
            return false;
        }
        definition->invoked = std::move(handler);
        m_impl->changed.emit({.kind = ActionChangeKind::handlerChanged,
                              .action = actionId,
                              .scope = definition->scope});
        return true;
    }

    ActionRegistrationResult
    ActionRegistry::moveToScope(const ActionId &actionId,
                                ActionScopeId scopeId) {
        auto *definition = m_impl->action(actionId);
        if (definition == nullptr) {
            return {.status = ActionRegistrationStatus::invalidActionId};
        }
        if (!m_impl->scopes.contains(scopeId)) {
            return {.status = ActionRegistrationStatus::unknownScope};
        }
        if (definition->scope == scopeId) {
            return {};
        }
        const auto validation = m_impl->validateShortcuts(
            definition->shortcuts, scopeId, definition->id);
        if (!validation) {
            return validation;
        }

        const ActionScopeId previous = definition->scope;
        m_impl->unindexShortcuts(*definition);
        definition->scope = scopeId;
        m_impl->indexShortcuts(*definition);
        m_impl->changed.emit({.kind = ActionChangeKind::scopeChanged,
                              .action = actionId,
                              .scope = scopeId});
        (void)previous;
        return {};
    }

    ActionDispatchResult ActionRegistry::invoke(const ActionId &actionId,
                                                ActionInvokeOptions options) {
        const auto *definition = m_impl->action(actionId);
        if (definition == nullptr) {
            return {};
        }

        const ActionId stableId = definition->id;
        const ActionScopeId stableScope = definition->scope;
        if (!isAvailable(stableId)) {
            return {.status = ActionDispatchStatus::blocked,
                    .action = stableId,
                    .scope = stableScope};
        }

        // Copy before invocation: callbacks may unregister themselves, remove
        // their scope, or mutate the registry.
        auto handler = definition->invoked;
        handler({.action = stableId,
                 .scope = stableScope,
                 .source = options.source,
                 .sourceWidget = options.sourceWidget,
                 .modifiers = options.modifiers});
        return {.status = ActionDispatchStatus::invoked,
                .action = stableId,
                .scope = stableScope};
    }

    ActionDispatchResult
    ActionRegistry::dispatchShortcut(const Input::KeyEvent &event,
                                     const Input::Modifiers &modifiers,
                                     WidgetId sourceWidget) {
        if (event.action != KeyAction::press &&
            event.action != KeyAction::hold) {
            return {};
        }
        const KeyChord chord{.key = event.key,
                             .modifiers = normalizedModifiers(modifiers)};
        if (!chord.isValid()) {
            return {};
        }

        const Impl::ScopeRecord *winner = nullptr;
        const ActionId *winnerAction = nullptr;
        for (const auto &[scopeId, scope] : m_impl->scopes) {
            (void)scopeId;
            if (!scope.snapshot.active) {
                continue;
            }
            const auto binding = scope.shortcuts.find(chord);
            if (binding == scope.shortcuts.end()) {
                continue;
            }
            if (winner == nullptr ||
                levelValue(scope.snapshot.level) >
                    levelValue(winner->snapshot.level) ||
                (scope.snapshot.level == winner->snapshot.level &&
                 scope.activationSequence > winner->activationSequence)) {
                winner = &scope;
                winnerAction = &binding->second;
            }
        }
        if (winner == nullptr || winnerAction == nullptr) {
            return {};
        }

        const auto *definition = m_impl->action(*winnerAction);
        if (definition == nullptr) {
            return {};
        }
        if (event.action == KeyAction::hold &&
            definition->repeatPolicy == ActionRepeatPolicy::initialPressOnly) {
            return {.status = ActionDispatchStatus::blocked,
                    .action = definition->id,
                    .scope = definition->scope};
        }
        return invoke(definition->id,
                      {.source = ActionInvocationSource::shortcut,
                       .sourceWidget = sourceWidget,
                       .modifiers = modifiers});
    }

    ActionRegistry::ChangedSignal &ActionRegistry::changed() noexcept {
        return m_impl->changed;
    }

} // namespace Bess::UI
