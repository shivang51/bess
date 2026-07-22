#include "controls/action_button.h"
#include "models/action_registry.h"
#include "widget_tree.h"

#include <gtest/gtest.h>

#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
    using namespace Bess;
    using namespace Bess::UI;

    constexpr auto control = KeyChordModifier::control;
    constexpr auto shift = KeyChordModifier::shift;

    TEST(ActionKeyChordTests, MatchesExactNonLockModifiersAndFormats) {
        const KeyChord chord{.key = KeyCode::z, .modifiers = control | shift};
        EXPECT_TRUE(chord.isValid());
        EXPECT_TRUE(chord.matches(KeyCode::z,
                                  {.control = true,
                                   .shift = true,
                                   .capsLock = true,
                                   .numLock = true}));
        EXPECT_FALSE(chord.matches(KeyCode::z, {.control = true}));
        EXPECT_FALSE(
            chord.matches(KeyCode::x, {.control = true, .shift = true}));
        EXPECT_EQ(formatKeyChord(chord), "Ctrl+Shift+Z");
        EXPECT_FALSE(KeyChord{.key = KeyCode::leftControl}.isValid());
    }

    TEST(ActionRegistryTests, RejectsDuplicateIdsAndLocalShortcutConflicts) {
        ActionRegistry registry;
        const KeyChord save{.key = KeyCode::s, .modifiers = control};

        EXPECT_TRUE(registry.registerAction({
            .id = ActionId{"project.save"},
            .state = {.label = "Save"},
            .shortcuts = {save},
        }));

        const auto duplicate = registry.registerAction({
            .id = ActionId{"project.save"},
            .state = {.label = "Save again"},
        });
        EXPECT_EQ(duplicate.status, ActionRegistrationStatus::duplicateAction);

        const auto conflict = registry.registerAction({
            .id = ActionId{"document.save"},
            .state = {.label = "Save document"},
            .shortcuts = {save},
        });
        EXPECT_EQ(conflict.status, ActionRegistrationStatus::shortcutConflict);
        EXPECT_EQ(conflict.conflictingAction, ActionId{"project.save"});
        EXPECT_EQ(registry.size(), 1U);
    }

    TEST(ActionRegistryTests, HighestAndMostRecentlyActivatedScopeWins) {
        ActionRegistry registry;
        const auto panelA =
            registry.createScope({.name = "Panel A",
                                  .level = ActionScopeLevel::panel,
                                  .active = true});
        const auto panelB =
            registry.createScope({.name = "Panel B",
                                  .level = ActionScopeLevel::panel,
                                  .active = true});
        const auto popup =
            registry.createScope({.name = "Popup",
                                  .level = ActionScopeLevel::popup,
                                  .active = false});
        const KeyChord copy{.key = KeyCode::c, .modifiers = control};
        std::vector<std::string> invocations;

        ASSERT_TRUE(registry.registerAction({
            .id = ActionId{"panel-a.copy"},
            .scope = panelA,
            .state = {.label = "Copy A"},
            .shortcuts = {copy},
            .invoked =
                [&invocations](const ActionInvocation &) {
                    invocations.emplace_back("a");
                },
        }));
        ASSERT_TRUE(registry.registerAction({
            .id = ActionId{"panel-b.copy"},
            .scope = panelB,
            .state = {.label = "Copy B"},
            .shortcuts = {copy},
            .invoked =
                [&invocations](const ActionInvocation &) {
                    invocations.emplace_back("b");
                },
        }));
        ASSERT_TRUE(registry.registerAction({
            .id = ActionId{"popup.copy"},
            .scope = popup,
            .state = {.label = "Copy popup"},
            .shortcuts = {copy},
            .invoked =
                [&invocations](const ActionInvocation &) {
                    invocations.emplace_back("popup");
                },
        }));

        auto result = registry.dispatchShortcut(
            {.key = KeyCode::c, .action = KeyAction::press}, {.control = true});
        ASSERT_TRUE(result.wasInvoked());
        EXPECT_EQ(invocations.back(), "b");

        ASSERT_TRUE(registry.activateScope(panelA));
        result = registry.dispatchShortcut(
            {.key = KeyCode::c, .action = KeyAction::press}, {.control = true});
        ASSERT_TRUE(result.wasInvoked());
        EXPECT_EQ(invocations.back(), "a");

        ASSERT_TRUE(registry.activateScope(popup));
        result = registry.dispatchShortcut(
            {.key = KeyCode::c, .action = KeyAction::press}, {.control = true});
        ASSERT_TRUE(result.wasInvoked());
        EXPECT_EQ(invocations.back(), "popup");
    }

    TEST(ActionRegistryTests, DisabledWinningActionBlocksLowerScope) {
        ActionRegistry registry;
        const auto editor =
            registry.createScope({.name = "Editor",
                                  .level = ActionScopeLevel::editor,
                                  .active = true});
        const KeyChord remove{.key = KeyCode::del};
        int globalInvocations = 0;

        ASSERT_TRUE(registry.registerAction({
            .id = ActionId{"global.delete"},
            .state = {.label = "Delete"},
            .shortcuts = {remove},
            .invoked = [&globalInvocations](
                           const ActionInvocation &) { ++globalInvocations; },
        }));
        ASSERT_TRUE(registry.registerAction({
            .id = ActionId{"editor.delete"},
            .scope = editor,
            .state = {.label = "Delete selection", .enabled = false},
            .shortcuts = {remove},
        }));

        const auto result = registry.dispatchShortcut(
            {.key = KeyCode::del, .action = KeyAction::press});
        EXPECT_EQ(result.status, ActionDispatchStatus::blocked);
        EXPECT_EQ(result.action, ActionId{"editor.delete"});
        EXPECT_EQ(globalInvocations, 0);
    }

    TEST(ActionRegistryTests, InactiveScopeBlocksExplicitInvocation) {
        ActionRegistry registry;
        const auto panel = registry.createScope(
            {.name = "Panel", .level = ActionScopeLevel::panel});
        const ActionId action{"panel.refresh"};
        int invocations = 0;
        ASSERT_TRUE(registry.registerAction({
            .id = action,
            .scope = panel,
            .state = {.label = "Refresh"},
            .invoked =
                [&invocations](const ActionInvocation &) { ++invocations; },
        }));

        EXPECT_FALSE(registry.isAvailable(action));
        EXPECT_EQ(registry.invoke(action).status,
                  ActionDispatchStatus::blocked);
        ASSERT_TRUE(registry.activateScope(panel));
        EXPECT_TRUE(registry.isAvailable(action));
        EXPECT_TRUE(registry.invoke(action).wasInvoked());
        EXPECT_EQ(invocations, 1);
    }

    TEST(ActionRegistryTests, InvocationMayUnregisterItsOwnAction) {
        ActionRegistry registry;
        const ActionId action{"project.close"};
        ASSERT_TRUE(registry.registerAction({
            .id = action,
            .state = {.label = "Close"},
            .invoked =
                [&registry](const ActionInvocation &invocation) {
                    EXPECT_TRUE(registry.unregisterAction(invocation.action));
                },
        }));

        const auto result = registry.invoke(action);
        EXPECT_TRUE(result.wasInvoked());
        EXPECT_EQ(result.action, action);
        EXPECT_FALSE(registry.contains(action));
    }

    TEST(ActionRegistryTests, ScopeRemovalCannotCreateOrphanedActions) {
        ActionRegistry registry;
        const auto panel =
            registry.createScope({.name = "Panel",
                                  .level = ActionScopeLevel::panel,
                                  .active = true});
        ASSERT_TRUE(registry.registerAction({
            .id = ActionId{"panel.close"},
            .scope = panel,
            .state = {.label = "Close"},
        }));

        auto attemptedRegistration = ActionRegistrationStatus::success;
        auto connection =
            registry.changed().connect([&](const ActionChange &change) {
                if (change.kind != ActionChangeKind::unregistered) {
                    return;
                }
                attemptedRegistration =
                    registry
                        .registerAction({
                            .id = ActionId{"panel.reentrant"},
                            .scope = panel,
                        })
                        .status;
            });

        EXPECT_TRUE(registry.removeScope(panel));
        EXPECT_EQ(attemptedRegistration,
                  ActionRegistrationStatus::unknownScope);
        EXPECT_EQ(registry.size(), 0U);
        EXPECT_FALSE(registry.scopeSnapshot(panel).id);
    }

    TEST(ActionRegistryTests,
         ScopeRemovalPreservesAnActionMovedByAReentrantObserver) {
        ActionRegistry registry;
        const auto panel =
            registry.createScope({.name = "Panel",
                                  .level = ActionScopeLevel::panel,
                                  .active = true});
        const ActionId first{"panel.first"};
        const ActionId rescued{"panel.rescued"};
        ASSERT_TRUE(registry.registerAction({.id = first, .scope = panel}));
        ASSERT_TRUE(registry.registerAction({.id = rescued, .scope = panel}));

        bool rescuedOnce = false;
        auto connection =
            registry.changed().connect([&](const ActionChange &change) {
                if (change.kind != ActionChangeKind::unregistered ||
                    rescuedOnce) {
                    return;
                }
                rescuedOnce = true;
                if (registry.contains(rescued)) {
                    EXPECT_TRUE(
                        registry.moveToScope(rescued, registry.globalScope()));
                } else {
                    EXPECT_TRUE(registry.registerAction({
                        .id = rescued,
                        .scope = registry.globalScope(),
                    }));
                }
            });

        EXPECT_TRUE(registry.removeScope(panel));
        EXPECT_FALSE(registry.contains(first));
        ASSERT_NE(registry.find(rescued), nullptr);
        EXPECT_EQ(registry.find(rescued)->scope, registry.globalScope());
    }

    TEST(ActionRegistryTests,
         ScopeRemovalRestoresInvariantsBeforeRethrowingObserverFailure) {
        ActionRegistry registry;
        const auto panel =
            registry.createScope({.name = "Panel",
                                  .level = ActionScopeLevel::panel,
                                  .active = true});
        const ActionId first{"panel.first"};
        const ActionId second{"panel.second"};
        ASSERT_TRUE(registry.registerAction({.id = first, .scope = panel}));
        ASSERT_TRUE(registry.registerAction({.id = second, .scope = panel}));

        size_t removalNotifications = 0;
        size_t scopeNotifications = 0;
        auto connection =
            registry.changed().connect([&](const ActionChange &change) {
                if (change.kind == ActionChangeKind::unregistered) {
                    ++removalNotifications;
                    if (removalNotifications == 1) {
                        throw std::logic_error("first observer failure");
                    }
                } else if (change.kind == ActionChangeKind::scopeChanged &&
                           change.scope == panel) {
                    ++scopeNotifications;
                    throw std::runtime_error("later observer failure");
                }
            });

        try {
            static_cast<void>(registry.removeScope(panel));
            FAIL() << "Expected the first observer failure to be rethrown";
        } catch (const std::logic_error &error) {
            EXPECT_STREQ(error.what(), "first observer failure");
        } catch (...) {
            FAIL() << "Expected the first observer failure type to be kept";
        }

        EXPECT_EQ(removalNotifications, 2U);
        EXPECT_EQ(scopeNotifications, 1U);
        EXPECT_FALSE(registry.scopeSnapshot(panel).id);
        EXPECT_FALSE(registry.contains(first));
        EXPECT_FALSE(registry.contains(second));
        EXPECT_EQ(registry.size(), 0U);

        EXPECT_TRUE(registry.registerAction({.id = first}));
        EXPECT_TRUE(registry.registerAction({.id = second}));
    }

    TEST(ActionRegistryTests, FailedShortcutMutationIsTransactional) {
        ActionRegistry registry;
        const KeyChord save{.key = KeyCode::s, .modifiers = control};
        const KeyChord open{.key = KeyCode::o, .modifiers = control};
        ASSERT_TRUE(registry.registerAction({
            .id = ActionId{"project.save"},
            .shortcuts = {save},
        }));
        ASSERT_TRUE(registry.registerAction({
            .id = ActionId{"project.open"},
            .shortcuts = {open},
        }));

        const auto result =
            registry.setShortcuts(ActionId{"project.open"}, {save});
        EXPECT_EQ(result.status, ActionRegistrationStatus::shortcutConflict);
        ASSERT_NE(registry.find(ActionId{"project.open"}), nullptr);
        EXPECT_EQ(registry.find(ActionId{"project.open"})->shortcuts,
                  std::vector<KeyChord>{open});
    }

    TEST(ActionRegistryTests, NormalizesCheckStateAndPublishesChanges) {
        ActionRegistry registry;
        const ActionId action{"view.grid"};
        std::vector<ActionChangeKind> changes;
        auto connection =
            registry.changed().connect([&changes](const ActionChange &change) {
                changes.push_back(change.kind);
            });

        ASSERT_TRUE(registry.registerAction({
            .id = action,
            .state = {.label = "Grid", .checkable = false, .checked = true},
        }));
        ASSERT_NE(registry.find(action), nullptr);
        EXPECT_FALSE(registry.find(action)->state.checked);

        EXPECT_TRUE(registry.updateState(action, [](ActionState &state) {
            state.checkable = true;
            state.checked = true;
        }));
        EXPECT_TRUE(registry.find(action)->state.checked);
        ASSERT_GE(changes.size(), 2U);
        EXPECT_EQ(changes[0], ActionChangeKind::registered);
        EXPECT_EQ(changes[1], ActionChangeKind::stateChanged);
    }

    TEST(ActionWidgetIntegrationTests,
         BoundButtonTracksActionStateAndTreeDispatchesDeclinedShortcut) {
        WidgetTree tree;
        tree.setViewportSize({240.f, 100.f});
        const ActionId action{"project.save"};
        size_t invocations = 0;
        WidgetId invokedFrom;
        ASSERT_TRUE(tree.actions().registerAction({
            .id = action,
            .state = {.label = "Save"},
            .shortcuts = {{.key = KeyCode::s, .modifiers = control}},
            .invoked =
                [&](const ActionInvocation &invocation) {
                    ++invocations;
                    invokedFrom = invocation.sourceWidget;
                },
        }));

        const auto binding =
            tree.emplaceWidget<ActionButton>(tree.actionRegistry(), action);
        const auto *bound = tree.getWidget<ActionButton>(binding);
        ASSERT_NE(bound, nullptr);
        const WidgetId button = bound->buttonId();
        ASSERT_TRUE(button);
        ASSERT_TRUE(tree.setFocus(button));

        const auto result = tree.dispatchEvent(UIEvent{
            Input::KeyEvent{.key = KeyCode::s, .action = KeyAction::press},
            {.control = true},
        });
        EXPECT_TRUE(result.handled);
        EXPECT_EQ(invocations, 1U);
        EXPECT_EQ(invokedFrom, button);

        ASSERT_TRUE(tree.actions().updateState(action, [](ActionState &state) {
            state.label = "Save all";
            state.enabled = false;
        }));
        ASSERT_NE(tree.getWidget<Button>(button), nullptr);
        EXPECT_EQ(tree.getWidget<Button>(button)->label(), "Save all");
        EXPECT_FALSE(tree.isEnabled(button));
        EXPECT_TRUE(
            tree.dispatchEvent(UIEvent{
                                   Input::KeyEvent{.key = KeyCode::s,
                                                   .action = KeyAction::press},
                                   {.control = true},
                               })
                .handled);
        EXPECT_EQ(invocations, 1U);

        ASSERT_TRUE(tree.actions().updateState(
            action, [](ActionState &state) { state.visible = false; }));
        EXPECT_EQ(tree.getVisibility(button), WidgetVisibility::collapsed);
    }

    TEST(ActionWidgetIntegrationTests,
         PointerActivationPreservesInputModifiers) {
        WidgetTree tree;
        tree.setViewportSize({200.f, 80.f});
        const ActionId action{"toolbar.alternate"};
        std::optional<Input::Modifiers> observed;
        ASSERT_TRUE(tree.actions().registerAction({
            .id = action,
            .state = {.label = "Run"},
            .invoked =
                [&](const ActionInvocation &invocation) {
                    observed = invocation.modifiers;
                },
        }));
        const auto binding =
            tree.emplaceWidget<ActionButton>(tree.actionRegistry(), action);
        const auto *bound = tree.getWidget<ActionButton>(binding);
        ASSERT_NE(bound, nullptr);
        const auto button = bound->buttonId();
        tree.performLayout();
        const auto position =
            tree.getBounds(button).center + tree.getViewportSize() * 0.5f;
        const Input::Modifiers modifiers{.control = true, .shift = true};

        static_cast<void>(tree.dispatchEvent(UIEvent{
            Input::MouseButtonEvent{.button = MouseButton::left,
                                    .action = MouseButtonAction::press,
                                    .pos = position},
            modifiers,
        }));
        static_cast<void>(tree.dispatchEvent(UIEvent{
            Input::MouseButtonEvent{.button = MouseButton::left,
                                    .action = MouseButtonAction::release,
                                    .pos = position},
            modifiers,
        }));

        ASSERT_TRUE(observed.has_value());
        EXPECT_TRUE(observed->control);
        EXPECT_TRUE(observed->shift);
    }

} // namespace
