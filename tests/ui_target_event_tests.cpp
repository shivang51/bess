#include "ui_core.h"

#include <gtest/gtest.h>

namespace {

    using namespace Bess;

    TEST(UITargetEvents, PreservesEventOrderAndTypedPayloads) {
        UI::UITarget target;
        const Input::Modifiers keyModifiers{
            .control = true,
            .shift = true,
        };

        target.enqueueEvent(
            UI::UITargetResizeEvent{.width = 1280, .height = 720});
        target.enqueueEvent(Input::MouseButtonEvent{
            .button = MouseButton::left,
            .action = MouseButtonAction::press,
            .pos = {30.f, 40.f},
        });
        target.enqueueEvent(Input::Event{
            Input::KeyEvent{.key = KeyCode::a, .action = KeyAction::press},
            keyModifiers,
        });
        target.enqueueEvent(Input::TextInputEvent{.codepoint = U'a'},
                            keyModifiers);
        target.enqueueEvent(
            Input::TextCompositionEvent{
                .phase = Input::TextCompositionPhase::update,
                .text = "candidate",
                .selectionStart = 4,
                .selectionLength = 2,
            },
            keyModifiers);

        target.update(TimeMs{0});

        const auto events = target.getFrameEvents();
        ASSERT_EQ(events.size(), 5);

        const auto *resize = events[0].getIf<UI::UITargetResizeEvent>();
        ASSERT_NE(resize, nullptr);
        EXPECT_EQ(resize->width, 1280);
        EXPECT_EQ(resize->height, 720);
        EXPECT_EQ(target.getRect().size, glm::vec2(1280.f, 720.f));
        EXPECT_EQ(target.getWidgetTree().getViewportSize(),
                  glm::vec2(1280.f, 720.f));

        const auto *mouseButton = events[1].getIf<Input::MouseButtonEvent>();
        ASSERT_NE(mouseButton, nullptr);
        EXPECT_EQ(mouseButton->button, MouseButton::left);
        EXPECT_EQ(mouseButton->action, MouseButtonAction::press);
        EXPECT_EQ(mouseButton->pos, glm::vec2(30.f, 40.f));

        const auto *key = events[2].getIf<Input::KeyEvent>();
        ASSERT_NE(key, nullptr);
        EXPECT_EQ(key->key, KeyCode::a);
        EXPECT_EQ(key->action, KeyAction::press);
        EXPECT_EQ(events[2].modifiers, keyModifiers);

        const auto *text = events[3].getIf<Input::TextInputEvent>();
        ASSERT_NE(text, nullptr);
        EXPECT_EQ(text->codepoint, U'a');
        EXPECT_EQ(events[3].modifiers, keyModifiers);

        const auto *composition =
            events[4].getIf<Input::TextCompositionEvent>();
        ASSERT_NE(composition, nullptr);
        EXPECT_EQ(composition->phase, Input::TextCompositionPhase::update);
        EXPECT_EQ(composition->text, "candidate");
        EXPECT_EQ(composition->selectionStart, 4u);
        EXPECT_EQ(composition->selectionLength, 2u);
        EXPECT_EQ(events[4].modifiers, keyModifiers);

        target.enqueueEvent(
            UI::UITargetResizeEvent{.width = 1920, .height = 1080});
        target.update(TimeMs{0});
        EXPECT_EQ(target.getInputContext().modifiers, keyModifiers);
        EXPECT_EQ(target.getRect().size, glm::vec2(1920.f, 1080.f));
    }

    TEST(UITargetEvents, BuildsFramePointerStateFromEveryQueuedEvent) {
        UI::UITarget target;

        target.enqueueEvent(Input::MouseMoveEvent{.pos = {1.f, 2.f}});
        target.update(TimeMs{0});

        auto events = target.getFrameEvents();
        ASSERT_EQ(events.size(), 1);
        ASSERT_NE(events[0].getIf<Input::MouseMoveEvent>(), nullptr);
        EXPECT_EQ(events[0].getIf<Input::MouseMoveEvent>()->delta,
                  glm::vec2(0.f));

        target.enqueueEvent(Input::MouseMoveEvent{.pos = {3.f, 5.f}});
        target.enqueueEvent(Input::MouseMoveEvent{.pos = {6.f, 9.f}});
        target.enqueueEvent(Input::MouseWheelEvent{
            .pos = {6.f, 9.f},
            .offset = {1.f, -2.f},
        });
        target.update(TimeMs{0});

        events = target.getFrameEvents();
        ASSERT_EQ(events.size(), 3);
        EXPECT_EQ(events[0].getIf<Input::MouseMoveEvent>()->delta,
                  glm::vec2(2.f, 3.f));
        EXPECT_EQ(events[1].getIf<Input::MouseMoveEvent>()->delta,
                  glm::vec2(3.f, 4.f));

        const auto &input = target.getInputContext();
        EXPECT_EQ(input.mousePos, glm::vec2(6.f, 9.f));
        EXPECT_EQ(input.mouseDelta, glm::vec2(5.f, 7.f));
        EXPECT_EQ(input.mouseWheelDelta, glm::vec2(1.f, -2.f));

        target.update(TimeMs{0});
        EXPECT_TRUE(target.getFrameEvents().empty());
        EXPECT_EQ(target.getInputContext().mouseDelta, glm::vec2(0.f));
        EXPECT_EQ(target.getInputContext().mouseWheelDelta, glm::vec2(0.f));
    }

    TEST(UITargetEvents, LaysOutNewWidgetTreeBeforeFirstInputBatch) {
        UI::UITarget target;
        target.resize({200.f, 100.f});
        size_t activations = 0;
        const auto button = target.getWidgetTree().emplaceWidget<UI::Button>(
            "Open", [&activations] { ++activations; });
        ASSERT_TRUE(button);

        target.enqueueEvent(Input::MouseButtonEvent{
            .button = MouseButton::left,
            .action = MouseButtonAction::press,
            .pos = {100.f, 50.f},
        });
        target.enqueueEvent(Input::MouseButtonEvent{
            .button = MouseButton::left,
            .action = MouseButtonAction::release,
            .pos = {100.f, 50.f},
        });
        target.update(TimeMs{0});

        EXPECT_EQ(activations, 1);
        EXPECT_EQ(target.getWidgetTree().getFocusedWidget(), button);
    }

} // namespace
