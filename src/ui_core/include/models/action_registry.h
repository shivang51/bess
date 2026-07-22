#pragma once

#include "bess_core/input/input_event.h"
#include "common/bess_api.h"
#include "models/signal.h"
#include "ui_types.h"

#include <concepts>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace Bess::UI {

    // Action IDs are semantic and process-independent. Prefer namespaced IDs
    // such as "project.save" or "scene.selection.delete" so menus, buttons,
    // command palettes, plugins, and persisted customizations can refer to the
    // same action without depending on a widget's lifetime.
    class BESS_API ActionId {
      public:
        ActionId() = default;
        explicit ActionId(std::string value);
        explicit ActionId(std::string_view value);
        explicit ActionId(const char *value);

        [[nodiscard]] bool isValid() const noexcept;
        [[nodiscard]] explicit operator bool() const noexcept;
        [[nodiscard]] const std::string &value() const noexcept;

        bool operator==(const ActionId &) const noexcept = default;

        template <typename H> friend H AbslHashValue(H h, const ActionId &id) {
            return H::combine(std::move(h), id.m_value);
        }

      private:
        std::string m_value;
    };

    struct ActionScopeIdTag;
    using ActionScopeId = StableId<ActionScopeIdTag>;

    enum class KeyChordModifier : uint8_t {
        none = 0,
        control = 1 << 0,
        shift = 1 << 1,
        alt = 1 << 2,
        super = 1 << 3,
    };

    [[nodiscard]] constexpr KeyChordModifier
    operator|(KeyChordModifier lhs, KeyChordModifier rhs) noexcept {
        return static_cast<KeyChordModifier>(static_cast<uint8_t>(lhs) |
                                             static_cast<uint8_t>(rhs));
    }

    constexpr KeyChordModifier &operator|=(KeyChordModifier &lhs,
                                           KeyChordModifier rhs) noexcept {
        lhs = lhs | rhs;
        return lhs;
    }

    [[nodiscard]] constexpr bool hasModifier(KeyChordModifier value,
                                             KeyChordModifier flag) noexcept {
        return (static_cast<uint8_t>(value) & static_cast<uint8_t>(flag)) != 0;
    }

    struct KeyChord {
        KeyCode key = KeyCode::unknown;
        KeyChordModifier modifiers = KeyChordModifier::none;

        [[nodiscard]] bool isValid() const noexcept;
        [[nodiscard]] bool
        matches(KeyCode candidate,
                const Input::Modifiers &candidateModifiers) const noexcept;
        [[nodiscard]] bool
        matches(const Input::KeyEvent &event,
                const Input::Modifiers &candidateModifiers) const noexcept;

        bool operator==(const KeyChord &) const noexcept = default;

        template <typename H>
        friend H AbslHashValue(H h, const KeyChord &chord) {
            return H::combine(std::move(h),
                              static_cast<uint16_t>(chord.key),
                              static_cast<uint8_t>(chord.modifiers));
        }
    };

    // Portable presentation used by the default menu implementation. A
    // platform shell may replace this with native glyphs (for example, macOS
    // Command symbols) without changing shortcut matching.
    [[nodiscard]] BESS_API std::string formatKeyChord(KeyChord chord);

    enum class ActionScopeLevel : uint8_t {
        global = 0,
        window = 10,
        panel = 20,
        editor = 30,
        popup = 40,
        modal = 50,
    };

    struct ActionScopeDefinition {
        std::string name;
        ActionScopeLevel level = ActionScopeLevel::panel;
        bool active = false;
    };

    struct ActionScopeSnapshot {
        ActionScopeId id;
        std::string name;
        ActionScopeLevel level = ActionScopeLevel::panel;
        bool active = false;
    };

    struct ActionState {
        std::string label;
        std::string description;
        std::string icon;
        bool enabled = true;
        bool visible = true;
        bool checkable = false;
        bool checked = false;

        bool operator==(const ActionState &) const noexcept = default;
    };

    enum class ActionRepeatPolicy : uint8_t {
        initialPressOnly,
        allowRepeat,
    };

    enum class ActionInvocationSource : uint8_t {
        programmatic,
        shortcut,
        button,
        menu,
        commandPalette,
    };

    struct ActionInvocation {
        ActionId action;
        ActionScopeId scope;
        ActionInvocationSource source = ActionInvocationSource::programmatic;
        WidgetId sourceWidget;
        Input::Modifiers modifiers;
    };

    using ActionHandler = std::function<void(const ActionInvocation &)>;

    struct ActionDefinition {
        ActionId id;
        // An empty scope is normalized to ActionRegistry::globalScope().
        ActionScopeId scope;
        ActionState state;
        std::vector<KeyChord> shortcuts;
        ActionRepeatPolicy repeatPolicy = ActionRepeatPolicy::initialPressOnly;
        ActionHandler invoked;
    };

    enum class ActionChangeKind : uint8_t {
        registered,
        unregistered,
        stateChanged,
        shortcutsChanged,
        handlerChanged,
        scopeChanged,
    };

    struct ActionChange {
        ActionChangeKind kind = ActionChangeKind::stateChanged;
        ActionId action;
        ActionScopeId scope;
    };

    enum class ActionRegistrationStatus : uint8_t {
        success,
        invalidActionId,
        duplicateAction,
        unknownScope,
        invalidShortcut,
        duplicateShortcut,
        shortcutConflict,
    };

    struct ActionRegistrationResult {
        ActionRegistrationStatus status = ActionRegistrationStatus::success;
        ActionId conflictingAction;

        [[nodiscard]] explicit operator bool() const noexcept {
            return status == ActionRegistrationStatus::success;
        }
    };

    enum class ActionDispatchStatus : uint8_t {
        unhandled,
        // The winning scope owns this chord, but its action cannot currently
        // run. Treating this as handled prevents an unrelated lower scope from
        // unexpectedly receiving the same shortcut.
        blocked,
        invoked,
    };

    struct ActionDispatchResult {
        ActionDispatchStatus status = ActionDispatchStatus::unhandled;
        ActionId action;
        ActionScopeId scope;

        [[nodiscard]] bool handled() const noexcept {
            return status != ActionDispatchStatus::unhandled;
        }

        [[nodiscard]] bool wasInvoked() const noexcept {
            return status == ActionDispatchStatus::invoked;
        }
    };

    struct ActionInvokeOptions {
        ActionInvocationSource source = ActionInvocationSource::programmatic;
        WidgetId sourceWidget;
        Input::Modifiers modifiers;
    };

    // Single-threaded UI action registry. State is cached and changed by
    // domain notifications rather than polled during every paint. Shortcut
    // lookup is O(number of active scopes), normally a small constant.
    // Scope activation is registry-wide; the registry has no UITarget/focus
    // context of its own. A host sharing one registry across targets must
    // coordinate contextual scope activation, or keep those registries local.
    class BESS_API ActionRegistry {
      public:
        using ChangedSignal = Signal<ActionChange>;

        ActionRegistry();
        ActionRegistry(const ActionRegistry &) = delete;
        ActionRegistry(ActionRegistry &&) = delete;
        ~ActionRegistry();
        ActionRegistry &operator=(const ActionRegistry &) = delete;
        ActionRegistry &operator=(ActionRegistry &&) = delete;

        [[nodiscard]] ActionScopeId globalScope() const noexcept;
        [[nodiscard]] ActionScopeId
        createScope(ActionScopeDefinition definition);
        // Observer failures are rethrown only after the scope and every action
        // still owned by it have been removed, so exceptions cannot leave
        // orphaned registry entries behind.
        bool removeScope(ActionScopeId scope);
        // Activating an already-active scope promotes it above peers at the
        // same level. Call this on a real focus/ownership transition, not once
        // per frame.
        bool activateScope(ActionScopeId scope);
        bool deactivateScope(ActionScopeId scope);
        [[nodiscard]] bool isScopeActive(ActionScopeId scope) const noexcept;
        [[nodiscard]] ActionScopeSnapshot
        scopeSnapshot(ActionScopeId scope) const;

        [[nodiscard]] ActionRegistrationResult
        registerAction(ActionDefinition definition);
        bool unregisterAction(const ActionId &action);

        [[nodiscard]] const ActionDefinition *
        find(const ActionId &action) const noexcept;
        [[nodiscard]] bool contains(const ActionId &action) const noexcept;
        // Availability combines cached state, handler presence, and scope
        // activation. Bound controls should use this rather than `enabled`
        // alone so stale controls cannot invoke an inactive context.
        [[nodiscard]] bool isAvailable(const ActionId &action) const noexcept;
        [[nodiscard]] size_t size() const noexcept;

        bool setState(const ActionId &action, ActionState state);

        template <typename Mutation>
            requires std::invocable<Mutation, ActionState &>
        bool updateState(const ActionId &action, Mutation &&mutation) {
            const auto *entry = find(action);
            if (entry == nullptr) {
                return false;
            }
            auto next = entry->state;
            std::invoke(std::forward<Mutation>(mutation), next);
            return setState(action, std::move(next));
        }

        [[nodiscard]] ActionRegistrationResult
        setShortcuts(const ActionId &action, std::vector<KeyChord> shortcuts);
        bool setHandler(const ActionId &action, ActionHandler handler);
        [[nodiscard]] ActionRegistrationResult
        moveToScope(const ActionId &action, ActionScopeId scope);

        [[nodiscard]] ActionDispatchResult
        invoke(const ActionId &action, ActionInvokeOptions options = {});
        [[nodiscard]] ActionDispatchResult
        dispatchShortcut(const Input::KeyEvent &event,
                         const Input::Modifiers &modifiers = {},
                         WidgetId sourceWidget = {});

        [[nodiscard]] ChangedSignal &changed() noexcept;

      private:
        struct Impl;
        std::unique_ptr<Impl> m_impl;
    };

} // namespace Bess::UI

namespace std {
    template <> struct hash<Bess::UI::ActionId> {
        size_t operator()(const Bess::UI::ActionId &id) const noexcept {
            return hash<string>{}(id.value());
        }
    };

    template <> struct hash<Bess::UI::KeyChord> {
        size_t operator()(const Bess::UI::KeyChord &chord) const noexcept {
            const size_t key =
                hash<uint16_t>{}(static_cast<uint16_t>(chord.key));
            const size_t modifiers =
                hash<uint8_t>{}(static_cast<uint8_t>(chord.modifiers));
            return key ^ (modifiers + 0x9e3779b9U + (key << 6U) + (key >> 2U));
        }
    };
} // namespace std
