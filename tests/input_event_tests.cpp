#include "bess_core/input/input_event.h"
#include "bess_core/sub_systems/input_sub_system.h"

#include <gtest/gtest.h>

namespace {

    using namespace Bess;

    TEST(InputEvents, InputSubsystemReturnsNormalizedMouseEvents) {
        InputSubSystem input;
        input.onInit();

        auto firstMove =
            input.processEvent(Input::MouseMoveEvent{.pos = {10.f, 20.f}});
        ASSERT_NE(firstMove.getIf<Input::MouseMoveEvent>(), nullptr);
        EXPECT_EQ(firstMove.getIf<Input::MouseMoveEvent>()->delta,
                  glm::vec2(0.f));

        auto secondMove =
            input.processEvent(Input::MouseMoveEvent{.pos = {13.f, 25.f}});
        ASSERT_NE(secondMove.getIf<Input::MouseMoveEvent>(), nullptr);
        EXPECT_EQ(secondMove.getIf<Input::MouseMoveEvent>()->delta,
                  glm::vec2(3.f, 5.f));

        auto press = input.processEvent(Input::MouseButtonEvent{
            .button = MouseButton::left,
            .action = MouseButtonAction::press,
            .pos = {13.f, 25.f},
        });
        ASSERT_NE(press.getIf<Input::MouseButtonEvent>(), nullptr);
        EXPECT_EQ(press.getIf<Input::MouseButtonEvent>()->action,
                  MouseButtonAction::press);

        static_cast<void>(input.processEvent(Input::MouseButtonEvent{
            .button = MouseButton::left,
            .action = MouseButtonAction::release,
            .pos = {13.f, 25.f},
        }));
        auto secondPress = input.processEvent(Input::MouseButtonEvent{
            .button = MouseButton::left,
            .action = MouseButtonAction::press,
            .pos = {13.f, 25.f},
        });
        ASSERT_NE(secondPress.getIf<Input::MouseButtonEvent>(), nullptr);
        EXPECT_EQ(secondPress.getIf<Input::MouseButtonEvent>()->action,
                  MouseButtonAction::doubleClick);
    }

    TEST(InputEvents, PreservesKeyboardEventOrderAndModifiers) {
        InputSubSystem input;
        input.onInit();
        const Input::Modifiers modifiers{.control = true, .shift = true};

        const auto key = input.processEvent(Input::Event{
            Input::KeyEvent{.key = KeyCode::a, .action = KeyAction::press},
            modifiers,
        });
        const auto text = input.processEvent(Input::Event{
            Input::TextInputEvent{.codepoint = U'A'},
            modifiers,
        });
        const auto composition = input.processEvent(Input::Event{
            Input::TextCompositionEvent{
                .phase = Input::TextCompositionPhase::update,
                .text = "preedit",
                .selectionStart = 2,
                .selectionLength = 3,
            },
            modifiers,
        });

        EXPECT_EQ(key.modifiers, modifiers);
        EXPECT_EQ(text.modifiers, modifiers);
        EXPECT_EQ(composition.modifiers, modifiers);

        const auto &frame = input.getFrameInpState();
        ASSERT_EQ(frame.keyboardEvents.size(), 3);
        EXPECT_TRUE(frame.hasTextCompositionEvent);
        EXPECT_TRUE(
            std::holds_alternative<Input::KeyEvent>(frame.keyboardEvents[0]));
        EXPECT_TRUE(std::holds_alternative<Input::TextInputEvent>(
            frame.keyboardEvents[1]));
        const auto *preedit =
            std::get_if<Input::TextCompositionEvent>(&frame.keyboardEvents[2]);
        ASSERT_NE(preedit, nullptr);
        EXPECT_EQ(preedit->text, "preedit");
        EXPECT_EQ(preedit->selectionStart, 2u);
        EXPECT_EQ(preedit->selectionLength, 3u);
    }

} // namespace
