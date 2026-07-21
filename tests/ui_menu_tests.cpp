#include "ui_core.h"

#include <gtest/gtest.h>

#include <array>
#include <memory>
#include <string>

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
} // namespace
