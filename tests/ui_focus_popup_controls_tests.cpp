#include "ui_core.h"

#include <gtest/gtest.h>

#include <memory>
#include <string>
#include <vector>

namespace {
    using namespace Bess;
    using namespace Bess::UI;

    UIEvent key(KeyCode code, bool shift = false, bool control = false) {
        return UIEvent{Input::KeyEvent{.key = code, .action = KeyAction::press},
                       {.control = control, .shift = shift}};
    }

    glm::vec2 surfacePoint(const WidgetTree &tree, WidgetId widget) {
        return tree.getBounds(widget).center + tree.getViewportSize() * 0.5f;
    }

    struct PopupFixture {
        WidgetTree tree;
        UIViewHost views{tree};
        PopupHost popups{views};

        PopupFixture() {
            tree.setViewportSize({400.f, 300.f});
        }
    };

    class ButtonView final : public UIView {
      public:
        explicit ButtonView(WidgetId &button) : m_button(button) {
        }

        void compose(UIComposer &ui) override {
            m_button = ui.button("Modal action").id();
        }

      private:
        WidgetId &m_button;
    };

    TEST(FocusScopeTests,
         TraversesInTreeOrderTrapsDirectFocusAndRestoresPreviousFocus) {
        WidgetTree tree;
        tree.setViewportSize({400.f, 300.f});
        const auto root = tree.emplaceWidget<FlexContainer>(
            FlexContainerOptions{.direction = LayoutDirection::vertical});
        const auto outside = tree.emplaceChild<Button>(root, "Outside");
        ASSERT_TRUE(tree.setFocus(outside));

        const auto scope = tree.emplaceChild<FocusScope>(
            root,
            FocusScopeOptions{.focus = {.trapFocus = true,
                                        .autoFocus = false,
                                        .restoreFocus = true}});
        const auto first = tree.emplaceChild<Button>(scope, "First");
        const auto second = tree.emplaceChild<Button>(scope, "Second");
        auto *focusScope = tree.getWidget<FocusScope>(scope);
        ASSERT_NE(focusScope, nullptr);
        ASSERT_TRUE(focusScope->setDefaultFocus(second));
        ASSERT_TRUE(focusScope->focusDefault());
        EXPECT_EQ(tree.getFocusedWidget(), second);
        EXPECT_FALSE(tree.setFocus(outside));

        EXPECT_TRUE(tree.dispatchEvent(key(KeyCode::tab)).handled);
        EXPECT_EQ(tree.getFocusedWidget(), first);
        EXPECT_TRUE(tree.dispatchEvent(key(KeyCode::tab, true)).handled);
        EXPECT_EQ(tree.getFocusedWidget(), second);

        EXPECT_TRUE(tree.removeWidget(scope));
        EXPECT_EQ(tree.getFocusedWidget(), outside);
    }

    TEST(FocusScopeTests, AutoFocusIsDeferredUntilChildrenExist) {
        WidgetTree tree;
        tree.setViewportSize({200.f, 100.f});
        const auto scope = tree.emplaceWidget<FocusScope>(FocusScopeOptions{
            .focus = {
                .trapFocus = false, .autoFocus = true, .restoreFocus = true}});
        const auto child = tree.emplaceChild<Button>(scope, "Default");
        EXPECT_FALSE(tree.getFocusedWidget());
        tree.performLayout();
        EXPECT_EQ(tree.getFocusedWidget(), child);
    }

    TEST(FocusScopeTests, ModalViewTrapsFocusAndRestoresItsOpener) {
        WidgetTree tree;
        tree.setViewportSize({400.f, 300.f});
        UIViewHost views{tree};
        const auto outside = tree.emplaceWidget<Button>("Outside");
        ASSERT_TRUE(tree.setFocus(outside));

        WidgetId modalButton;
        auto modal = views.mountModal<ButtonView>(modalButton);
        ASSERT_TRUE(modal);
        tree.performLayout();

        EXPECT_EQ(tree.getFocusedWidget(), modalButton);
        EXPECT_FALSE(tree.setFocus(outside));
        EXPECT_TRUE(tree.dispatchEvent(key(KeyCode::tab)).handled);
        EXPECT_EQ(tree.getFocusedWidget(), modalButton);

        ASSERT_TRUE(modal.unmount());
        views.flushPendingUnmounts();
        EXPECT_EQ(tree.getFocusedWidget(), outside);
    }

    TEST(FocusScopeTests,
         ClosingModalClosesAnchoredPopupAndRestoresFocusChain) {
        WidgetTree tree;
        tree.setViewportSize({400.f, 300.f});
        UIViewHost views{tree};
        PopupHost popups{views};
        const auto outside = tree.emplaceWidget<Button>("Outside");
        ASSERT_TRUE(tree.setFocus(outside));

        WidgetId modalButton;
        auto modal = views.mountModal<ButtonView>(modalButton);
        tree.performLayout();
        ASSERT_EQ(tree.getFocusedWidget(), modalButton);

        auto popup = popups.open(
            {.anchor = PopupAnchor::forWidget(modalButton),
             .focus = {.trapFocus = true,
                       .autoFocus = true,
                       .restoreFocus = true}},
            [](UIComposer &content) { content.button("Popup action"); });
        ASSERT_TRUE(popup);
        tree.performLayout();
        ASSERT_NE(tree.getFocusedWidget(), modalButton);

        ASSERT_TRUE(modal.unmount());
        views.flushPendingUnmounts();
        EXPECT_FALSE(popup.isOpen());
        EXPECT_EQ(tree.getFocusedWidget(), outside);
    }

    TEST(PopupPlacementTests, FlipsAndClampsInsideViewportMargins) {
        AnchoredPopupOptions options{
            .preferredSide = PopupSide::bottom,
            .alignment = PopupAlignment::end,
            .gap = 4.f,
            .viewportMargin = 8.f,
        };
        const auto result = PopupPlacementSolver::calculate(
            {.center = {}, .size = {200.f, 100.f}},
            {.center = {75.f, 38.f}, .size = {20.f, 16.f}},
            {80.f, 35.f},
            options);
        EXPECT_EQ(result.side, PopupSide::top);
        EXPECT_GE(result.bounds.topLeft().x, -92.f);
        EXPECT_LE(result.bounds.bottomRight().x, 92.f);
        EXPECT_GE(result.bounds.topLeft().y, -42.f);
        EXPECT_LE(result.bounds.bottomRight().y, 42.f);
    }

    TEST(PopupHostTests, EscapeDismissesAndRestoresFocus) {
        PopupFixture fixture;
        const auto root = fixture.tree.emplaceWidget<FlexContainer>();
        const auto anchor = fixture.tree.emplaceChild<Button>(root, "Anchor");
        ASSERT_TRUE(fixture.tree.setFocus(anchor));

        auto popup = fixture.popups.open(
            {.anchor = PopupAnchor::forWidget(anchor),
             .focus = {.trapFocus = true,
                       .autoFocus = true,
                       .restoreFocus = true}},
            [](UIComposer &content) { content.button("Popup action"); });
        ASSERT_TRUE(popup);
        fixture.tree.performLayout();
        const auto popupChildren =
            fixture.tree.getChildren(popup.content().id());
        ASSERT_EQ(popupChildren.size(), 1u);
        EXPECT_EQ(fixture.tree.getFocusedWidget(), popupChildren.front());

        const auto result = fixture.tree.dispatchEvent(key(KeyCode::escape));
        fixture.views.flushPendingUnmounts();
        EXPECT_TRUE(result.handled);
        EXPECT_FALSE(popup.isOpen());
        EXPECT_EQ(fixture.tree.getFocusedWidget(), anchor);
    }

    TEST(PopupHostTests, EscapeDismissesWhenFocusRemainsOnAnchor) {
        PopupFixture fixture;
        const auto root = fixture.tree.emplaceWidget<FlexContainer>();
        const auto anchor = fixture.tree.emplaceChild<Button>(root, "Anchor");
        ASSERT_TRUE(fixture.tree.setFocus(anchor));

        auto popup = fixture.popups.open(
            {.anchor = PopupAnchor::forWidget(anchor),
             .focus = {.trapFocus = false,
                       .autoFocus = false,
                       .restoreFocus = true}},
            [](UIComposer &content) { content.label("No focus request"); });
        ASSERT_TRUE(popup);
        fixture.tree.performLayout();
        ASSERT_EQ(fixture.tree.getFocusedWidget(), anchor);

        const auto result = fixture.tree.dispatchEvent(key(KeyCode::escape));
        fixture.views.flushPendingUnmounts();
        EXPECT_TRUE(result.handled);
        EXPECT_FALSE(popup.isOpen());
        EXPECT_EQ(fixture.tree.getFocusedWidget(), anchor);
    }

    TEST(PopupHostTests, OutsidePressDismissesButAnchorCanPassThrough) {
        PopupFixture fixture;
        size_t activations = 0;
        const auto root = fixture.tree.emplaceWidget<FlexContainer>();
        const auto anchor = fixture.tree.emplaceChild<Button>(
            root, "Anchor", [&activations] { ++activations; });
        auto popup = fixture.popups.open(
            {.anchor = PopupAnchor::forWidget(anchor),
             .passThroughAnchor = true,
             .focus = {.trapFocus = false,
                       .autoFocus = false,
                       .restoreFocus = true}},
            [](UIComposer &content) { content.label("Popup"); });
        fixture.tree.performLayout();

        const auto point = surfacePoint(fixture.tree, anchor);
        static_cast<void>(fixture.tree.dispatchEvent(Input::MouseButtonEvent{
            .button = MouseButton::left,
            .action = MouseButtonAction::press,
            .pos = point,
        }));
        static_cast<void>(fixture.tree.dispatchEvent(Input::MouseButtonEvent{
            .button = MouseButton::left,
            .action = MouseButtonAction::release,
            .pos = point,
        }));
        EXPECT_EQ(activations, 1u);
        EXPECT_TRUE(popup.isOpen());

        static_cast<void>(fixture.tree.dispatchEvent(Input::MouseButtonEvent{
            .button = MouseButton::left,
            .action = MouseButtonAction::press,
            .pos = {390.f, 290.f},
        }));
        fixture.views.flushPendingUnmounts();
        EXPECT_FALSE(popup.isOpen());
    }

    TEST(ValueControlTests, KeyboardAndPointerInteractionsUpdateModels) {
        WidgetTree tree;
        tree.setViewportSize({500.f, 300.f});
        const auto root = tree.emplaceWidget<FlexContainer>(
            FlexContainerOptions{.direction = LayoutDirection::vertical});
        const auto checkModel = std::make_shared<CheckStateModel>();
        const auto checkbox = tree.addWidget(
            std::make_unique<CheckBox>("Check", checkModel), root);
        const auto range = std::make_shared<RangeModel>(0.0, 10.0, 5.0, 1.0);
        const auto slider =
            tree.addWidget(std::make_unique<Slider>(range), root);
        const auto radios = std::make_shared<RadioGroupModel>();
        const auto first = tree.addWidget(
            std::make_unique<RadioButton>("First", radios), root);
        const auto second = tree.addWidget(
            std::make_unique<RadioButton>("Second", radios), root);
        tree.performLayout();

        ASSERT_TRUE(tree.setFocus(checkbox));
        static_cast<void>(tree.dispatchEvent(key(KeyCode::space)));
        static_cast<void>(tree.dispatchEvent(UIEvent{Input::KeyEvent{
            .key = KeyCode::space, .action = KeyAction::release}}));
        EXPECT_EQ(checkModel->value(), CheckState::checked);

        ASSERT_TRUE(tree.setFocus(slider));
        static_cast<void>(tree.dispatchEvent(key(KeyCode::arrowRight)));
        EXPECT_DOUBLE_EQ(range->value(), 6.0);

        const auto sliderBounds = tree.getBounds(slider);
        const auto toSurface = [&tree](glm::vec2 point) {
            return point + tree.getViewportSize() * 0.5f;
        };
        static_cast<void>(tree.dispatchEvent(Input::MouseButtonEvent{
            .button = MouseButton::left,
            .action = MouseButtonAction::press,
            .pos = toSurface(sliderBounds.topLeft()),
        }));
        EXPECT_EQ(tree.getPointerCapture(), slider);
        static_cast<void>(tree.dispatchEvent(Input::MouseMoveEvent{
            .pos = toSurface(sliderBounds.bottomRight() + glm::vec2{20.f, 0.f}),
        }));
        EXPECT_DOUBLE_EQ(range->value(), 10.0);
        static_cast<void>(tree.dispatchEvent(Input::MouseButtonEvent{
            .button = MouseButton::left,
            .action = MouseButtonAction::release,
            .pos = toSurface(sliderBounds.bottomRight() + glm::vec2{20.f, 0.f}),
        }));
        EXPECT_FALSE(tree.getPointerCapture());

        ASSERT_TRUE(
            radios->select(tree.getWidget<RadioButton>(first)->radioId()));
        ASSERT_TRUE(tree.setFocus(first));
        static_cast<void>(tree.dispatchEvent(key(KeyCode::arrowDown)));
        EXPECT_EQ(radios->value(),
                  tree.getWidget<RadioButton>(second)->radioId());
        EXPECT_EQ(tree.getFocusedWidget(), second);
    }

    TEST(ValueControlTests, RangeAndStepChangesNotifyWithoutValueChanges) {
        RangeModel range{0.0, 10.0, 4.0, 1.0};
        std::vector<RangeChange> changes;
        auto connection =
            range.changed().connect([&changes](const RangeChange &change) {
                changes.push_back(change);
            });

        ASSERT_TRUE(range.setRange(0.0, 20.0));
        ASSERT_EQ(changes.size(), 1u);
        EXPECT_EQ(changes.back().kind, RangeChangeKind::range);
        EXPECT_DOUBLE_EQ(changes.back().previousValue, 4.0);
        EXPECT_DOUBLE_EQ(changes.back().value, 4.0);

        ASSERT_TRUE(range.setStep(2.0));
        ASSERT_EQ(changes.size(), 2u);
        EXPECT_EQ(changes.back().kind, RangeChangeKind::step);
        EXPECT_DOUBLE_EQ(changes.back().value, 4.0);
    }

    TEST(PopupControlTests,
         DropdownTooltipContextMenuAndAutocompleteUseSharedPopupHost) {
        PopupFixture fixture;
        const auto root = fixture.tree.emplaceWidget<FlexContainer>(
            FlexContainerOptions{.direction = LayoutDirection::vertical});

        const auto dropdownModel = std::make_shared<DropdownModel>();
        static_cast<void>(dropdownModel->add("One"));
        static_cast<void>(dropdownModel->add("Two"));
        const auto dropdownId = fixture.tree.addWidget(
            std::make_unique<Dropdown>(dropdownModel), root);

        const auto tooltipId = fixture.tree.addWidget(
            std::make_unique<Tooltip>("Helpful",
                                      TooltipOptions{.delayMs = 0.f}),
            root);
        fixture.tree.emplaceChild<Button>(tooltipId, "Hover");

        const auto textModel = std::make_shared<TextEditModel>("a");
        const auto autocompleteId = fixture.tree.addWidget(
            std::make_unique<Autocomplete>(
                textModel,
                [](std::string_view) {
                    return std::vector<AutocompleteItem>{
                        {.label = "Alpha", .replacement = "Alpha"},
                        {.label = "Alpine", .replacement = "Alpine"},
                    };
                }),
            root);
        fixture.tree.performLayout();

        auto *dropdown = fixture.tree.getWidget<Dropdown>(dropdownId);
        ASSERT_NE(dropdown, nullptr);
        EXPECT_TRUE(dropdown->open());
        EXPECT_TRUE(dropdown->isOpen());
        EXPECT_TRUE(dropdown->close());

        const auto tooltipPoint = surfacePoint(fixture.tree, tooltipId);
        static_cast<void>(fixture.tree.dispatchEvent(
            Input::MouseMoveEvent{.pos = tooltipPoint}));
        fixture.tree.update(TimeMs{1});
        EXPECT_TRUE(fixture.tree.getWidget<Tooltip>(tooltipId)->isOpen());
        EXPECT_TRUE(fixture.tree.getWidget<Tooltip>(tooltipId)->hide());

        auto *autocomplete =
            fixture.tree.getWidget<Autocomplete>(autocompleteId);
        ASSERT_NE(autocomplete, nullptr);
        ASSERT_TRUE(fixture.tree.setFocus(autocomplete->textBoxId()));
        fixture.tree.update(TimeMs{0});
        ASSERT_TRUE(autocomplete->isOpen());
        static_cast<void>(fixture.tree.dispatchEvent(key(KeyCode::arrowDown)));
        static_cast<void>(fixture.tree.dispatchEvent(key(KeyCode::enter)));
        EXPECT_EQ(textModel->text(), "Alpine");
        EXPECT_FALSE(autocomplete->isOpen());

        auto menu = std::make_shared<MenuModel>();
        size_t invoked = 0;
        size_t nestedInvoked = 0;
        const MenuId menuId = menu->addMenu(
            {.name = "Context",
             .items = {{.name = "Run", .activated = [&invoked] { ++invoked; }},
                       {.name = "More",
                        .children = {
                            {.name = "Nested", .activated = [&nestedInvoked] {
                                 ++nestedInvoked;
                             }}}}}});
        auto context = ContextMenu::open(
            fixture.popups, menu, menuId, PopupAnchor::forPoint({0.f, 0.f}));
        ASSERT_TRUE(context.isOpen());
        fixture.tree.performLayout();
        EXPECT_TRUE(fixture.tree.dispatchEvent(key(KeyCode::enter)).handled);
        fixture.views.flushPendingUnmounts();
        EXPECT_FALSE(context.isOpen());
        EXPECT_EQ(invoked, 1u);

        context = ContextMenu::open(
            fixture.popups, menu, menuId, PopupAnchor::forPoint({0.f, 0.f}));
        ASSERT_TRUE(context.isOpen());
        fixture.tree.performLayout();
        EXPECT_TRUE(
            fixture.tree.dispatchEvent(key(KeyCode::arrowDown)).handled);
        EXPECT_TRUE(
            fixture.tree.dispatchEvent(key(KeyCode::arrowRight)).handled);
        EXPECT_TRUE(fixture.tree.dispatchEvent(key(KeyCode::enter)).handled);
        fixture.views.flushPendingUnmounts();
        EXPECT_FALSE(context.isOpen());
        EXPECT_EQ(nestedInvoked, 1u);
    }

    TEST(PopupControlTests, OversizedContextMenuConsumesWheelScrolling) {
        PopupFixture fixture;
        std::vector<MenuItem> items;
        for (size_t index = 0; index < 24; ++index) {
            items.push_back({.name = "Command " + std::to_string(index)});
        }

        auto context =
            ContextMenu::open(fixture.popups,
                              std::move(items),
                              {0.f, 0.f},
                              ContextMenuOptions{.maximumHeight = 80.f});
        ASSERT_TRUE(context);
        fixture.tree.performLayout();
        const auto children = fixture.tree.getChildren(context.content().id());
        ASSERT_EQ(children.size(), 1u);

        const auto result = fixture.tree.dispatchEvent(Input::MouseWheelEvent{
            .pos = surfacePoint(fixture.tree, children.front()),
            .offset = {0.f, -1.f},
        });
        EXPECT_TRUE(result.handled);
        EXPECT_EQ(result.target, children.front());
    }

} // namespace
