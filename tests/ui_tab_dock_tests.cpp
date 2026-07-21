#include "ui_core.h"

#include <gtest/gtest.h>

#include <memory>
#include <string>
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
                          state.theme().tabs.height * 0.5f} +
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
        const glm::vec2 redockHeader =
            glm::vec2{redockBounds->center.x,
                      redockBounds->topLeft().y +
                          state.theme().tabs.height * 0.5f} +
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
} // namespace
