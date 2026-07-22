#include "ui_core.h"

#include <gtest/gtest.h>

#include <limits>
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

    class ThrowingUnmountWidget final : public Widget {
      public:
        void onUnmount(WidgetTree &, WidgetId) override {
            throw std::runtime_error("unmount callback failed");
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

    TEST(WidgetRefTests, ProvidesDeclarativeLayoutAndInteractionHelpers) {
        WidgetTree tree;
        tree.setViewportSize({400.f, 200.f});
        UIComposer ui{tree};
        auto button = ui.button("Action");

        const auto configured = button.withLayout({
            .width = LayoutLength::percent(25.f),
            .height = 28.f,
            .minSize = glm::vec2{40.f, 20.f},
            .margin = Core::Style::Margin::fromHorizontal(3.f),
            .alignSelf = LayoutSelfAlignment::center,
            .flexGrow = 1.f,
            .flexShrink = 0.f,
            .flexBasis = LayoutLength::fitContent(),
            .zIndex = 2.f,
        });
        ASSERT_TRUE(configured);
        const auto *layout = tree.getLayout(button.id());
        ASSERT_NE(layout, nullptr);
        EXPECT_EQ(layout->getWidthMode(), LayoutSizeMode::percent);
        EXPECT_FLOAT_EQ(layout->getWidthValue(), 25.f);
        EXPECT_EQ(layout->getHeightMode(), LayoutSizeMode::point);
        EXPECT_FLOAT_EQ(layout->getHeightValue(), 28.f);
        EXPECT_EQ(layout->getMinSize(), glm::vec2(40.f, 20.f));
        EXPECT_EQ(layout->getMargin(),
                  Core::Style::Margin::fromHorizontal(3.f));
        EXPECT_EQ(layout->getAlignSelf(), LayoutSelfAlignment::center);
        EXPECT_FLOAT_EQ(layout->getFlexGrow(), 1.f);
        EXPECT_FLOAT_EQ(layout->getFlexShrink(), 0.f);
        EXPECT_FLOAT_EQ(layout->getZVal(), 2.f);

        tree.performLayout();
        EXPECT_TRUE(button.focus());
        EXPECT_TRUE(button.isFocused());
        EXPECT_TRUE(button.blur());
        EXPECT_FALSE(button.isFocused());
        EXPECT_TRUE(button.hide());
        EXPECT_EQ(button.visibility(), WidgetVisibility::hidden);
        EXPECT_TRUE(button.show());
        EXPECT_TRUE(button.setEnabled(false));
        EXPECT_FALSE(button.isEnabled());
        EXPECT_TRUE(button.setEnabled(true));
        EXPECT_TRUE(button.collapse());
        EXPECT_EQ(button.visibility(), WidgetVisibility::collapsed);
    }

    TEST(WidgetRefTests, KeepsLayoutBorrowAliveThroughDeferredRemoval) {
        WidgetTree tree;
        const auto id = tree.emplaceWidget<Label>("Temporary");
        WidgetRef<Label> label{tree, id};
        ASSERT_TRUE(label);

        EXPECT_TRUE(label.updateLayout([&label](LayoutNode &layout) {
            EXPECT_TRUE(label.remove());
            // This remains valid until the mutation returns.
            layout.setWidth(42.f);
        }));
        EXPECT_FALSE(label);

        const auto throwingId = tree.emplaceWidget<Label>("Throwing");
        WidgetRef<Label> throwing{tree, throwingId};
        EXPECT_THROW(throwing.updateLayout([&throwing](LayoutNode &) {
            EXPECT_TRUE(throwing.remove());
            throw std::runtime_error("layout mutation failed");
        }),
                     std::runtime_error);
        EXPECT_FALSE(throwing);
    }

    TEST(WidgetRefTests, FlushesEveryDeferredRemovalAfterUnmountFailure) {
        WidgetTree tree;
        auto owner =
            WidgetRef<Label>{tree, tree.emplaceWidget<Label>("Mutation owner")};
        const WidgetId throwing = tree.emplaceWidget<ThrowingUnmountWidget>();
        const WidgetId ordinary = tree.emplaceWidget<Label>("Ordinary");
        ASSERT_TRUE(owner && throwing && ordinary);

        EXPECT_THROW(owner.mutate([&tree, throwing, ordinary](Label &) {
            EXPECT_TRUE(tree.removeWidget(throwing));
            EXPECT_TRUE(tree.removeWidget(ordinary));
        }),
                     std::runtime_error);
        EXPECT_FALSE(tree.contains(throwing));
        EXPECT_FALSE(tree.contains(ordinary));
        EXPECT_TRUE(owner);
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
        EXPECT_EQ(tree.getLayout(row.id())->getCrossAxisAlignment(),
                  LayoutAlignment::center);
        EXPECT_EQ(tree.getChildren(row.id()).size(), 3);

        EXPECT_THROW(ui.column([](UIComposer &children) {
            children.label("Temporary");
            throw std::runtime_error("stop");
        }),
                     std::runtime_error);
        ASSERT_EQ(tree.getChildren(root).size(), 1);
        EXPECT_EQ(tree.getChildren(root).front(), row.id());
    }

    TEST(UIComposerTests, BuildsAndRollsBackStackSubtreesTransactionally) {
        WidgetTree tree;
        const auto root = tree.emplaceWidget<FlexContainer>();
        UIComposer ui{tree, root};

        const auto stack = ui.stack([](UIComposer &overlay) {
            overlay.surface();
            overlay.label("Foreground");
        });
        ASSERT_TRUE(stack);
        EXPECT_EQ(stack.get()->typeName(), "StackContainer");
        EXPECT_EQ(tree.getChildren(stack.id()).size(), 2);

        EXPECT_THROW(ui.stack([](UIComposer &overlay) {
            overlay.label("Temporary");
            throw std::runtime_error("stop");
        }),
                     std::runtime_error);
        ASSERT_EQ(tree.getChildren(root).size(), 1);
        EXPECT_EQ(tree.getChildren(root).front(), stack.id());
    }

    TEST(StackContainerTests, AlignsChildrenInsidePaddingAndMargins) {
        WidgetTree tree;
        tree.setViewportSize({300.f, 200.f});
        const auto stackId =
            tree.emplaceWidget<StackContainer>(StackContainerOptions{
                .horizontalAlignment = StackAlignment::center,
                .verticalAlignment = StackAlignment::end,
                .padding = {10.f, 20.f, 30.f, 40.f},
            });
        const auto childId = tree.emplaceChild<Surface>(stackId);
        auto *childLayout = tree.getLayout(childId);
        ASSERT_NE(childLayout, nullptr);
        childLayout->setWidth(80.f);
        childLayout->setHeight(40.f);
        childLayout->setMargin({5.f, 7.f, 11.f, 13.f});

        tree.performLayout();
        EXPECT_EQ(tree.getBounds(childId).center, glm::vec2(13.f, 39.f));
        EXPECT_EQ(tree.getBounds(childId).size, glm::vec2(80.f, 40.f));
        const auto *stack = tree.getWidget<StackContainer>(stackId);
        ASSERT_NE(stack, nullptr);
        EXPECT_TRUE(stack->traits().clipChildren);
        EXPECT_FALSE(stack->traits().hitTestVisible);

        WidgetRef<StackContainer> stackRef{tree, stackId};
        ASSERT_TRUE(stackRef.update([](StackContainer &value) {
            value.setAlignment(StackAlignment::start, StackAlignment::center);
        }));
        tree.performLayout();
        EXPECT_EQ(tree.getBounds(childId).center, glm::vec2(-57.f, -13.f));
        EXPECT_EQ(tree.getBounds(childId).size, glm::vec2(80.f, 40.f));
    }

    TEST(StackContainerTests,
         LaterChildrenAreTopmostUnlessExplicitZOverridesThem) {
        WidgetTree tree;
        tree.setViewportSize({240.f, 120.f});
        size_t backActivations = 0;
        size_t frontActivations = 0;
        const auto stackId = tree.emplaceWidget<StackContainer>();
        const auto backId = tree.emplaceChild<Button>(
            stackId, "Back", [&backActivations] { ++backActivations; });
        const auto frontId = tree.emplaceChild<Button>(
            stackId, "Front", [&frontActivations] { ++frontActivations; });
        tree.performLayout();
        EXPECT_EQ(tree.getBounds(backId).center,
                  tree.getBounds(stackId).center);
        EXPECT_EQ(tree.getBounds(backId).size, tree.getBounds(stackId).size);
        EXPECT_EQ(tree.getBounds(frontId).center,
                  tree.getBounds(stackId).center);
        EXPECT_EQ(tree.getBounds(frontId).size, tree.getBounds(stackId).size);

        const glm::vec2 pointer = tree.getViewportSize() * 0.5f;
        static_cast<void>(tree.dispatchEvent(Input::MouseButtonEvent{
            .button = MouseButton::left,
            .action = MouseButtonAction::press,
            .pos = pointer,
        }));
        static_cast<void>(tree.dispatchEvent(Input::MouseButtonEvent{
            .button = MouseButton::left,
            .action = MouseButtonAction::release,
            .pos = pointer,
        }));
        EXPECT_EQ(backActivations, 0);
        EXPECT_EQ(frontActivations, 1);

        tree.getLayout(backId)->setZVal(1.f);
        tree.performLayout();
        static_cast<void>(tree.dispatchEvent(Input::MouseButtonEvent{
            .button = MouseButton::left,
            .action = MouseButtonAction::press,
            .pos = pointer,
        }));
        static_cast<void>(tree.dispatchEvent(Input::MouseButtonEvent{
            .button = MouseButton::left,
            .action = MouseButtonAction::release,
            .pos = pointer,
        }));
        EXPECT_EQ(backActivations, 1);
        EXPECT_EQ(frontActivations, 1);
    }

    TEST(StackContainerTests, NormalizesInsetsAndHonorsChildMaximumSize) {
        WidgetTree tree;
        tree.setViewportSize({200.f, 100.f});
        const auto stackId =
            tree.emplaceWidget<StackContainer>(StackContainerOptions{
                .padding = {std::numeric_limits<float>::quiet_NaN(),
                            std::numeric_limits<float>::infinity(),
                            -5.f,
                            20.f},
            });
        const auto childId = tree.emplaceChild<Surface>(stackId);
        tree.getLayout(childId)->setMaxSize({80.f, 40.f});

        tree.performLayout();
        EXPECT_EQ(tree.getBounds(childId).center, glm::vec2(10.f, 0.f));
        EXPECT_EQ(tree.getBounds(childId).size, glm::vec2(80.f, 40.f));
    }

    TEST(UIComposerTests, FixedGapFollowsItsFlexParentMainAxis) {
        WidgetTree tree;
        tree.setViewportSize({400.f, 300.f});
        const auto root =
            tree.emplaceWidget<FlexContainer>(FlexContainerOptions{
                .stretchWidth = false, .stretchHeight = false});
        UIComposer ui{tree, root};

        const auto first = ui.surface();
        const auto gap = ui.gap(12.f);
        const auto second = ui.surface();
        ASSERT_TRUE(first && gap && second);
        ASSERT_TRUE(first.updateLayout([](LayoutNode &layout) {
            layout.setWidth(20.f);
            layout.setHeight(10.f);
        }));
        ASSERT_TRUE(second.updateLayout([](LayoutNode &layout) {
            layout.setWidth(20.f);
            layout.setHeight(10.f);
        }));

        tree.performLayout();
        EXPECT_EQ(gap.get()->typeName(), "Gap");
        EXPECT_EQ(tree.getBounds(gap.id()).size, glm::vec2(12.f, 0.f));
        EXPECT_FLOAT_EQ(tree.getBounds(second.id()).topLeft().x -
                            tree.getBounds(first.id()).bottomRight().x,
                        12.f);

        tree.getLayout(root)->setDirection(LayoutDirection::vertical);
        tree.performLayout();
        EXPECT_EQ(tree.getBounds(gap.id()).size, glm::vec2(0.f, 12.f));
        EXPECT_FLOAT_EQ(tree.getBounds(second.id()).topLeft().y -
                            tree.getBounds(first.id()).bottomRight().y,
                        12.f);
    }

    TEST(UIComposerTests, ContainerGapSpacesEveryAdjacentChild) {
        WidgetTree tree;
        tree.setViewportSize({400.f, 300.f});
        const auto root =
            tree.emplaceWidget<FlexContainer>(FlexContainerOptions{
                .gap = 7.f, .stretchWidth = false, .stretchHeight = false});
        UIComposer ui{tree, root};

        const auto first = ui.surface();
        const auto second = ui.surface();
        ASSERT_TRUE(first.updateLayout([](LayoutNode &layout) {
            layout.setWidth(20.f);
            layout.setHeight(10.f);
        }));
        ASSERT_TRUE(second.updateLayout([](LayoutNode &layout) {
            layout.setWidth(20.f);
            layout.setHeight(10.f);
        }));

        tree.performLayout();
        EXPECT_FLOAT_EQ(tree.getLayout(root)->getGap(), 7.f);
        EXPECT_FLOAT_EQ(tree.getBounds(second.id()).topLeft().x -
                            tree.getBounds(first.id()).bottomRight().x,
                        7.f);
    }

    TEST(UIComposerTests, FlexibleSpacerCanEstablishRowHeight) {
        WidgetTree tree;
        tree.setViewportSize({400.f, 300.f});
        const auto root = tree.emplaceWidget<FlexContainer>();
        UIComposer rootComposer{tree, root};
        const auto row = rootComposer.row(FlexContainerOptions{
            .stretchWidth = false, .stretchHeight = false});
        ASSERT_TRUE(row.updateLayout(
            [](LayoutNode &layout) { layout.setWidth(120.f); }));
        UIComposer ui{tree, row.id()};

        const auto first = ui.surface();
        const auto spacer =
            ui.spacer(SpacerOptions{.flex = 2.f, .minimumSize = {0.f, 36.f}});
        const auto second = ui.surface();
        for (const auto surface : {first, second}) {
            ASSERT_TRUE(surface.updateLayout([](LayoutNode &layout) {
                layout.setWidth(20.f);
                layout.setHeight(10.f);
            }));
        }

        tree.performLayout();
        EXPECT_FLOAT_EQ(tree.getLayout(spacer.id())->getFlexGrow(), 2.f);
        EXPECT_EQ(tree.getLayout(spacer.id())->getMinSize(),
                  glm::vec2(0.f, 36.f));
        EXPECT_FLOAT_EQ(tree.getBounds(row.id()).size.y, 36.f);
        EXPECT_GE(tree.getBounds(spacer.id()).size.y, 36.f);
        EXPECT_FLOAT_EQ(tree.getBounds(spacer.id()).size.x, 80.f);
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
        EXPECT_GE(countWidgetType(tree, "Gap"), 1);
        EXPECT_EQ(countWidgetType(tree, "StackContainer"), 1);
        EXPECT_EQ(countWidgetType(tree, "TabBar"), 1);
        EXPECT_EQ(countWidgetType(tree, "MenuBar"), 1);
        EXPECT_EQ(countWidgetType(tree, "DockSpace"), 1);
        EXPECT_EQ(countWidgetType(tree, "DockPanel"), 6);
        EXPECT_EQ(countWidgetType(tree, "FocusScope"), 1);
        EXPECT_EQ(countWidgetType(tree, "CheckBox"), 1);
        EXPECT_EQ(countWidgetType(tree, "ToggleSwitch"), 1);
        EXPECT_EQ(countWidgetType(tree, "RadioButton"), 2);
        EXPECT_EQ(countWidgetType(tree, "Slider"), 1);
        EXPECT_EQ(countWidgetType(tree, "Dropdown"), 1);
        EXPECT_GE(countWidgetType(tree, "TextBox"), 2);
        EXPECT_EQ(countWidgetType(tree, "Autocomplete"), 1);
        EXPECT_EQ(countWidgetType(tree, "Tooltip"), 1);
        EXPECT_EQ(countWidgetType(tree, "ContextMenuRegion"), 1);

        ASSERT_NE(demo.get()->tabs(), nullptr);
        EXPECT_EQ(demo.get()->tabs()->size(), 3);
        ASSERT_NE(demo.get()->menus(), nullptr);
        EXPECT_EQ(demo.get()->menus()->menus().size(), 4);
        EXPECT_TRUE(demo.get()->menus()->validate());
        auto dockRef = demo.get()->dockSpace();
        ASSERT_TRUE(dockRef);
        EXPECT_EQ(dockRef.get()->model().itemCount(), 6);
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
