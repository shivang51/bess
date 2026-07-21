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
} // namespace
