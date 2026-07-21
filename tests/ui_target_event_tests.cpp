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

        target.enqueueEvent(UI::UITargetResizeEvent{.width = 1280,
                                                     .height = 720});
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

        target.update(TimeMs{0});

        const auto events = target.getFrameEvents();
        ASSERT_EQ(events.size(), 4);

        const auto *resize = events[0].getIf<UI::UITargetResizeEvent>();
        ASSERT_NE(resize, nullptr);
        EXPECT_EQ(resize->width, 1280);
        EXPECT_EQ(resize->height, 720);

        const auto *mouseButton =
            events[1].getIf<Input::MouseButtonEvent>();
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

        target.enqueueEvent(
            UI::UITargetResizeEvent{.width = 1920, .height = 1080});
        target.update(TimeMs{0});
        EXPECT_EQ(target.getInputContext().modifiers, keyModifiers);
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

} // namespace
