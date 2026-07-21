#include "ui_core.h"

#include "bess_core/style/bess_theme.h"

#include <gtest/gtest.h>

#include <algorithm>
#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace {
    using namespace Bess;
    using namespace Bess::UI;

    DockItem dockItem(std::string title) {
        return {.id = DockItemId::generate(), .title = std::move(title)};
    }

    class DockContent final : public Widget {
      public:
        WidgetTraits traits() const noexcept override {
            return {.hitTestVisible = true};
        }
    };

    class DockRecordingPainter final : public UIPainter {
      public:
        glm::vec2 viewportSize() const noexcept override {
            return {800.f, 600.f};
        }

        void drawBox(const BoxPaint &paint) override {
            boxes.push_back(paint);
        }

        void drawText(std::string_view, const TextPaint &) override {
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

        std::vector<BoxPaint> boxes;
    };

    TEST(TabModelTests, MaintainsSelectionAndEmitsAtomicChanges) {
        TabModel model;
        std::vector<TabChange> changes;
        auto connection = model.changed().connect(
            [&changes](const TabChange &change) { changes.push_back(change); });

        const TabId first = model.add("First");
        const TabId second = model.add("Second");
        const TabId third = model.add("Third");
        ASSERT_TRUE(model.validate());
        EXPECT_EQ(model.active(), first);
        EXPECT_EQ(changes.size(), 3);

        changes.clear();
        EXPECT_TRUE(model.activate(second));
        ASSERT_EQ(changes.size(), 1);
        EXPECT_EQ(changes[0].previousActive, first);
        EXPECT_EQ(changes[0].active, second);

        changes.clear();
        EXPECT_TRUE(model.move(second, 0));
        ASSERT_EQ(changes.size(), 1);
        EXPECT_EQ(changes[0].kind, TabChangeKind::moved);
        EXPECT_EQ(model.items()[0].id, second);

        changes.clear();
        EXPECT_TRUE(model.setEnabled(second, false));
        ASSERT_EQ(changes.size(), 1);
        EXPECT_NE(model.active(), second);
        EXPECT_TRUE(model.validate());

        auto detached = model.detach(third);
        ASSERT_TRUE(detached);
        TabModel destination;
        EXPECT_TRUE(destination.attach(std::move(detached)));
        EXPECT_NE(destination.find(third), nullptr);
        EXPECT_EQ(model.find(third), nullptr);
        EXPECT_TRUE(model.validate());
        EXPECT_TRUE(destination.validate());
        EXPECT_TRUE(connection.connected());
    }

    TEST(TabStripLayoutTests, ProducesDeterministicRegionsAndHitTesting) {
        const WidgetBounds bounds{.center = {0.f, 0.f}, .size = {300.f, 40.f}};
        const auto regions =
            TabStripLayout::calculate(bounds,
                                      3,
                                      {.height = 32.f,
                                       .minimumWidth = 60.f,
                                       .maximumWidth = 120.f,
                                       .horizontalPadding = 8.f});
        ASSERT_EQ(regions.size(), 3);
        EXPECT_FLOAT_EQ(regions[0].bounds.size.x, 100.f);
        EXPECT_LT(regions[0].bounds.center.x, regions[1].bounds.center.x);
        const auto hit =
            TabStripLayout::hitTest(regions, regions[1].bounds.center);
        ASSERT_TRUE(hit.has_value());
        EXPECT_EQ(*hit, 1);
    }

    TEST(TabStripLayoutTests, DarkThemeUsesSlimDockTabs) {
        const auto theme = UITheme::dark();
        const auto &style = theme.tabs;
        const WidgetBounds bounds{.center = {0.f, 0.f},
                                  .size = {320.f, style.height}};
        const auto regions = TabStripLayout::calculate(
            bounds,
            2,
            {.height = style.height,
             .minimumWidth = style.minimumWidth,
             .maximumWidth = style.maximumWidth,
             .horizontalPadding = style.horizontalPadding,
             .stripPadding = style.stripPadding,
             .gap = style.gap});

        ASSERT_EQ(regions.size(), 2);
        EXPECT_FLOAT_EQ(style.height, 28.f);
        EXPECT_FLOAT_EQ(regions.front().bounds.size.y,
                        style.height - style.stripPadding.y * 2.f);
        EXPECT_EQ(style.text.fontSize, 13.f);
        EXPECT_EQ(style.active.cornerRadius,
                  theme.menus.itemHovered.cornerRadius);
        EXPECT_EQ(style.active.cornerRadius,
                  theme.menus.barItemHovered.cornerRadius);
    }

    TEST(UIThemeTests, DerivesEveryComponentPaletteFromBessThemeRoles) {
        using Color = Core::Renderer::Color;
        using Core::Style::BessTheme;
        using Core::Style::Brightness;
        using Core::Style::ColorScheme;

        auto colors =
            BessTheme::defaultDarkTheme()->getColorScheme().getColors();
        colors.primary = Color::fromRGBA8(11, 12, 13);
        colors.onPrimary = Color::fromRGBA8(21, 22, 23);
        colors.primaryContainer = Color::fromRGBA8(31, 32, 33);
        colors.outline = Color::fromRGBA8(41, 42, 43);
        colors.outlineVariant = Color::fromRGBA8(51, 52, 53);
        colors.surface = Color::fromRGBA8(61, 62, 63);
        colors.onSurface = Color::fromRGBA8(71, 72, 73);
        colors.surfaceContainerLowest = Color::fromRGBA8(81, 82, 83);
        colors.surfaceContainerLow = Color::fromRGBA8(91, 92, 93);
        colors.surfaceContainer = Color::fromRGBA8(101, 102, 103);
        colors.surfaceContainerHigh = Color::fromRGBA8(111, 112, 113);
        colors.surfaceContainerHighest = Color::fromRGBA8(121, 122, 123);
        colors.onSurfaceVariant = Color::fromRGBA8(131, 132, 133);
        colors.shadow = Color::fromRGBA8(141, 142, 143);

        const ColorScheme scheme{colors, Brightness::dark};
        const BessTheme source{std::string_view{"UICore sentinel"}, scheme};
        const auto theme = UITheme::fromBessTheme(source);

        const auto expectColor = [](const Color &actual,
                                    const Color &expected,
                                    std::string_view role) {
            EXPECT_EQ(actual.toHex(), expected.toHex()) << role;
        };
        const auto transparent = colors.surface.withAlpha(0.f);

        expectColor(theme.canvas, colors.surface, "canvas");
        expectColor(theme.surface.background,
                    colors.surfaceContainerLow,
                    "surface background");
        expectColor(theme.surface.border, transparent, "surface border");
        EXPECT_EQ(theme.surface.borderThickness, glm::vec4(0.f));
        expectColor(theme.panel.background,
                    colors.surfaceContainerLow,
                    "panel background");
        expectColor(theme.panel.border, colors.outlineVariant, "panel border");
        expectColor(theme.panel.shadow.color,
                    colors.shadow.withAlpha(0.f),
                    "panel disabled shadow");
        expectColor(theme.label.color, colors.onSurface, "label");

        expectColor(theme.button.normal.background,
                    colors.surfaceContainerHigh,
                    "button normal");
        expectColor(theme.button.hovered.background,
                    colors.surfaceContainerHighest,
                    "button hovered");
        expectColor(theme.button.pressed.background,
                    colors.surfaceContainer,
                    "button pressed");
        expectColor(
            theme.button.focused.border, colors.primary, "button focus border");
        expectColor(theme.button.disabled.background,
                    colors.surfaceContainerLow,
                    "button disabled");
        expectColor(theme.button.disabled.border,
                    colors.outlineVariant.withAlpha(0.45f),
                    "button disabled border");
        expectColor(theme.button.text.color, colors.onSurface, "button text");
        expectColor(theme.button.disabledText,
                    colors.onSurface.withAlpha(0.38f),
                    "button disabled text");

        expectColor(theme.tabs.strip.background,
                    colors.surfaceContainerLowest,
                    "tab strip");
        expectColor(theme.tabs.normal.background, transparent, "tab normal");
        expectColor(theme.tabs.hovered.background,
                    colors.surfaceContainerHigh,
                    "tab hovered");
        expectColor(theme.tabs.active.background,
                    colors.surfaceContainerHighest,
                    "tab active");
        expectColor(theme.tabs.active.border,
                    colors.outlineVariant,
                    "tab active border");
        expectColor(theme.tabs.pressed.background,
                    colors.surfaceContainer,
                    "tab pressed");
        expectColor(theme.tabs.text.color, colors.onSurface, "tab text");
        expectColor(theme.tabs.inactiveText,
                    colors.onSurfaceVariant,
                    "tab inactive text");

        expectColor(theme.menus.bar.background,
                    colors.surfaceContainerLowest,
                    "menu bar");
        expectColor(
            theme.menus.bar.border, colors.outlineVariant, "menu bar border");
        expectColor(
            theme.menus.barItem.background, transparent, "menu bar item");
        expectColor(theme.menus.barItemHovered.background,
                    colors.surfaceContainerHigh,
                    "menu bar item hovered");
        expectColor(theme.menus.barItemActive.background,
                    colors.surfaceContainerHighest,
                    "menu bar item active");
        expectColor(theme.menus.popup.background,
                    colors.surfaceContainerLow.withAlpha(0.98f),
                    "menu popup");
        expectColor(
            theme.menus.popup.border, transparent, "menu borderless popup");
        EXPECT_EQ(theme.menus.popup.borderThickness, glm::vec4(0.f));
        expectColor(theme.menus.popup.shadow.color,
                    colors.shadow.withAlpha(0.45f),
                    "menu shadow");
        expectColor(theme.menus.itemHovered.background,
                    colors.surfaceContainerHighest,
                    "menu item hovered");
        expectColor(theme.menus.itemPressed.background,
                    colors.surfaceContainer,
                    "menu item pressed");
        expectColor(theme.menus.text.color, colors.onSurface, "menu text");
        expectColor(
            theme.menus.barText.color, colors.onSurface, "menu bar text");
        EXPECT_FLOAT_EQ(theme.menus.text.fontSize, theme.tabs.text.fontSize);
        EXPECT_FLOAT_EQ(theme.menus.barText.fontSize, theme.tabs.text.fontSize);
        expectColor(
            theme.menus.iconColor, colors.onSurfaceVariant, "menu icon");
        expectColor(theme.menus.shortcutColor,
                    colors.onSurfaceVariant.withAlpha(0.78f),
                    "menu shortcut");
        expectColor(theme.menus.disabledText,
                    colors.onSurface.withAlpha(0.38f),
                    "menu disabled text");
        expectColor(
            theme.menus.separator, colors.outlineVariant, "menu separator");

        expectColor(theme.dock.background.background,
                    colors.surface,
                    "dock background");
        expectColor(theme.dock.stack.background,
                    colors.surfaceContainerLow,
                    "dock stack");
        expectColor(theme.dock.floatingWindow.background,
                    colors.surfaceContainerLow,
                    "floating window");
        expectColor(theme.dock.floatingWindow.border,
                    colors.outline,
                    "floating border");
        expectColor(theme.dock.floatingWindow.shadow.color,
                    colors.shadow.withAlpha(0.47f),
                    "floating shadow");
        expectColor(theme.dock.floatingHeader.background,
                    colors.surfaceContainerHighest,
                    "floating header");
        expectColor(
            theme.dock.splitter, colors.outlineVariant, "dock splitter");
        expectColor(theme.dock.splitterHovered,
                    colors.primary,
                    "dock splitter hovered");
        EXPECT_GT(theme.dock.splitterThickness,
                  theme.dock.splitterIdleThickness);
        EXPECT_GT(theme.dock.splitterIdleThickness, 0.f);
        expectColor(theme.dock.dropGuide.background,
                    colors.primaryContainer.withAlpha(0.92f),
                    "dock guide");
        expectColor(theme.dock.dropGuide.border,
                    colors.primary.withAlpha(0.86f),
                    "dock guide border");
        expectColor(theme.dock.dropGuideHovered.background,
                    colors.primary.withAlpha(0.96f),
                    "dock guide hovered");
        expectColor(theme.dock.dropGuideHovered.border,
                    colors.onPrimary,
                    "dock guide hovered border");
        expectColor(theme.dock.dropPreview.background,
                    colors.primary.withAlpha(0.30f),
                    "dock preview");
        expectColor(theme.dock.dropPreview.border,
                    colors.primary.withAlpha(0.86f),
                    "dock preview border");

        EXPECT_EQ(theme.tabs.active.cornerRadius,
                  theme.menus.itemHovered.cornerRadius);
        EXPECT_EQ(theme.tabs.active.cornerRadius,
                  theme.menus.barItemHovered.cornerRadius);
    }

    TEST(DockDropGuideLayoutTests, ProducesNodeLocalZonesWithMatchingPreviews) {
        const DockNodeId target = DockNodeId::generate();
        const WidgetBounds bounds{.center = {50.f, -20.f},
                                  .size = {600.f, 400.f}};
        const auto guides = DockDropGuideLayoutSolver::calculate(
            bounds,
            target,
            {.indicatorSize = 40.f, .indicatorGap = 8.f, .previewInset = 6.f});

        ASSERT_FALSE(guides.empty());
        EXPECT_EQ(guides.target, target);
        ASSERT_EQ(guides.regions.size(), 5);
        const auto *main = guides.region(DockZone::main);
        const auto *left = guides.region(DockZone::left);
        const auto *right = guides.region(DockZone::right);
        const auto *top = guides.region(DockZone::top);
        const auto *bottom = guides.region(DockZone::bottom);
        ASSERT_NE(main, nullptr);
        ASSERT_NE(left, nullptr);
        ASSERT_NE(right, nullptr);
        ASSERT_NE(top, nullptr);
        ASSERT_NE(bottom, nullptr);
        EXPECT_EQ(guides.regionAt(main->indicatorBounds.center), main);
        EXPECT_LT(left->indicatorBounds.center.x,
                  main->indicatorBounds.center.x);
        EXPECT_GT(right->indicatorBounds.center.x,
                  main->indicatorBounds.center.x);
        EXPECT_LT(top->indicatorBounds.center.y,
                  main->indicatorBounds.center.y);
        EXPECT_GT(bottom->indicatorBounds.center.y,
                  main->indicatorBounds.center.y);
        EXPECT_FLOAT_EQ(left->previewBounds.size.x,
                        main->previewBounds.size.x * 0.5f);
        EXPECT_FLOAT_EQ(top->previewBounds.size.y,
                        main->previewBounds.size.y * 0.5f);
    }

    TEST(DockDropGuideLayoutTests, RootGuideHasOnlyWholeHostEdgeDestinations) {
        const DockNodeId root = DockNodeId::generate();
        const WidgetBounds bounds{.center = {0.f, 0.f}, .size = {800.f, 500.f}};
        const auto guide =
            DockDropGuideLayoutSolver::calculateRootEdges(bounds, root);
        ASSERT_EQ(guide.regions.size(), 4);
        EXPECT_EQ(guide.target, root);
        EXPECT_EQ(guide.region(DockZone::main), nullptr);
        ASSERT_NE(guide.region(DockZone::left), nullptr);
        ASSERT_NE(guide.region(DockZone::right), nullptr);
        ASSERT_NE(guide.region(DockZone::top), nullptr);
        ASSERT_NE(guide.region(DockZone::bottom), nullptr);
        EXPECT_LT(guide.region(DockZone::left)->indicatorBounds.center.x,
                  bounds.center.x);
        EXPECT_FLOAT_EQ(guide.region(DockZone::bottom)->previewBounds.size.y,
                        (bounds.size.y - 10.f) * 0.5f);
    }

    TEST(DockStyleTests, FloatingContentDoesNotDrawASecondRoundedFrame) {
        const auto &style = UITheme::dark().dock.floatingStack;
        EXPECT_FLOAT_EQ(style.cornerRadius.x, 0.f);
        EXPECT_FLOAT_EQ(style.cornerRadius.y, 0.f);
        EXPECT_GT(style.cornerRadius.z, 0.f);
        EXPECT_GT(style.cornerRadius.w, 0.f);
        EXPECT_EQ(style.borderThickness, glm::vec4(0.f));
        EXPECT_FALSE(style.shadow.enabled);
    }

    TEST(DockSpaceModelTests, FourSideDockedItemsProduceFourTerminalStacks) {
        DockSpaceModel model;
        const auto a = model.addItem(dockItem("A"));
        const auto stackA = model.stackForItem(a);
        const auto b = model.addItem(dockItem("B"), stackA, DockZone::right);
        const auto c = model.addItem(dockItem("C"), stackA, DockZone::bottom);
        const auto d =
            model.addItem(dockItem("D"), model.stackForItem(c), DockZone::left);

        ASSERT_TRUE(a && b && c && d);
        std::string reason;
        EXPECT_TRUE(model.validate(&reason)) << reason;
        EXPECT_EQ(model.itemCount(), 4);
        EXPECT_EQ(model.stackCount(), 4);
        EXPECT_EQ(model.nodeCount(), 7);

        const auto layout = model.layout(
            {.center = {0.f, 0.f}, .size = {1000.f, 700.f}}, 34.f, 4.f);
        EXPECT_EQ(layout.stacks.size(), 4);
        EXPECT_EQ(layout.splits.size(), 3);
        for (const auto &stack : layout.stacks) {
            ASSERT_NE(model.getStack(stack.node), nullptr);
            EXPECT_EQ(model.getStack(stack.node)->tabs.size(), 1);
            EXPECT_FALSE(stack.contentBounds.empty());
        }
    }

    TEST(DockSpaceModelTests,
         MovingLastItemCollapsesSourceWithoutIdentityChurn) {
        DockSpaceModel model;
        const auto a = model.addItem(dockItem("A"));
        const auto stackA = model.stackForItem(a);
        const auto b = model.addItem(dockItem("B"), stackA, DockZone::right);
        const auto stackB = model.stackForItem(b);
        ASSERT_NE(stackA, stackB);

        EXPECT_TRUE(model.moveItem(a, stackB, DockZone::main));
        EXPECT_EQ(model.stackCount(), 1);
        EXPECT_EQ(model.nodeCount(), 1);
        EXPECT_EQ(model.stackForItem(a), stackB);
        EXPECT_EQ(model.stackForItem(b), stackB);
        EXPECT_EQ(model.root(), stackB);
        EXPECT_TRUE(model.validate());

        EXPECT_TRUE(model.moveItem(a, stackB, DockZone::left));
        EXPECT_EQ(model.stackCount(), 2);
        EXPECT_EQ(model.nodeCount(), 3);
        EXPECT_NE(model.stackForItem(a), model.stackForItem(b));
        EXPECT_TRUE(model.validate());
    }

    TEST(DockSpaceModelTests,
         DetachAndAttachTransferPreservesDockItemIdentity) {
        DockSpaceModel source;
        DockSpaceModel destination;
        const auto item = source.addItem(dockItem("Movable"));
        auto detached = source.detachItem(item);
        ASSERT_TRUE(detached);
        ASSERT_NE(detached.get(), nullptr);
        EXPECT_EQ(detached.get()->id, item);
        EXPECT_TRUE(source.empty());

        EXPECT_TRUE(destination.attachItem(std::move(detached)));
        EXPECT_NE(destination.getItem(item), nullptr);
        EXPECT_TRUE(destination.validate());
    }

    TEST(DockSpaceModelTests, RootAttachmentWrapsTheCompleteExistingTree) {
        DockSpaceModel destination;
        const auto a = destination.addItem(dockItem("A"));
        const auto b = destination.addItem(
            dockItem("B"), destination.stackForItem(a), DockZone::right);
        const DockNodeId previousRoot = destination.root();
        ASSERT_NE(destination.getSplit(previousRoot), nullptr);

        DockSpaceModel source;
        const auto c = source.addItem(dockItem("C"));
        auto detached = source.detachItem(c);
        ASSERT_TRUE(destination.attachItemAtRoot(std::move(detached),
                                                 DockZone::bottom));

        const auto *newRoot = destination.getSplit(destination.root());
        ASSERT_NE(newRoot, nullptr);
        EXPECT_EQ(newRoot->first, previousRoot);
        EXPECT_EQ(newRoot->second, destination.stackForItem(c));
        EXPECT_EQ(destination.parentOf(previousRoot), destination.root());
        EXPECT_EQ(destination.itemCount(), 3);
        EXPECT_TRUE(destination.validate());
        EXPECT_TRUE(source.empty());
        EXPECT_TRUE(a && b && c);
    }

    TEST(DockSpaceModelTests, WholeTreeAttachmentPreservesNestedTopology) {
        DockSpaceModel destination;
        const auto destinationItem = destination.addItem(dockItem("Target"));
        const auto destinationStack = destination.stackForItem(destinationItem);

        DockSpaceModel source;
        const auto a = source.addItem(dockItem("A"));
        const auto b = source.addItem(
            dockItem("B"), source.stackForItem(a), DockZone::right);
        const auto c = source.addItem(
            dockItem("C"), source.stackForItem(a), DockZone::bottom);
        const DockNodeId sourceRoot = source.root();
        ASSERT_EQ(source.stackCount(), 3);

        ASSERT_TRUE(destination.attachTree(
            std::move(source), destinationStack, DockZone::left));
        EXPECT_TRUE(source.empty());
        EXPECT_EQ(destination.itemCount(), 4);
        EXPECT_EQ(destination.stackCount(), 4);
        EXPECT_EQ(destination.nodeCount(), 7);
        EXPECT_EQ(destination.parentOf(sourceRoot), destination.root());
        EXPECT_NE(destination.getItem(a), nullptr);
        EXPECT_NE(destination.getItem(b), nullptr);
        EXPECT_NE(destination.getItem(c), nullptr);
        EXPECT_TRUE(destination.validate());
    }

    TEST(DockSpaceWidgetTests, ArrangesOnlyTheActivePanelsArbitraryContent) {
        WidgetTree state;
        state.setViewportSize({800.f, 600.f});
        const WidgetId dockId = state.emplaceWidget<DockSpace>();
        auto *dock = state.getWidget<DockSpace>(dockId);
        ASSERT_NE(dock, nullptr);

        const auto first = dock->createPanel(
            state, dockId, "Explorer", std::make_unique<DockContent>());
        const auto second = dock->createPanel(
            state, dockId, "Inspector", std::make_unique<DockContent>());
        ASSERT_TRUE(first);
        ASSERT_TRUE(second);
        state.performLayout();

        const auto initialLayout =
            dock->model().layout(state.getBounds(dockId),
                                 state.theme().tabs.height,
                                 state.theme().dock.splitterThickness);
        const auto *stackLayout = initialLayout.findStack(dock->model().root());
        ASSERT_NE(stackLayout, nullptr);
        const WidgetId secondContent = state.getChildren(second.panel)[0];
        EXPECT_EQ(state.hitTest(stackLayout->contentBounds.center),
                  secondContent);

        ASSERT_TRUE(dock->model().activateItem(first.item));
        state.performLayout();
        const WidgetId firstContent = state.getChildren(first.panel)[0];
        EXPECT_EQ(state.hitTest(stackLayout->contentBounds.center),
                  firstContent);

        EXPECT_TRUE(dock->removePanel(state, second.item));
        EXPECT_FALSE(state.contains(second.panel));
        EXPECT_EQ(dock->model().itemCount(), 1);
        EXPECT_TRUE(dock->model().validate());
    }

    TEST(DockSpaceWidgetTests, SplitterDragUsesPointerCaptureAndUpdatesRatio) {
        WidgetTree state;
        state.setViewportSize({800.f, 600.f});
        const WidgetId dockId = state.emplaceWidget<DockSpace>();
        auto *dock = state.getWidget<DockSpace>(dockId);
        ASSERT_NE(dock, nullptr);
        const auto left = dock->createPanel(
            state, dockId, "Left", std::make_unique<DockContent>());
        const auto right =
            dock->createPanel(state,
                              dockId,
                              "Right",
                              std::make_unique<DockContent>(),
                              dock->model().stackForItem(left.item),
                              DockZone::right);
        ASSERT_TRUE(left);
        ASSERT_TRUE(right);
        state.performLayout();

        const DockNodeId splitId = dock->model().root();
        ASSERT_NE(dock->model().getSplit(splitId), nullptr);
        const auto dockLayout =
            dock->model().layout(state.getBounds(dockId),
                                 state.theme().tabs.height,
                                 state.theme().dock.splitterThickness);
        const auto *splitLayout = dockLayout.findSplit(splitId);
        ASSERT_NE(splitLayout, nullptr);
        const glm::vec2 surfaceCenter =
            splitLayout->dividerBounds.center + state.getViewportSize() * 0.5f;

        static_cast<void>(state.dispatchEvent(Input::MouseButtonEvent{
            .button = MouseButton::left,
            .action = MouseButtonAction::press,
            .pos = surfaceCenter,
        }));
        EXPECT_EQ(state.getPointerCapture(), dockId);
        static_cast<void>(state.dispatchEvent(Input::MouseMoveEvent{
            .pos = surfaceCenter + glm::vec2{100.f, 0.f},
        }));
        EXPECT_GT(dock->model().getSplit(splitId)->ratio, 0.5f);
        static_cast<void>(state.dispatchEvent(Input::MouseButtonEvent{
            .button = MouseButton::left,
            .action = MouseButtonAction::release,
            .pos = surfaceCenter + glm::vec2{100.f, 0.f},
        }));
        EXPECT_FALSE(state.getPointerCapture());
        EXPECT_TRUE(dock->model().validate());
    }

    TEST(DockSpaceWidgetTests,
         SplitterKeepsHitTargetButOnlyExpandsVisualWhenHovered) {
        WidgetTree state;
        state.setViewportSize({800.f, 600.f});
        const WidgetId dockId = state.emplaceWidget<DockSpace>();
        auto *dock = state.getWidget<DockSpace>(dockId);
        ASSERT_NE(dock, nullptr);
        const auto left = dock->createPanel(
            state, dockId, "Left", std::make_unique<DockContent>());
        const auto right =
            dock->createPanel(state,
                              dockId,
                              "Right",
                              std::make_unique<DockContent>(),
                              dock->model().stackForItem(left.item),
                              DockZone::right);
        ASSERT_TRUE(left && right);
        state.performLayout();

        const auto layout =
            dock->model().layout(state.getBounds(dockId),
                                 state.theme().tabs.height,
                                 state.theme().dock.splitterThickness);
        const auto *splitLayout = layout.findSplit(dock->model().root());
        ASSERT_NE(splitLayout, nullptr);
        EXPECT_FLOAT_EQ(splitLayout->dividerBounds.size.x,
                        state.theme().dock.splitterThickness);

        const auto findBoxWithColor =
            [](const DockRecordingPainter &painter,
               Core::Renderer::Color color) -> const BoxPaint * {
            const auto found =
                std::find_if(painter.boxes.begin(),
                             painter.boxes.end(),
                             [color](const BoxPaint &box) {
                                 return box.color.toHex() == color.toHex();
                             });
            return found != painter.boxes.end() ? &*found : nullptr;
        };

        DockRecordingPainter painter;
        state.paint(painter);
        const auto *idle =
            findBoxWithColor(painter, state.theme().dock.splitter);
        ASSERT_NE(idle, nullptr);
        EXPECT_FLOAT_EQ(idle->bounds.size.x,
                        state.theme().dock.splitterIdleThickness);
        EXPECT_EQ(idle->bounds.center, splitLayout->dividerBounds.center);

        const glm::vec2 pointer =
            splitLayout->dividerBounds.center + state.getViewportSize() * 0.5f;
        static_cast<void>(
            state.dispatchEvent(Input::MouseMoveEvent{.pos = pointer}));
        painter.boxes.clear();
        state.paint(painter);
        const auto *hovered =
            findBoxWithColor(painter, state.theme().dock.splitterHovered);
        ASSERT_NE(hovered, nullptr);
        EXPECT_FLOAT_EQ(hovered->bounds.size.x,
                        state.theme().dock.splitterThickness);
        EXPECT_EQ(hovered->bounds.center, splitLayout->dividerBounds.center);
    }

    TEST(DockSpaceWidgetTests, CompletedTabClickActivatesThePressedTab) {
        WidgetTree state;
        state.setViewportSize({800.f, 600.f});
        const WidgetId dockId = state.emplaceWidget<DockSpace>();
        auto *dock = state.getWidget<DockSpace>(dockId);
        ASSERT_NE(dock, nullptr);
        const auto first = dock->createPanel(
            state, dockId, "First", std::make_unique<DockContent>());
        const auto second = dock->createPanel(
            state, dockId, "Second", std::make_unique<DockContent>());
        ASSERT_TRUE(first && second);
        state.performLayout();

        const auto stackId = dock->model().stackForItem(first.item);
        const auto layout =
            dock->model().layout(state.getBounds(dockId),
                                 state.theme().tabs.height,
                                 state.theme().dock.splitterThickness);
        const auto *stackLayout = layout.findStack(stackId);
        ASSERT_NE(stackLayout, nullptr);
        const auto regions = TabStripLayout::calculate(
            stackLayout->tabBarBounds,
            2,
            {.height = state.theme().tabs.height,
             .minimumWidth = state.theme().tabs.minimumWidth,
             .maximumWidth = state.theme().tabs.maximumWidth,
             .horizontalPadding = state.theme().tabs.horizontalPadding,
             .stripPadding = state.theme().tabs.stripPadding,
             .gap = state.theme().tabs.gap});
        ASSERT_EQ(regions.size(), 2);
        const glm::vec2 position =
            regions.front().bounds.center + state.getViewportSize() * 0.5f;
        static_cast<void>(state.dispatchEvent(Input::MouseButtonEvent{
            .button = MouseButton::left,
            .action = MouseButtonAction::press,
            .pos = position,
        }));
        static_cast<void>(state.dispatchEvent(Input::MouseButtonEvent{
            .button = MouseButton::left,
            .action = MouseButtonAction::release,
            .pos = position,
        }));

        ASSERT_NE(dock->model().getStack(stackId), nullptr);
        EXPECT_EQ(dock->model().getStack(stackId)->tabs.active(), first.item);
        EXPECT_FALSE(state.getPointerCapture());
    }

    TEST(DockSpaceWidgetTests,
         TabDragFloatsWithoutAZoneAndRedocksThroughNodeGuide) {
        WidgetTree state;
        state.setViewportSize({900.f, 600.f});
        const WidgetId dockId = state.emplaceWidget<DockSpace>();
        auto *dock = state.getWidget<DockSpace>(dockId);
        ASSERT_NE(dock, nullptr);
        const auto first = dock->createPanel(
            state, dockId, "First", std::make_unique<DockContent>());
        const auto second = dock->createPanel(
            state, dockId, "Second", std::make_unique<DockContent>());
        ASSERT_TRUE(first);
        ASSERT_TRUE(second);
        state.performLayout();

        const auto stackId = dock->model().stackForItem(first.item);
        const auto initialLayout =
            dock->model().layout(state.getBounds(dockId),
                                 state.theme().tabs.height,
                                 state.theme().dock.splitterThickness);
        const auto *stack = initialLayout.findStack(stackId);
        ASSERT_NE(stack, nullptr);
        const auto regions = TabStripLayout::calculate(
            stack->tabBarBounds,
            2,
            {.height = state.theme().tabs.height,
             .minimumWidth = state.theme().tabs.minimumWidth,
             .maximumWidth = state.theme().tabs.maximumWidth,
             .horizontalPadding = state.theme().tabs.horizontalPadding,
             .stripPadding = state.theme().tabs.stripPadding,
             .gap = state.theme().tabs.gap});
        ASSERT_EQ(regions.size(), 2);
        const glm::vec2 viewportOffset = state.getViewportSize() * 0.5f;
        const glm::vec2 press = regions[0].bounds.center + viewportOffset;
        const glm::vec2 floatAt = press + glm::vec2{0.f, 100.f};

        static_cast<void>(state.dispatchEvent(Input::MouseButtonEvent{
            .button = MouseButton::left,
            .action = MouseButtonAction::press,
            .pos = press,
        }));
        EXPECT_EQ(state.getPointerCapture(), dockId);
        static_cast<void>(
            state.dispatchEvent(Input::MouseMoveEvent{.pos = floatAt}));
        EXPECT_TRUE(dock->isItemFloating(first.item));
        EXPECT_EQ(dock->floatingItemCount(), 1);
        EXPECT_EQ(dock->model().itemCount(), 1);
        static_cast<void>(state.dispatchEvent(Input::MouseButtonEvent{
            .button = MouseButton::left,
            .action = MouseButtonAction::release,
            .pos = floatAt,
        }));
        EXPECT_FALSE(state.getPointerCapture());
        EXPECT_TRUE(dock->isItemFloating(first.item));
        EXPECT_TRUE(dock->model().validate());

        state.performLayout();
        const auto floatingBounds = dock->floatingItemBounds(first.item);
        ASSERT_TRUE(floatingBounds.has_value());
        const glm::vec2 floatingHeader =
            glm::vec2{floatingBounds->center.x,
                      floatingBounds->topLeft().y +
                          state.theme().dock.floatingTitleBarHeight * 0.5f} +
            viewportOffset;
        static_cast<void>(state.dispatchEvent(Input::MouseButtonEvent{
            .button = MouseButton::left,
            .action = MouseButtonAction::press,
            .pos = floatingHeader,
        }));
        ASSERT_EQ(state.getPointerCapture(), dockId);

        const auto dockedLayout =
            dock->model().layout(state.getBounds(dockId),
                                 state.theme().tabs.height,
                                 state.theme().dock.splitterThickness);
        ASSERT_EQ(dockedLayout.stacks.size(), 1);
        const glm::vec2 mainGuide =
            dockedLayout.stacks.front().bounds.center + viewportOffset;
        static_cast<void>(
            state.dispatchEvent(Input::MouseMoveEvent{.pos = mainGuide}));
        ASSERT_TRUE(dock->isItemFloating(first.item));
        // A release can carry a newer coordinate without a preceding move.
        // It must not reuse the previously hovered guide.
        static_cast<void>(state.dispatchEvent(Input::MouseButtonEvent{
            .button = MouseButton::left,
            .action = MouseButtonAction::release,
            .pos = {-20.f, -20.f},
        }));
        EXPECT_FALSE(state.getPointerCapture());
        EXPECT_TRUE(dock->isItemFloating(first.item));

        state.performLayout();
        const auto redockBounds = dock->floatingItemBounds(first.item);
        ASSERT_TRUE(redockBounds.has_value());
        const auto dockBounds = state.getBounds(dockId);
        const glm::vec2 redockHeader =
            glm::vec2{
                std::clamp(redockBounds->center.x,
                           dockBounds.topLeft().x + 1.f,
                           dockBounds.bottomRight().x - 1.f),
                std::clamp(redockBounds->topLeft().y +
                               state.theme().dock.floatingTitleBarHeight * 0.5f,
                           dockBounds.topLeft().y + 1.f,
                           dockBounds.bottomRight().y - 1.f)} +
            viewportOffset;
        static_cast<void>(state.dispatchEvent(Input::MouseButtonEvent{
            .button = MouseButton::left,
            .action = MouseButtonAction::press,
            .pos = redockHeader,
        }));
        ASSERT_EQ(state.getPointerCapture(), dockId);
        static_cast<void>(
            state.dispatchEvent(Input::MouseMoveEvent{.pos = mainGuide}));
        static_cast<void>(state.dispatchEvent(Input::MouseButtonEvent{
            .button = MouseButton::left,
            .action = MouseButtonAction::release,
            .pos = mainGuide,
        }));

        EXPECT_FALSE(dock->isItemFloating(first.item));
        EXPECT_EQ(dock->floatingItemCount(), 0);
        EXPECT_EQ(dock->model().itemCount(), 2);
        EXPECT_EQ(dock->model().stackCount(), 1);
        EXPECT_EQ(dock->model().stackForItem(first.item),
                  dock->model().stackForItem(second.item));
        EXPECT_TRUE(dock->model().validate());
    }

    TEST(DockSpaceWidgetTests, FloatingHostsReceiveTabsFromOtherHosts) {
        WidgetTree state;
        state.setViewportSize({1000.f, 700.f});
        const WidgetId dockId = state.emplaceWidget<DockSpace>();
        auto *dock = state.getWidget<DockSpace>(dockId);
        ASSERT_NE(dock, nullptr);
        const auto first = dock->createPanel(
            state, dockId, "First", std::make_unique<DockContent>());
        const auto second = dock->createPanel(
            state, dockId, "Second", std::make_unique<DockContent>());
        ASSERT_TRUE(first && second);
        state.performLayout();

        const WidgetBounds dockBounds = state.getBounds(dockId);
        ASSERT_TRUE(dock->floatItem(
            first.item,
            {.center = dockBounds.center + glm::vec2{-170.f, 20.f},
             .size = {320.f, 260.f}}));
        ASSERT_TRUE(dock->floatItem(
            second.item,
            {.center = dockBounds.center + glm::vec2{190.f, 30.f},
             .size = {320.f, 260.f}}));
        state.performLayout();
        ASSERT_EQ(dock->floatingWindowCount(), 2);
        ASSERT_TRUE(dock->model().empty());

        const auto sourceBounds = dock->floatingItemBounds(second.item);
        const auto targetBounds = dock->floatingItemBounds(first.item);
        ASSERT_TRUE(sourceBounds && targetBounds);
        const glm::vec2 viewportOffset = state.getViewportSize() * 0.5f;
        const glm::vec2 sourceHeader =
            glm::vec2{sourceBounds->center.x,
                      sourceBounds->topLeft().y +
                          state.theme().dock.floatingTitleBarHeight * 0.5f} +
            viewportOffset;
        static_cast<void>(state.dispatchEvent(Input::MouseButtonEvent{
            .button = MouseButton::left,
            .action = MouseButtonAction::press,
            .pos = sourceHeader,
        }));
        ASSERT_EQ(state.getPointerCapture(), dockId);

        const float title = state.theme().dock.floatingTitleBarHeight;
        const glm::vec2 targetClientCenter = targetBounds->center +
                                             glm::vec2{0.f, title * 0.5f} +
                                             viewportOffset;
        static_cast<void>(state.dispatchEvent(
            Input::MouseMoveEvent{.pos = targetClientCenter}));
        static_cast<void>(state.dispatchEvent(Input::MouseButtonEvent{
            .button = MouseButton::left,
            .action = MouseButtonAction::release,
            .pos = targetClientCenter,
        }));

        EXPECT_EQ(dock->floatingWindowCount(), 1);
        EXPECT_EQ(dock->floatingItemCount(), 2);
        EXPECT_TRUE(dock->isItemFloating(first.item));
        EXPECT_TRUE(dock->isItemFloating(second.item));
        const auto mergedFirst = dock->floatingItemBounds(first.item);
        const auto mergedSecond = dock->floatingItemBounds(second.item);
        ASSERT_TRUE(mergedFirst && mergedSecond);
        EXPECT_EQ(mergedFirst->center, mergedSecond->center);
        EXPECT_EQ(mergedFirst->size, mergedSecond->size);
        EXPECT_TRUE(dock->model().empty());
    }

    TEST(DockSpaceWidgetTests,
         FloatingWindowKeepsMovingPastBoundsWhileRetainingATitleGrip) {
        WidgetTree state;
        state.setViewportSize({800.f, 600.f});
        const WidgetId dockId = state.emplaceWidget<DockSpace>();
        auto *dock = state.getWidget<DockSpace>(dockId);
        ASSERT_NE(dock, nullptr);
        const auto panel = dock->createPanel(
            state, dockId, "Floating", std::make_unique<DockContent>());
        ASSERT_TRUE(panel);
        state.performLayout();
        const auto dockBounds = state.getBounds(dockId);
        ASSERT_TRUE(dock->floatItem(
            panel.item, {.center = dockBounds.center, .size = {320.f, 240.f}}));
        state.performLayout();

        const auto before = dock->floatingItemBounds(panel.item);
        ASSERT_TRUE(before);
        const glm::vec2 viewportOffset = state.getViewportSize() * 0.5f;
        const glm::vec2 header =
            glm::vec2{before->center.x,
                      before->topLeft().y +
                          state.theme().dock.floatingTitleBarHeight * 0.5f} +
            viewportOffset;
        static_cast<void>(state.dispatchEvent(Input::MouseButtonEvent{
            .button = MouseButton::left,
            .action = MouseButtonAction::press,
            .pos = header,
        }));
        static_cast<void>(state.dispatchEvent(Input::MouseMoveEvent{
            .pos = header + glm::vec2{-600.f, 0.f},
        }));
        static_cast<void>(state.dispatchEvent(Input::MouseButtonEvent{
            .button = MouseButton::left,
            .action = MouseButtonAction::release,
            .pos = header + glm::vec2{-600.f, 0.f},
        }));

        const auto after = dock->floatingItemBounds(panel.item);
        ASSERT_TRUE(after);
        EXPECT_LT(after->topLeft().x, dockBounds.topLeft().x);
        const float visible = after->bottomRight().x - dockBounds.topLeft().x;
        EXPECT_GE(visible,
                  state.theme().dock.floatingVisibleTitleWidth - 0.01f);
        EXPECT_LT(after->center.x, before->center.x);
    }
} // namespace
