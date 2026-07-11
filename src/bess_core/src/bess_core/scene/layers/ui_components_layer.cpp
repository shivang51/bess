#include "bess_core/scene/layers/ui_components_layer.h"
#include "bess_core/scene/scene_draw_context.h"
#include "bess_core/scene/scene_events.h"
#include "bess_core/scene/scene_state/components/scene_component.h"
#include "bess_core/scene/scene_ui/ui_scene_component.h"
#include "bess_core/viewport.h"
#include "pages/main_page/scene_components/scene_comp_types.h"

namespace Bess::Canvas {
    namespace {
        constexpr uint8_t kCursorPriorityReset = 5;
        constexpr uint8_t kCursorPriorityHover = 20;
        constexpr uint8_t kCursorPriorityCapture = 30;

        Events::MouseClickAction toSceneMouseAction(MouseButtonAction action) {
            switch (action) {
            case MouseButtonAction::press:
                return Events::MouseClickAction::press;
            case MouseButtonAction::release:
                return Events::MouseClickAction::release;
            case MouseButtonAction::doubleClick:
                return Events::MouseClickAction::doubleClick;
            case MouseButtonAction::unknown:
            default:
                return Events::MouseClickAction::release;
            }
        }

        Events::MouseButton toSceneMouseButton(MouseButton button) {
            switch (button) {
            case MouseButton::left:
                return Events::MouseButton::left;
            case MouseButton::right:
                return Events::MouseButton::right;
            case MouseButton::middle:
                return Events::MouseButton::middle;
            case MouseButton::button4:
                return Events::MouseButton::button4;
            case MouseButton::button5:
                return Events::MouseButton::button5;
            case MouseButton::button6:
                return Events::MouseButton::button6;
            case MouseButton::button7:
                return Events::MouseButton::button7;
            case MouseButton::button8:
                return Events::MouseButton::button8;
            case MouseButton::unknown:
            default:
                return Events::MouseButton::left;
            }
        }

        UI::UISceneComponent *getUIComponent(SceneState *state,
                                             const UUID &uuid) {
            if (state == nullptr || uuid == UUID::null) {
                return nullptr;
            }

            auto comp = state->getComponentByUuid(uuid);
            if (comp == nullptr || comp->getType() != SceneComponentType::ui) {
                return nullptr;
            }

            return dynamic_cast<UI::UISceneComponent *>(comp);
        }

        UI::UISceneComponent *getUIComponent(SceneState *state,
                                             const PickingId &pickingId) {
            if (state == nullptr || !pickingId.isValid()) {
                return nullptr;
            }

            auto comp = state->getComponentByPickingId(pickingId);
            if (comp == nullptr || comp->getType() != SceneComponentType::ui) {
                return nullptr;
            }

            return dynamic_cast<UI::UISceneComponent *>(comp.get());
        }

        UI::UISceneComponent *getParentUIComponent(SceneState *state,
                                                   UI::UISceneComponent *comp) {
            if (state == nullptr || comp == nullptr ||
                comp->getParentComponent() == UUID::null) {
                return nullptr;
            }
            return getUIComponent(state, comp->getParentComponent());
        }

        bool hasPassiveCursor(const PickingId &pickingId) {
            return (pickingId.info & PickingId::InfoFlags::passiveCursor) != 0u;
        }

        uint32_t eventDetails(const PickingId &pickingId) {
            return pickingId.info & ~PickingId::InfoFlags::passiveCursor;
        }

        Events::FocusEvent makeFocusEvent(SceneEventContext &ctx,
                                          const UUID &uuid,
                                          const glm::vec2 &mousePos,
                                          uint32_t details) {
            return {
                .entityUuid = uuid,
                .mousePos = mousePos,
                .details = details,
                .sceneState = ctx.sceneState,
            };
        }
    } // namespace

    EventResult UIComponentsLayer::handleEvent(SceneEvent &evt,
                                               SceneEventContext &ctx) {
        switch (evt.type) {
        case SceneEvent::Type::mouseMove:
            return handleMouseMove(evt, ctx);
        case SceneEvent::Type::mouseButton:
            return handleMouseButton(evt, ctx);
        case SceneEvent::Type::mouseWheel:
            return handleMouseWheel(evt, ctx);
        case SceneEvent::Type::key:
        case SceneEvent::Type::textInput:
            return handleKey(evt, ctx);
        case SceneEvent::Type::none:
            return EventResult::Ignored;
        }

        return EventResult::Ignored;
    }

    bool
    UIComponentsLayer::shouldReceiveConsumedEvent(const SceneEvent &evt) const {
        return evt.type == SceneEvent::Type::mouseMove;
    }

    EventResult UIComponentsLayer::handleMouseMove(SceneEvent &evt,
                                                   SceneEventContext &ctx) {
        if (ctx.sceneState == nullptr) {
            return EventResult::Ignored;
        }

        const auto &data = evt.data.mouseMove;
        if (auto pressed = getUIComponent(ctx.sceneState, m_pressedComponent);
            pressed != nullptr && pressed->hasPointerCapture()) {
            const bool handled = pressed->onPointerMove({
                .mousePos = data.pos,
                .details = eventDetails(m_pressedPickingId),
                .sceneState = ctx.sceneState,
            });
            if (ctx.viewportCtx) {
                const auto cursor = hasPassiveCursor(m_pressedPickingId)
                                        ? Core::Viewport::SceneCursor::normal
                                        : pressed->getCursor();
                ctx.viewportCtx->inputCtx.requestCursor(cursor,
                                                        kCursorPriorityCapture);
            }
            return handled ? EventResult::Consumed : EventResult::Handled;
        }

        auto uiComp = getUIComponent(ctx.sceneState, evt.pickingId);
        if (uiComp == nullptr) {
            const bool hadHoveredComponent = m_hoveredComponent != UUID::null;
            clearHover(ctx, data.pos);
            if (ctx.viewportCtx && hadHoveredComponent) {
                ctx.viewportCtx->inputCtx.requestCursor(
                    Core::Viewport::SceneCursor::normal, kCursorPriorityReset);
            }
            return EventResult::Ignored;
        }

        if (m_hoveredComponent != uiComp->getUuid() ||
            m_hoveredPickingId != evt.pickingId) {
            clearHover(ctx, data.pos);
            m_hoveredComponent = uiComp->getUuid();
            m_hoveredPickingId = evt.pickingId;
            uiComp->onMouseEnter({data.pos, eventDetails(evt.pickingId)});
        }

        if (ctx.viewportCtx) {
            const auto cursor = hasPassiveCursor(evt.pickingId)
                                    ? Core::Viewport::SceneCursor::normal
                                    : uiComp->getCursor();
            ctx.viewportCtx->inputCtx.requestCursor(cursor,
                                                    kCursorPriorityHover);
        }

        return EventResult::Consumed;
    }

    EventResult UIComponentsLayer::handleMouseButton(SceneEvent &evt,
                                                     SceneEventContext &ctx) {
        if (ctx.sceneState == nullptr) {
            return EventResult::Ignored;
        }

        const auto &data = evt.data.mouseButton;
        if (data.button == MouseButton::unknown) {
            return EventResult::Ignored;
        }
        const auto sceneButton = toSceneMouseButton(data.button);

        const bool isPress = data.action == MouseButtonAction::press ||
                             data.action == MouseButtonAction::doubleClick;
        if (!isPress && data.action != MouseButtonAction::release) {
            return EventResult::Ignored;
        }

        auto uiComp = getUIComponent(ctx.sceneState, evt.pickingId);
        if (isPress && uiComp == nullptr) {
            ctx.sceneState->clearUIFocus(
                makeFocusEvent(ctx, UUID::null, data.pos, evt.pickingId.info));
            m_pressedComponent = UUID::null;
            m_pressedPickingId = PickingId::invalid();
            m_pressedButton = Events::MouseButton::left;
            return EventResult::Ignored;
        }

        if (isPress && uiComp != nullptr) {
            const UUID compId = uiComp->getUuid();
            m_pressedComponent = compId;
            m_pressedPickingId = evt.pickingId;
            m_pressedButton = sceneButton;
            const auto focusEvent = makeFocusEvent(
                ctx, compId, data.pos, eventDetails(evt.pickingId));

            if (uiComp->isFocusable()) {
                ctx.sceneState->focusUIComponent(compId, focusEvent);
            }

            const bool handled = uiComp->onMouseButton({
                .mousePos = data.pos,
                .button = sceneButton,
                .action = toSceneMouseAction(data.action),
                .details = eventDetails(evt.pickingId),
                .sceneState = ctx.sceneState,
            });

            auto current = getUIComponent(ctx.sceneState, compId);
            if (current != nullptr && current->isFocusable() &&
                !ctx.sceneState->isUIComponentFocused(compId)) {
                ctx.sceneState->focusUIComponent(compId, focusEvent);
            } else if (current == nullptr || !current->isFocusable()) {
                ctx.sceneState->clearUIFocus(focusEvent);
            }

            (void)handled;
            return EventResult::Consumed;
        }

        if (data.action == MouseButtonAction::release &&
            m_pressedComponent != UUID::null) {
            auto pressed = getUIComponent(ctx.sceneState, m_pressedComponent);
            if (pressed != nullptr) {
                pressed->onMouseButton({
                    .mousePos = data.pos,
                    .button = m_pressedButton,
                    .action = Events::MouseClickAction::release,
                    .details = eventDetails(m_pressedPickingId),
                    .sceneState = ctx.sceneState,
                });
            }

            m_pressedComponent = UUID::null;
            m_pressedPickingId = PickingId::invalid();
            m_pressedButton = Events::MouseButton::left;
            return EventResult::Consumed;
        }

        return uiComp != nullptr ? EventResult::Consumed : EventResult::Ignored;
    }

    EventResult UIComponentsLayer::handleMouseWheel(SceneEvent &evt,
                                                    SceneEventContext &ctx) {
        if (ctx.sceneState == nullptr) {
            return EventResult::Ignored;
        }

        auto *uiComp = getUIComponent(ctx.sceneState, evt.pickingId);
        if (uiComp == nullptr) {
            return EventResult::Ignored;
        }

        const auto &data = evt.data.mouseWheel;
        uint32_t details = eventDetails(evt.pickingId);
        while (uiComp != nullptr) {
            const bool handled = uiComp->onMouseWheel({
                .mousePos = data.pos,
                .delta = data.delta,
                .details = details,
                .sceneState = ctx.sceneState,
            });
            if (handled) {
                return EventResult::Consumed;
            }

            uiComp = getParentUIComponent(ctx.sceneState, uiComp);
            details = 0u;
        }

        return EventResult::Ignored;
    }

    EventResult UIComponentsLayer::handleKey(SceneEvent &evt,
                                             SceneEventContext &ctx) {
        if (ctx.sceneState == nullptr) {
            return EventResult::Ignored;
        }

        auto focused = ctx.sceneState->getFocusedUIComponentPtr();
        if (focused == nullptr || !focused->wantsKeyboardInput()) {
            return EventResult::Ignored;
        }

        const auto focusedId = focused->getUuid();
        const bool handled = focused->onKeyEvent(evt);
        if (auto current = ctx.sceneState->getComponentByUuid(focusedId);
            current != nullptr && !current->isFocusable()) {
            ctx.sceneState->clearUIFocus({
                .entityUuid = focusedId,
                .sceneState = ctx.sceneState,
            });
        }

        return handled ? EventResult::Consumed : EventResult::Ignored;
    }

    void UIComponentsLayer::clearHover(SceneLayerContext &ctx,
                                       const glm::vec2 &mousePos) {
        if (m_hoveredComponent == UUID::null || ctx.sceneState == nullptr) {
            m_hoveredComponent = UUID::null;
            m_hoveredPickingId = PickingId::invalid();
            return;
        }

        if (auto prev = getUIComponent(ctx.sceneState, m_hoveredComponent)) {
            prev->onMouseLeave({mousePos, eventDetails(m_hoveredPickingId)});
        }

        m_hoveredComponent = UUID::null;
        m_hoveredPickingId = PickingId::invalid();
    }

    void UIComponentsLayer::update(TimeMs ts, SceneUpdateContext &ctx) {
        if (ctx.sceneState == nullptr) {
            return;
        }

        for (const auto &compId : ctx.sceneState->getRootComponents()) {
            auto comp = ctx.sceneState->getComponentByUuid(compId);
            if (comp != nullptr && comp->getType() == SceneComponentType::ui) {
                comp->update(ts, *ctx.sceneState);
            }
        }
    }

    void UIComponentsLayer::draw(SceneRenderContext &ctx) {
        if (ctx.sceneState == nullptr || ctx.viewportCtx == nullptr ||
            ctx.viewportCtx->isSchematicMode()) {
            return;
        }

        SceneDrawContext drawCtx{
            .sceneState = ctx.sceneState,
            .renderer = ctx.renderer,
            .camera = ctx.camera,
            .viewportId = ctx.viewportCtx->viewportId,
            .isSchematicMode = false,
            .sceneWidgetsState = ctx.sceneWidgetsState,
            .viewportCtx = ctx.viewportCtx,
        };

        for (const auto &compId : ctx.sceneState->getRootComponents()) {
            auto comp = ctx.sceneState->getComponentByUuid(compId);
            if (comp != nullptr && comp->getType() == SceneComponentType::ui) {
                comp->draw(drawCtx);
            }
        }
    }

    void UIComponentsLayer::reset(SceneLifecycleContext &ctx) {
        if (ctx.sceneState && ctx.viewportCtx) {
            clearHover(ctx,
                       ctx.camera ? ctx.camera->toWorldPos(
                                        ctx.viewportCtx->inputCtx.mousePos)
                                  : glm::vec2{0.f});
            ctx.sceneState->clearUIFocus({
                .sceneState = ctx.sceneState,
            });
        }

        m_hoveredComponent = UUID::null;
        m_pressedComponent = UUID::null;
        m_hoveredPickingId = PickingId::invalid();
        m_pressedPickingId = PickingId::invalid();
        m_pressedButton = Events::MouseButton::left;
    }

    void UIComponentsLayer::destroy(SceneLifecycleContext &ctx) {
        reset(ctx);
    }
} // namespace Bess::Canvas
