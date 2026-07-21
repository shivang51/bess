#include "ui_core.h"

#include <gtest/gtest.h>

#include <memory>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>

namespace {
    using namespace Bess;
    using namespace Bess::UI;

    size_t countWidgetType(const WidgetTree &tree,
                           WidgetId id,
                           std::string_view type) {
        const auto *widget = tree.getWidget(id);
        size_t count =
            widget != nullptr && widget->typeName() == type ? 1U : 0U;
        for (const auto child : tree.getChildren(id)) {
            count += countWidgetType(tree, child, type);
        }
        return count;
    }

    size_t countWidgetType(const WidgetTree &tree, std::string_view type) {
        size_t count = 0;
        for (const auto root : tree.getRoots()) {
            count += countWidgetType(tree, root, type);
        }
        return count;
    }

    WidgetId
    findButton(const WidgetTree &tree, WidgetId id, std::string_view label) {
        if (const auto *button = tree.getWidget<Button>(id);
            button != nullptr && button->label() == label) {
            return id;
        }
        for (const auto child : tree.getChildren(id)) {
            if (const auto result = findButton(tree, child, label)) {
                return result;
            }
        }
        return {};
    }

    class BasicView final : public UIView {
      public:
        explicit BasicView(size_t *mounted = nullptr,
                           size_t *unmounted = nullptr)
            : m_mounted(mounted),
              m_unmounted(unmounted) {
        }

        void compose(UIComposer &ui) override {
            ui.column([](UIComposer &column) {
                column.label("Basic view");
                column.button("Action");
            });
        }

        void onMounted(UIViewContext &) override {
            if (m_mounted != nullptr) {
                ++*m_mounted;
            }
        }

        void onUnmounting(UIViewContext &) noexcept override {
            if (m_unmounted != nullptr) {
                ++*m_unmounted;
            }
        }

      private:
        size_t *m_mounted = nullptr;
        size_t *m_unmounted = nullptr;
    };

    class ThrowingView final : public UIView {
      public:
        void compose(UIComposer &ui) override {
            ui.label("Rolled back");
            throw std::runtime_error("composition failed");
        }
    };

    struct SelfUnmountProbe {
        bool returnedFromUnmount = false;
        bool destroyed = false;
    };

    class SelfUnmountView final : public UIView {
      public:
        explicit SelfUnmountView(SelfUnmountProbe &probe) : m_probe(probe) {
        }

        ~SelfUnmountView() override {
            m_probe.destroyed = true;
        }

        void compose(UIComposer &ui) override {
            m_button = ui.button("Unmount", [this] {
                EXPECT_FALSE(m_probe.destroyed);
                EXPECT_TRUE(m_host->unmount(m_id));
                // This member access must remain valid until the widget event
                // callback has unwound.
                m_probe.returnedFromUnmount = true;
                EXPECT_FALSE(m_probe.destroyed);
            });
        }

        void onMounted(UIViewContext &context) override {
            m_host = &context.host;
            m_id = context.id;
        }

        [[nodiscard]] WidgetId buttonId() const noexcept {
            return m_button.id();
        }

      private:
        SelfUnmountProbe &m_probe;
        UIViewHost *m_host = nullptr;
        ViewId m_id;
        WidgetRef<Button> m_button;
    };

    TEST(WidgetRefTests, ExpiresForWrongTypeRemovalAndTreeDestruction) {
        WidgetRef<Label> retained;
        {
            WidgetTree tree;
            const auto labelId = tree.emplaceWidget<Label>("Original");
            retained = WidgetRef<Label>{tree, labelId};

            ASSERT_TRUE(retained);
            EXPECT_FALSE((WidgetRef<Button>{tree, labelId}));
            EXPECT_TRUE(
                retained.update(WidgetInvalidation::paint, [](Label &label) {
                    label.setText("Updated");
                }));
            EXPECT_EQ(retained.get()->text(), "Updated");
            EXPECT_TRUE(retained.updateLayout(
                [](LayoutNode &layout) { layout.setWidth(240.f); }));

            EXPECT_TRUE(retained.remove());
            EXPECT_FALSE(retained);
        }
        EXPECT_FALSE(retained);
        EXPECT_EQ(retained.tree(), nullptr);
    }

    TEST(WidgetRefTests, RefreshesIntrinsicLayoutAfterContentAndThemeChanges) {
        WidgetTree tree;
        tree.setViewportSize({800.f, 300.f});
        const auto root = tree.emplaceWidget<FlexContainer>(
            FlexContainerOptions{.direction = LayoutDirection::vertical});
        const auto labelId = tree.emplaceChild<Label>(root, "A");
        const auto buttonId = tree.emplaceChild<Button>(root, "B");
        WidgetRef<Label> label{tree, labelId};
        WidgetRef<Button> button{tree, buttonId};
        tree.performLayout();

        const float initialLabelWidth = tree.getBounds(labelId).size.x;
        const float initialButtonWidth = tree.getBounds(buttonId).size.x;
        EXPECT_TRUE(label.update([](Label &value) {
            value.setText("A substantially longer label");
        }));
        EXPECT_TRUE(button.update([](Button &value) {
            value.setLabel("A substantially longer button");
        }));
        tree.performLayout();
        EXPECT_GT(tree.getBounds(labelId).size.x, initialLabelWidth);
        EXPECT_GT(tree.getBounds(buttonId).size.x, initialButtonWidth);

        auto theme = tree.theme();
        theme.label.fontSize *= 1.5f;
        tree.setTheme(std::move(theme));
        const float beforeThemeWidth = tree.getBounds(labelId).size.x;
        tree.performLayout();
        EXPECT_GT(tree.getBounds(labelId).size.x, beforeThemeWidth);
    }

    TEST(UIComposerTests, BuildsNestedControlsAndRollsBackFailedSubtrees) {
        WidgetTree tree;
        const auto root = tree.emplaceWidget<FlexContainer>();
        UIComposer ui{tree, root};

        const auto row = ui.row([](UIComposer &children) {
            children.label("Name");
            children.spacer();
            children.button("Save");
        });
        ASSERT_TRUE(row);
        EXPECT_EQ(tree.getChildren(row.id()).size(), 3);

        EXPECT_THROW(ui.column([](UIComposer &children) {
            children.label("Temporary");
            throw std::runtime_error("stop");
        }),
                     std::runtime_error);
        ASSERT_EQ(tree.getChildren(root).size(), 1);
        EXPECT_EQ(tree.getChildren(root).front(), row.id());
    }

    TEST(DockComposerTests, BuildsTopologyAndRollsBackFailedPanels) {
        WidgetTree tree;
        UIComposer ui{tree};
        const auto dockRef = ui.dockSpace();
        DockComposer dock{tree, dockRef};

        const auto explorer = dock.panel(
            "Explorer", [](UIComposer &panel) { panel.label("Files"); });
        ASSERT_TRUE(explorer);
        const auto inspector =
            dock.panel("Inspector",
                       DockPanelPlacement{
                           .target = dock.stackFor(explorer.item),
                           .zone = DockZone::right,
                       },
                       [](UIComposer &panel) { panel.button("Apply"); });
        ASSERT_TRUE(inspector);
        EXPECT_EQ(dock.model().itemCount(), 2);
        EXPECT_EQ(dock.model().stackCount(), 2);
        EXPECT_TRUE(dock.model().validate());

        EXPECT_THROW(dock.panel("Broken",
                                [](UIComposer &panel) {
                                    panel.label("Temporary");
                                    throw std::runtime_error("stop");
                                }),
                     std::runtime_error);
        EXPECT_EQ(dock.model().itemCount(), 2);
        EXPECT_EQ(tree.getChildren(dockRef.id()).size(), 2);
        EXPECT_TRUE(dock.model().validate());
    }

    TEST(UIViewHostTests, ReplacesContentTransactionallyAndOrdersLayers) {
        WidgetTree tree;
        tree.setViewportSize({800.f, 600.f});
        UIViewHost host{tree};
        size_t mounted = 0;
        size_t unmounted = 0;

        auto content = host.setContent<BasicView>(&mounted, &unmounted);
        auto overlay = host.mountOverlay<BasicView>();
        auto modal = host.mountModal<BasicView>();
        ASSERT_TRUE(content && overlay && modal);
        EXPECT_EQ(host.size(), 3);
        EXPECT_EQ(mounted, 1);

        const auto rootsBeforeFailure = tree.getRoots().size();
        EXPECT_THROW(host.setContent<ThrowingView>(), std::runtime_error);
        EXPECT_TRUE(content);
        EXPECT_EQ(host.content(), content.id());
        EXPECT_EQ(tree.getRoots().size(), rootsBeforeFailure);

        auto replacement = host.setContent<BasicView>(&mounted, &unmounted);
        ASSERT_TRUE(replacement);
        EXPECT_FALSE(content);
        EXPECT_EQ(unmounted, 1);

        const auto roots = tree.getRoots();
        ASSERT_EQ(roots.size(), 3);
        EXPECT_EQ(roots[0], replacement.root().id());
        EXPECT_EQ(roots[1], overlay.root().id());
        EXPECT_EQ(roots[2], modal.root().id());

        host.clear();
        EXPECT_TRUE(tree.getRoots().empty());
        EXPECT_FALSE(replacement);
        EXPECT_FALSE(overlay);
        EXPECT_FALSE(modal);
        EXPECT_EQ(unmounted, 2);
    }

    TEST(UIViewHostTests, KeepsViewAliveThroughSelfUnmountCallback) {
        UITarget target;
        target.resize({300.f, 120.f});
        SelfUnmountProbe probe;
        auto view = target.setContent<SelfUnmountView>(probe);
        ASSERT_TRUE(view);

        target.update(TimeMs{0});
        const WidgetId button = view.get()->buttonId();
        const auto bounds = target.getWidgetTree().getBounds(button);
        const glm::vec2 surfacePosition =
            bounds.center + target.getWidgetTree().getViewportSize() * 0.5f;

        target.enqueueEvent(Input::MouseButtonEvent{
            .button = MouseButton::left,
            .action = MouseButtonAction::press,
            .pos = surfacePosition,
        });
        target.enqueueEvent(Input::MouseButtonEvent{
            .button = MouseButton::left,
            .action = MouseButtonAction::release,
            .pos = surfacePosition,
        });
        target.update(TimeMs{0});

        EXPECT_TRUE(probe.returnedFromUnmount);
        EXPECT_TRUE(probe.destroyed);
        EXPECT_FALSE(view);
        EXPECT_EQ(target.getViewHost().size(), 0);
        EXPECT_TRUE(target.getWidgetTree().getRoots().empty());
    }

    TEST(UIDemoViewTests, MountsAndExercisesEveryShippedControl) {
        UITarget target;
        target.resize({1280.f, 800.f});
        auto demo = target.setContent<UIDemoView>();
        ASSERT_TRUE(demo);
        target.update(TimeMs{0});

        const auto &tree = target.getWidgetTree();
        EXPECT_GE(countWidgetType(tree, "FlexContainer"), 1);
        EXPECT_GE(countWidgetType(tree, "Surface"), 1);
        EXPECT_GE(countWidgetType(tree, "Label"), 1);
        EXPECT_GE(countWidgetType(tree, "Button"), 1);
        EXPECT_GE(countWidgetType(tree, "Spacer"), 1);
        EXPECT_EQ(countWidgetType(tree, "TabBar"), 1);
        EXPECT_EQ(countWidgetType(tree, "DockSpace"), 1);
        EXPECT_EQ(countWidgetType(tree, "DockPanel"), 5);

        ASSERT_NE(demo.get()->tabs(), nullptr);
        EXPECT_EQ(demo.get()->tabs()->size(), 3);
        auto dockRef = demo.get()->dockSpace();
        ASSERT_TRUE(dockRef);
        EXPECT_EQ(dockRef.get()->model().itemCount(), 5);
        EXPECT_EQ(dockRef.get()->model().stackCount(), 4);
        EXPECT_EQ(dockRef.get()->model().nodeCount(), 7);
        EXPECT_TRUE(dockRef.get()->model().validate());
        EXPECT_GT(tree.getBounds(dockRef.id()).size.y, 500.f)
            << tree.getBounds(dockRef.id()).size.y
            << " parent=" << tree.getBounds(tree.getParent(dockRef.id())).size.y
            << " grandparent="
            << tree.getBounds(tree.getParent(tree.getParent(dockRef.id())))
                   .size.y
            << " root=" << tree.getBounds(demo.root().id()).size.y;

        const auto dockLayout =
            dockRef.get()->model().layout(tree.getBounds(dockRef.id()),
                                          tree.theme().tabs.height,
                                          tree.theme().dock.splitterThickness);
        for (const auto &stack : dockLayout.stacks) {
            const auto *item = dockRef.get()->model().getItem(stack.activeItem);
            ASSERT_NE(item, nullptr);
            const auto panelChildren = tree.getChildren(item->content);
            ASSERT_EQ(panelChildren.size(), 1);
            const auto contentBounds = tree.getBounds(panelChildren.front());
            EXPECT_EQ(contentBounds.center, stack.contentBounds.center);
            EXPECT_EQ(contentBounds.size, stack.contentBounds.size);
            for (const auto control : tree.getChildren(panelChildren.front())) {
                const auto controlBounds = tree.getBounds(control);
                EXPECT_TRUE(contentBounds.contains(controlBounds.center))
                    << tree.getWidget(control)->typeName() << " center=["
                    << controlBounds.center.x << ", " << controlBounds.center.y
                    << "] content center=[" << contentBounds.center.x << ", "
                    << contentBounds.center.y << "] size=["
                    << contentBounds.size.x << ", " << contentBounds.size.y
                    << "]";
            }
        }

        const WidgetId counter = findButton(tree, demo.root().id(), "Count: 0");
        ASSERT_TRUE(counter);
        const auto bounds = tree.getBounds(counter);
        const glm::vec2 surfacePosition =
            bounds.center + tree.getViewportSize() * 0.5f;
        target.enqueueEvent(Input::MouseButtonEvent{
            .button = MouseButton::left,
            .action = MouseButtonAction::press,
            .pos = surfacePosition,
        });
        target.enqueueEvent(Input::MouseButtonEvent{
            .button = MouseButton::left,
            .action = MouseButtonAction::release,
            .pos = surfacePosition,
        });
        target.update(TimeMs{0});
        EXPECT_EQ(demo.get()->activationCount(), 1);
        EXPECT_TRUE(findButton(tree, demo.root().id(), "Count: 1"));
    }

    TEST(UIDemoViewTests, ReflowsToQueuedTargetResize) {
        UITarget target;
        target.resize({1280.f, 800.f});
        auto demo = target.setContent<UIDemoView>();
        ASSERT_TRUE(demo);
        target.update(TimeMs{0});

        target.enqueueEvent(UITargetResizeEvent{.width = 760, .height = 540});
        target.update(TimeMs{0});

        const auto &tree = target.getWidgetTree();
        EXPECT_EQ(target.getRect().size, glm::vec2(760.f, 540.f));
        EXPECT_EQ(tree.getViewportSize(), glm::vec2(760.f, 540.f));
        EXPECT_EQ(tree.getBounds(demo.root().id()).size,
                  glm::vec2(760.f, 540.f));
        const auto dock = demo.get()->dockSpace();
        ASSERT_TRUE(dock);
        EXPECT_FLOAT_EQ(tree.getBounds(dock.id()).size.x, 760.f)
            << " root=" << tree.getBounds(demo.root().id()).size.x
            << " parent=" << tree.getBounds(tree.getParent(dock.id())).size.x
            << " grandparent="
            << tree.getBounds(tree.getParent(tree.getParent(dock.id()))).size.x;
        EXPECT_LT(tree.getBounds(dock.id()).size.y, 540.f);
        EXPECT_GT(tree.getBounds(dock.id()).size.y, 180.f);
    }

} // namespace
