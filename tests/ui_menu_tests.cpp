#include "ui_core.h"

#include "bess_core/ui/icons/font_awesome_icons.h"

#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <iterator>
#include <memory>
#include <string>
#include <vector>

namespace {
    using namespace Bess;
    using namespace Bess::UI;

    class MenuUnderlay final : public Widget {
      public:
        void onMount(WidgetMountContext &context) override {
            context.layout.setWidthPercent(1.f);
            context.layout.setHeightPercent(1.f);
        }
    };

    class MenuHoverUnderlay final : public Widget {
      public:
        WidgetTraits traits() const noexcept override {
            return {.hitTestVisible = true};
        }

        void onMount(WidgetMountContext &context) override {
            context.layout.setWidthStretch();
            context.layout.setHeightStretch();
        }

        UIEventReply onEvent(WidgetEventContext &context,
                             const UIEvent &event) override {
            if (context.phase != UIEventPhase::target) {
                return {};
            }
            if (const auto *crossing = event.getIf<UIPointerCrossingEvent>()) {
                hovered = crossing->entered;
            }
            if (event.is<Input::MouseMoveEvent>()) {
                ++mouseMoves;
            }
            return {};
        }

        bool hovered = false;
        size_t mouseMoves = 0;
    };

    class MenuRecordingPainter final : public UIPainter {
      public:
        glm::vec2 viewportSize() const noexcept override {
            return {640.f, 420.f};
        }

        void drawBox(const BoxPaint &paint) override {
            boxes.push_back(paint);
        }

        void drawText(std::string_view text, const TextPaint &paint) override {
            texts.emplace_back(text);
            textPaints.push_back(paint);
        }

        glm::vec2 measureText(std::string_view text,
                              float fontSize,
                              float letterSpacing) const override {
            return {static_cast<float>(text.size()) *
                        (fontSize * 0.6f + letterSpacing),
                    fontSize};
        }

        void pushClip(WidgetBounds) override {
        }

        void popClip() override {
        }

        std::vector<std::string> texts;
        std::vector<TextPaint> textPaints;
        std::vector<BoxPaint> boxes;
    };

    TEST(MenuModelTests, PreservesStableNestedCommandsAndState) {
        MenuModel model;
        const MenuItemId recent = MenuItemId::generate();
        const MenuItemId project = MenuItemId::generate();
        int activations = 0;
        const MenuId file = model.addMenu({
            .name = "File",
            .items =
                {
                    {.id = recent,
                     .name = "Recent",
                     .children = {{.id = project,
                                   .icon = "P",
                                   .name = "Project",
                                   .shortcut = "Ctrl+1",
                                   .activated =
                                       [&activations] { ++activations; }}}},
                    MenuItem::separator(),
                },
        });
        ASSERT_TRUE(file);
        EXPECT_TRUE(model.validate());
        ASSERT_NE(model.findItem(recent), nullptr);
        EXPECT_TRUE(model.findItem(recent)->isSubmenu());
        EXPECT_TRUE(model.setItemChecked(project, true));
        EXPECT_TRUE(model.findItem(project)->checked);
        EXPECT_TRUE(model.activate(project));
        EXPECT_EQ(activations, 1);
        EXPECT_TRUE(model.setItemEnabled(project, false));
        EXPECT_FALSE(model.activate(project));
        EXPECT_EQ(activations, 1);
        EXPECT_TRUE(model.validate());
    }

    TEST(MenuBarLayoutTests, PlacesNestedPopupsAndKeepsThemHittable) {
        MenuModel model;
        const MenuItemId submenu = MenuItemId::generate();
        const MenuItemId child = MenuItemId::generate();
        const MenuId menu = model.addMenu({
            .name = "Tools",
            .items = {{.id = submenu,
                       .icon = "T",
                       .name = "Generators",
                       .children = {{.id = child,
                                     .name = "Clock",
                                     .shortcut = "Ctrl+K"}}}},
        });
        ASSERT_TRUE(menu);

        const WidgetBounds viewport{.center = {0.f, 0.f},
                                    .size = {500.f, 300.f}};
        const WidgetBounds bar{.center = {0.f, -136.5f}, .size = {500.f, 27.f}};
        const auto layout =
            MenuBarLayoutSolver::calculate(bar,
                                           viewport,
                                           model,
                                           menu,
                                           std::array<MenuItemId, 1>{submenu},
                                           UITheme::dark().menus);
        ASSERT_EQ(layout.headings.size(), 1);
        ASSERT_EQ(layout.popups.size(), 2);
        ASSERT_EQ(layout.popups[0].items.size(), 1);
        ASSERT_EQ(layout.popups[1].items.size(), 1);
        const auto &rootPopup = layout.popups[0];
        const auto &rootItem = rootPopup.items[0];
        const float leadingInset =
            rootItem.iconBounds.topLeft().x - rootPopup.bounds.topLeft().x;
        const float trailingInset =
            rootPopup.bounds.bottomRight().x -
            rootItem.submenuIndicatorBounds.bottomRight().x;
        EXPECT_FLOAT_EQ(leadingInset, trailingInset)
            << "leading icons and trailing affordances must share the same "
               "outer inset";
        EXPECT_EQ(layout.itemAt(layout.popups[0].items[0].bounds.center)->item,
                  submenu);
        size_t depth = 0;
        EXPECT_EQ(
            layout.itemAt(layout.popups[1].items[0].bounds.center, &depth)
                ->item,
            child);
        EXPECT_EQ(depth, 1);
        EXPECT_TRUE(layout.contains(layout.popups[1].bounds.center));
        EXPECT_LE(layout.popups[1].bounds.bottomRight().x,
                  viewport.bottomRight().x - 3.9f);
        EXPECT_FLOAT_EQ(UITheme::dark().menus.barItem.cornerRadius.x, 5.f);
    }

    TEST(MenuBarLayoutTests, AlignsShortcutAndSubmenuAffordanceColumns) {
        MenuModel model;
        const MenuItemId command = MenuItemId::generate();
        const MenuItemId submenu = MenuItemId::generate();
        const MenuId file = model.addMenu({
            .name = "File",
            .items = {{.id = command, .name = "Open...", .shortcut = "Ctrl+O"},
                      {.id = submenu,
                       .name = "Open Recent",
                       .children = {{.name = "demo.bess"}}}},
        });

        const WidgetBounds viewport{.center = {0.f, 0.f},
                                    .size = {640.f, 420.f}};
        const WidgetBounds bar{.center = {0.f, -199.f}, .size = {640.f, 22.f}};
        const auto layout = MenuBarLayoutSolver::calculate(
            bar, viewport, model, file, {}, UITheme::dark().menus);

        ASSERT_EQ(layout.popups.size(), 1);
        ASSERT_EQ(layout.popups.front().items.size(), 2);
        const auto &commandRow = layout.popups.front().items[0];
        const auto &submenuRow = layout.popups.front().items[1];
        ASSERT_GT(commandRow.shortcutBounds.size.x, 0.f);
        ASSERT_GT(commandRow.submenuIndicatorBounds.size.x, 0.f);

        EXPECT_FLOAT_EQ(commandRow.shortcutBounds.bottomRight().x,
                        commandRow.submenuIndicatorBounds.topLeft().x);
        EXPECT_FLOAT_EQ(submenuRow.shortcutBounds.bottomRight().x,
                        submenuRow.submenuIndicatorBounds.topLeft().x);
        EXPECT_FLOAT_EQ(commandRow.shortcutBounds.center.x,
                        submenuRow.shortcutBounds.center.x);
        EXPECT_FLOAT_EQ(commandRow.submenuIndicatorBounds.center.x,
                        submenuRow.submenuIndicatorBounds.center.x);
        EXPECT_LE(commandRow.labelBounds.bottomRight().x,
                  commandRow.shortcutBounds.topLeft().x -
                      UITheme::dark().menus.shortcutGap);
    }

    TEST(MenuBarWidgetTests, OverflowCommandReceivesInputAndActivates) {
        WidgetTree state;
        state.setViewportSize({640.f, 420.f});
        auto model = std::make_shared<MenuModel>();
        const MenuItemId command = MenuItemId::generate();
        int activations = 0;
        const MenuId file = model->addMenu({
            .name = "File",
            .items = {{.id = command,
                       .icon = "+",
                       .name = "New",
                       .shortcut = "Ctrl+N",
                       .activated = [&activations] { ++activations; }}},
        });
        const WidgetId barId = state.emplaceWidget<MenuBar>(model);
        const WidgetId underlayId = state.emplaceWidget<MenuUnderlay>();
        ASSERT_TRUE(underlayId);
        auto *bar = state.getWidget<MenuBar>(barId);
        ASSERT_NE(bar, nullptr);
        state.performLayout();

        const auto barBounds = state.getBounds(barId);
        const auto closedLayout = MenuBarLayoutSolver::calculate(
            barBounds,
            {.center = {0.f, 0.f}, .size = state.getViewportSize()},
            *model,
            {},
            {},
            state.theme().menus);
        ASSERT_EQ(closedLayout.headings.size(), 1);
        const glm::vec2 viewportOffset = state.getViewportSize() * 0.5f;
        const glm::vec2 heading =
            closedLayout.headings.front().bounds.center + viewportOffset;
        static_cast<void>(state.dispatchEvent(Input::MouseButtonEvent{
            .button = MouseButton::left,
            .action = MouseButtonAction::press,
            .pos = heading,
        }));
        static_cast<void>(state.dispatchEvent(Input::MouseButtonEvent{
            .button = MouseButton::left,
            .action = MouseButtonAction::release,
            .pos = heading,
        }));
        ASSERT_TRUE(bar->isOpen());
        EXPECT_EQ(bar->activeMenu(), file);

        const auto openLayout = MenuBarLayoutSolver::calculate(
            barBounds,
            {.center = {0.f, 0.f}, .size = state.getViewportSize()},
            *model,
            file,
            {},
            state.theme().menus);
        ASSERT_EQ(openLayout.popups.size(), 1);
        const glm::vec2 commandPosition =
            openLayout.popups.front().items.front().bounds.center;
        EXPECT_EQ(state.hitTest(commandPosition), barId)
            << "popup overflow must route back to MenuBar";
        const glm::vec2 surfaceCommand = commandPosition + viewportOffset;
        static_cast<void>(state.dispatchEvent(Input::MouseButtonEvent{
            .button = MouseButton::left,
            .action = MouseButtonAction::press,
            .pos = surfaceCommand,
        }));
        static_cast<void>(state.dispatchEvent(Input::MouseButtonEvent{
            .button = MouseButton::left,
            .action = MouseButtonAction::release,
            .pos = surfaceCommand,
        }));
        EXPECT_EQ(activations, 1);
        EXPECT_FALSE(bar->isOpen());
        EXPECT_FALSE(state.getPointerCapture());
    }

    TEST(MenuBarWidgetTests, PointerOpensAndActivatesNestedSubmenuCommands) {
        WidgetTree state;
        state.setViewportSize({640.f, 420.f});
        auto model = std::make_shared<MenuModel>();
        const MenuItemId submenu = MenuItemId::generate();
        const MenuItemId child = MenuItemId::generate();
        int activations = 0;
        const MenuId file = model->addMenu({
            .name = "File",
            .items = {{.id = submenu,
                       .name = "Open Recent",
                       .children = {{.id = child,
                                     .name = "demo.bess",
                                     .activated =
                                         [&activations] { ++activations; }}}}},
        });
        const WidgetId barId = state.emplaceWidget<MenuBar>(model);
        auto *bar = state.getWidget<MenuBar>(barId);
        ASSERT_NE(bar, nullptr);
        state.performLayout();

        const auto viewport =
            WidgetBounds{.center = {0.f, 0.f}, .size = state.getViewportSize()};
        const glm::vec2 surfaceOffset = state.getViewportSize() * 0.5f;
        const auto closed =
            MenuBarLayoutSolver::calculate(state.getBounds(barId),
                                           viewport,
                                           *model,
                                           {},
                                           {},
                                           state.theme().menus);
        ASSERT_EQ(closed.headings.size(), 1);
        const glm::vec2 heading =
            closed.headings.front().bounds.center + surfaceOffset;
        static_cast<void>(state.dispatchEvent(Input::MouseButtonEvent{
            .button = MouseButton::left,
            .action = MouseButtonAction::press,
            .pos = heading,
        }));
        static_cast<void>(state.dispatchEvent(Input::MouseButtonEvent{
            .button = MouseButton::left,
            .action = MouseButtonAction::release,
            .pos = heading,
        }));
        ASSERT_TRUE(bar->isOpen());

        const auto rootPopup =
            MenuBarLayoutSolver::calculate(state.getBounds(barId),
                                           viewport,
                                           *model,
                                           file,
                                           {},
                                           state.theme().menus);
        ASSERT_EQ(rootPopup.popups.size(), 1);
        const glm::vec2 parent =
            rootPopup.popups.front().items.front().bounds.center +
            surfaceOffset;
        static_cast<void>(
            state.dispatchEvent(Input::MouseMoveEvent{.pos = parent}));

        MenuRecordingPainter painter;
        state.paint(painter);
        const auto chevron =
            std::find(painter.texts.begin(),
                      painter.texts.end(),
                      Icons::FontAwesomeIcons::FA_CHEVRON_RIGHT);
        ASSERT_NE(chevron, painter.texts.end())
            << "submenus must use the shared Font Awesome chevron";
        const auto chevronIndex =
            static_cast<size_t>(std::distance(painter.texts.begin(), chevron));
        ASSERT_LT(chevronIndex, painter.textPaints.size());
        EXPECT_FLOAT_EQ(painter.textPaints[chevronIndex].fontSize,
                        state.theme().menus.submenuChevronSize);
        EXPECT_GE(painter.textPaints[chevronIndex].fontSize, 10.f)
            << "submenu chevrons must remain legible in compact menus";
        EXPECT_TRUE(
            std::none_of(painter.texts.begin(),
                         painter.texts.end(),
                         [](const std::string &text) { return text == ">"; }))
            << "submenu indicators must not fall back to an ASCII bracket";

        const auto nested =
            MenuBarLayoutSolver::calculate(state.getBounds(barId),
                                           viewport,
                                           *model,
                                           file,
                                           std::array<MenuItemId, 1>{submenu},
                                           state.theme().menus);
        ASSERT_EQ(nested.popups.size(), 2);
        const glm::vec2 command =
            nested.popups.back().items.front().bounds.center + surfaceOffset;
        static_cast<void>(
            state.dispatchEvent(Input::MouseMoveEvent{.pos = command}));
        painter.boxes.clear();
        state.paint(painter);
        const auto hoveredColor =
            state.theme().menus.itemHovered.background.toHex();
        EXPECT_EQ(std::count_if(painter.boxes.begin(),
                                painter.boxes.end(),
                                [&state, hoveredColor](const BoxPaint &box) {
                                    return box.color.toHex() == hoveredColor &&
                                           std::abs(
                                               box.bounds.size.y -
                                               state.theme().menus.itemHeight) <
                                               0.01f;
                                }),
                  2)
            << "the submenu ancestry remains visually active";
        static_cast<void>(state.dispatchEvent(Input::MouseButtonEvent{
            .button = MouseButton::left,
            .action = MouseButtonAction::press,
            .pos = command,
        }));
        static_cast<void>(state.dispatchEvent(Input::MouseButtonEvent{
            .button = MouseButton::left,
            .action = MouseButtonAction::release,
            .pos = command,
        }));

        EXPECT_EQ(activations, 1);
        EXPECT_FALSE(bar->isOpen());
        EXPECT_FALSE(state.getPointerCapture());
    }

    TEST(MenuBarWidgetTests, PopupHitTestingUsesAccumulatedPaintZ) {
        WidgetTree state;
        state.setViewportSize({640.f, 420.f});
        auto model = std::make_shared<MenuModel>();
        const MenuItemId panels = MenuItemId::generate();
        const MenuItemId console = MenuItemId::generate();
        const MenuId view = model->addMenu({
            .name = "View",
            .items = {{.id = panels,
                       .name = "Panels",
                       .children = {{.id = console, .name = "Console"}}}},
        });

        UIComposer ui{state};
        WidgetRef<MenuBar> menuBar;
        WidgetRef<MenuHoverUnderlay> underlay;
        static_cast<void>(ui.column(
            FlexContainerOptions{
                .direction = LayoutDirection::vertical,
                .crossAxisAlignment = LayoutAlignment::start,
                .stretchWidth = true,
                .stretchHeight = true,
                .clipChildren = false,
                .hitTestVisible = false,
            },
            [&](UIComposer &page) {
                static_cast<void>(page.row(
                    FlexContainerOptions{
                        .direction = LayoutDirection::horizontal,
                        .crossAxisAlignment = LayoutAlignment::center,
                        .stretchWidth = true,
                        .stretchHeight = false,
                        .clipChildren = false,
                        .hitTestVisible = false,
                    },
                    [&](UIComposer &header) {
                        menuBar = header.menuBar(model);
                    }));
                underlay = page.emplace<MenuHoverUnderlay>();
            }));
        state.performLayout();

        const WidgetBounds viewport{.center = {0.f, 0.f},
                                    .size = state.getViewportSize()};
        const glm::vec2 surfaceOffset = state.getViewportSize() * 0.5f;
        const auto closed =
            MenuBarLayoutSolver::calculate(state.getBounds(menuBar.id()),
                                           viewport,
                                           *model,
                                           {},
                                           {},
                                           state.theme().menus);
        ASSERT_EQ(closed.headings.size(), 1);
        const glm::vec2 heading =
            closed.headings.front().bounds.center + surfaceOffset;
        static_cast<void>(state.dispatchEvent(Input::MouseButtonEvent{
            .button = MouseButton::left,
            .action = MouseButtonAction::press,
            .pos = heading,
        }));
        static_cast<void>(state.dispatchEvent(Input::MouseButtonEvent{
            .button = MouseButton::left,
            .action = MouseButtonAction::release,
            .pos = heading,
        }));
        ASSERT_NE(menuBar.get(), nullptr);
        ASSERT_TRUE(menuBar.get()->isOpen());

        const auto rootPopup =
            MenuBarLayoutSolver::calculate(state.getBounds(menuBar.id()),
                                           viewport,
                                           *model,
                                           view,
                                           {},
                                           state.theme().menus);
        ASSERT_EQ(rootPopup.popups.size(), 1);
        const glm::vec2 panelsPosition =
            rootPopup.popups.front().items.front().bounds.center;
        EXPECT_EQ(state.hitTest(panelsPosition), menuBar.id());
        static_cast<void>(state.dispatchEvent(Input::MouseMoveEvent{
            .pos = panelsPosition + surfaceOffset,
        }));

        const auto nested =
            MenuBarLayoutSolver::calculate(state.getBounds(menuBar.id()),
                                           viewport,
                                           *model,
                                           view,
                                           std::array<MenuItemId, 1>{panels},
                                           state.theme().menus);
        ASSERT_EQ(nested.popups.size(), 2);
        const glm::vec2 consolePosition =
            nested.popups.back().items.front().bounds.center;
        EXPECT_EQ(state.hitTest(consolePosition), menuBar.id());
        static_cast<void>(state.dispatchEvent(Input::MouseMoveEvent{
            .pos = consolePosition + surfaceOffset,
        }));

        ASSERT_NE(underlay.get(), nullptr);
        EXPECT_FALSE(underlay.get()->hovered);
        EXPECT_EQ(underlay.get()->mouseMoves, 0);
        EXPECT_EQ(state.getHoveredWidget(), menuBar.id());
    }

    TEST(MenuBarWidgetTests, IsCompactAndContentSizedInsideAToolbarRow) {
        WidgetTree state;
        state.setViewportSize({640.f, 120.f});
        auto model = std::make_shared<MenuModel>();
        static_cast<void>(model->addMenu({.name = "File"}));
        static_cast<void>(model->addMenu({.name = "Edit"}));

        UIComposer ui{state};
        WidgetRef<MenuBar> menuBar;
        WidgetRef<Label> trailingAction;
        static_cast<void>(ui.row(
            FlexContainerOptions{
                .direction = LayoutDirection::horizontal,
                .crossAxisAlignment = LayoutAlignment::center,
                .stretchWidth = true,
                .stretchHeight = false,
            },
            [&](UIComposer &toolbar) {
                static_cast<void>(toolbar.label("B"));
                menuBar = toolbar.menuBar(model);
                static_cast<void>(toolbar.spacer());
                trailingAction = toolbar.label("Action");
            }));
        state.performLayout();

        const auto menuBounds = state.getBounds(menuBar.id());
        const auto actionBounds = state.getBounds(trailingAction.id());
        const auto &menuStyle = state.theme().menus;
        EXPECT_FLOAT_EQ(menuBounds.size.y,
                        menuStyle.barHeight +
                            menuStyle.barVerticalMargin * 2.f);
        EXPECT_LT(menuBounds.size.x, state.getViewportSize().x * 0.5f);
        EXPECT_GT(actionBounds.center.x, menuBounds.center.x);
        EXPECT_FLOAT_EQ(menuStyle.barHeight, 22.f);
        EXPECT_GE(menuStyle.barVerticalMargin, 2.f);
        EXPECT_FLOAT_EQ(menuStyle.barText.fontSize,
                        state.theme().tabs.text.fontSize);
        EXPECT_FLOAT_EQ(menuStyle.text.fontSize,
                        state.theme().tabs.text.fontSize);

        const WidgetBounds viewport{.center = {0.f, 0.f},
                                    .size = state.getViewportSize()};
        const auto layout = MenuBarLayoutSolver::calculate(
            menuBounds, viewport, *model, {}, {}, menuStyle);
        EXPECT_FLOAT_EQ(layout.barBounds.size.y, menuStyle.barHeight);
        EXPECT_FLOAT_EQ(layout.barBounds.topLeft().y - menuBounds.topLeft().y,
                        menuStyle.barVerticalMargin);
        EXPECT_FLOAT_EQ(menuBounds.bottomRight().y -
                            layout.barBounds.bottomRight().y,
                        menuStyle.barVerticalMargin);
    }

    TEST(MenuActionBindingTests, SyncsPresentationAndInvokesRegistryActions) {
        auto registry = std::make_shared<ActionRegistry>();
        int invocations = 0;
        ASSERT_TRUE(registry->registerAction({
            .id = ActionId{"shell.file.save"},
            .state = {.label = "Save", .icon = "S"},
            .shortcuts = {{.key = KeyCode::s,
                           .modifiers = KeyChordModifier::control}},
            .invoked = [&invocations](const ActionInvocation &invocation) {
                ++invocations;
                EXPECT_EQ(invocation.source, ActionInvocationSource::menu);
                EXPECT_EQ(invocation.action, ActionId{"shell.file.save"});
            },
        }));

        MenuModel model;
        model.setActionRegistry(registry);
        const MenuItemId save = MenuItemId::generate();
        const MenuItemId unbound = MenuItemId::generate();
        int legacyActivations = 0;
        ASSERT_TRUE(model.addMenu({
            .name = "File",
            .items =
                {
                    {.id = save, .action = ActionId{"shell.file.save"}},
                    MenuItem::separator(),
                    {.id = unbound,
                     .name = "Legacy",
                     .activated = [&legacyActivations] { ++legacyActivations; }},
                },
        }));

        const auto *bound = model.findItem(save);
        ASSERT_NE(bound, nullptr);
        EXPECT_EQ(bound->name, "Save");
        EXPECT_EQ(bound->icon, "S");
        EXPECT_FALSE(bound->shortcut.empty());
        EXPECT_TRUE(bound->enabled);
        EXPECT_TRUE(bound->visible);
        EXPECT_TRUE(model.activate(save));
        EXPECT_EQ(invocations, 1);

        ASSERT_TRUE(registry->updateState(
            ActionId{"shell.file.save"}, [](ActionState &state) {
                state.enabled = false;
                state.label = "Save Project";
            }));
        bound = model.findItem(save);
        ASSERT_NE(bound, nullptr);
        EXPECT_EQ(bound->name, "Save Project");
        EXPECT_FALSE(bound->enabled);
        EXPECT_FALSE(model.activate(save));
        EXPECT_EQ(invocations, 1);

        EXPECT_FALSE(model.setItemEnabled(save, true));
        EXPECT_TRUE(model.activate(unbound));
        EXPECT_EQ(legacyActivations, 1);
        EXPECT_TRUE(model.validate());
    }

    TEST(MenuActionBindingTests, HidesUnavailableActionsAndSupportsManyToOne) {
        auto registry = std::make_shared<ActionRegistry>();
        ASSERT_TRUE(registry->registerAction({
            .id = ActionId{"shell.view.console"},
            .state = {.label = "Console", .visible = true},
        }));

        MenuModel model;
        model.setActionRegistry(registry);
        const MenuItemId viewConsole = MenuItemId::generate();
        const MenuItemId windowConsole = MenuItemId::generate();
        ASSERT_TRUE(model.addMenu({
            .name = "View",
            .items = {{.id = viewConsole,
                       .action = ActionId{"shell.view.console"}}},
        }));
        ASSERT_TRUE(model.addMenu({
            .name = "Window",
            .items = {{.id = windowConsole,
                       .action = ActionId{"shell.view.console"}}},
        }));

        ASSERT_TRUE(registry->updateState(
            ActionId{"shell.view.console"},
            [](ActionState &state) { state.visible = false; }));

        ASSERT_NE(model.findItem(viewConsole), nullptr);
        ASSERT_NE(model.findItem(windowConsole), nullptr);
        EXPECT_FALSE(model.findItem(viewConsole)->visible);
        EXPECT_FALSE(model.findItem(windowConsole)->visible);

        const auto preferred = MenuPopupLayoutSolver::preferredSize(
            model.findMenu(model.menus().front().id)->items,
            UITheme::dark().menus);
        EXPECT_FLOAT_EQ(preferred.y,
                        UITheme::dark().menus.popupPadding * 2.f);
    }
} // namespace
