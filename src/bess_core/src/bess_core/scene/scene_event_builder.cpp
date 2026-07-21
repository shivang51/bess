#include "bess_core/scene/scene_event_builder.h"
#include "bess_core/sub_systems/input_sub_system.h"

namespace Bess::Canvas {
    std::vector<SceneEvent> SceneEventBuilder::buildFrameEvents(
        const InputSubSystem &inputSystem,
        const std::shared_ptr<Camera> &camera,
        const Core::Viewport::ViewportTransform &viewportTransform) {
        std::vector<SceneEvent> events;
        if (!camera) {
            return events;
        }

        const bool isCtrlPressed = inputSystem.isCtrlPressed();
        const bool isShiftPressed = inputSystem.isShiftPressed();
        const bool isAltPressed = inputSystem.isAltPressed();
        const auto &frameInputState = inputSystem.getFrameInpState();

        auto toVpPos = [&viewportTransform](const glm::vec2 &pos) -> glm::vec2 {
            return {pos.x - viewportTransform.pos.x,
                    pos.y - viewportTransform.pos.y};
        };

        if (frameInputState.hasMouseMoved) {
            const auto &mouseMoveState = inputSystem.getMouseMoveState();
            const auto viewportPos = toVpPos(mouseMoveState.pos);

            SceneEvent::Data data;
            data.mouseMove = {
                .pos = camera->toWorldPos(viewportPos),
                .delta = mouseMoveState.delta,
                .viewportPos = viewportPos,
            };

            events.emplace_back(SceneEvent{
                .type = SceneEvent::Type::mouseMove,
                .data = data,
                .isCtrlPressed = isCtrlPressed,
                .isShiftPressed = isShiftPressed,
                .isAltPressed = isAltPressed,
            });
        }

        if (frameInputState.hasMouseWheelScrolled) {
            const auto &mouseWheelState = inputSystem.getMouseWheelState();
            const auto viewportPos = toVpPos(mouseWheelState.pos);

            SceneEvent::Data data;
            data.mouseWheel = {
                .pos = camera->toWorldPos(viewportPos),
                .delta = mouseWheelState.offset,
                .viewportPos = viewportPos,
            };

            events.emplace_back(SceneEvent{
                .type = SceneEvent::Type::mouseWheel,
                .data = data,
                .isCtrlPressed = isCtrlPressed,
                .isShiftPressed = isShiftPressed,
                .isAltPressed = isAltPressed,
            });
        }

        if (frameInputState.hasMouseBtnEvent) {
            const auto &mouseBtnState = frameInputState.mouseBtnState;

            SceneEvent::Data data;
            data.mouseButton = {
                .button = mouseBtnState.button,
                .action = mouseBtnState.action,
                .pos = camera->toWorldPos(toVpPos(mouseBtnState.pos)),
            };

            events.emplace_back(SceneEvent{
                .type = SceneEvent::Type::mouseButton,
                .data = data,
                .isCtrlPressed = isCtrlPressed,
                .isShiftPressed = isShiftPressed,
                .isAltPressed = isAltPressed,
            });
        }

        for (const auto &keyboardEvent : frameInputState.keyboardEvents) {
            SceneEvent::Data data;
            if (const auto *keyEvent =
                    std::get_if<Input::KeyEvent>(&keyboardEvent)) {
                data.keyPress = {
                    .keycode = keyEvent->key,
                    .action = keyEvent->action,
                };

                events.emplace_back(SceneEvent{
                    .type = SceneEvent::Type::key,
                    .data = data,
                    .isCtrlPressed = isCtrlPressed,
                    .isShiftPressed = isShiftPressed,
                    .isAltPressed = isAltPressed,
                });
            } else if (const auto *textEvent =
                           std::get_if<Input::TextInputEvent>(
                               &keyboardEvent)) {
                data.textInput = {
                    .codepoint = textEvent->codepoint,
                };

                events.emplace_back(SceneEvent{
                    .type = SceneEvent::Type::textInput,
                    .data = data,
                    .isCtrlPressed = isCtrlPressed,
                    .isShiftPressed = isShiftPressed,
                    .isAltPressed = isAltPressed,
                });
            }
        }

        return events;
    }
} // namespace Bess::Canvas
