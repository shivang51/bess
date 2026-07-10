#include "bess_core/scene/layers/ui_components_layer.h"
#include "bess_core/scene/scene_draw_context.h"
#include "bess_core/scene/scene_events.h"
#include "bess_core/scene/scene_state/components/scene_component.h"
#include "bess_core/scene/scene_ui/ui_scene_component.h"
#include "bess_core/scene/widgets/scene_widgets.h"
#include "bess_core/viewport.h"
#include "pages/main_page/scene_components/scene_comp_types.h"

namespace Bess::Canvas {
    namespace {
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

        bool targetKeepsOwnCursor(SceneEventContext &ctx,
                                  const PickingId &pickingId) {
            if (SceneWidgets::contains(ctx.sceneWidgetsState, pickingId)) {
                return true;
            }

            if (ctx.sceneState == nullptr || !pickingId.isValid()) {
                return false;
            }

            const auto comp = ctx.sceneState->getComponentByPickingId(pickingId);
            if (comp == nullptr) {
                return false;
            }

            switch (comp->getType()) {
            case SceneComponentType::slot:
            case SceneComponentType::connection:
            case SceneComponentType::connJoint:
                return true;
            default:
                return false;
            }
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

    bool UIComponentsLayer::shouldReceiveConsumedEvent(
        const SceneEvent &evt) const {
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
                .details = m_pressedPickingId.info,
                .sceneState = ctx.sceneState,
            });
            if (ctx.viewportCtx) {
                ctx.viewportCtx->inputCtx.cursor = pressed->getCursor();
            }
            return handled ? EventResult::Consumed : EventResult::Handled;
        }

        auto uiComp = getUIComponent(ctx.sceneState, evt.pickingId);
        if (uiComp == nullptr) {
            const bool hadHoveredComponent = m_hoveredComponent != UUID::null;
            clearHover(ctx, data.pos);
            if (ctx.viewportCtx && hadHoveredComponent &&
                !targetKeepsOwnCursor(ctx, evt.pickingId)) {
                ctx.viewportCtx->inputCtx.cursor =
                    Core::Viewport::SceneCursor::normal;
            }
            return EventResult::Ignored;
        }

        if (m_hoveredComponent != uiComp->getUuid() ||
            m_hoveredPickingId != evt.pickingId) {
            clearHover(ctx, data.pos);
            m_hoveredComponent = uiComp->getUuid();
            m_hoveredPickingId = evt.pickingId;
            uiComp->onMouseEnter({data.pos, evt.pickingId.info});
        }

        if (ctx.viewportCtx) {
            ctx.viewportCtx->inputCtx.cursor = uiComp->getCursor();
        }

        return EventResult::Consumed;
    }

    EventResult UIComponentsLayer::handleMouseButton(SceneEvent &evt,
                                                     SceneEventContext &ctx) {
        if (ctx.sceneState == nullptr) {
            return EventResult::Ignored;
        }

        const auto &data = evt.data.mouseButton;
        if (data.button != MouseButton::left) {
            return EventResult::Ignored;
        }

        const bool isPress = data.action == MouseButtonAction::press ||
                             data.action == MouseButtonAction::doubleClick;
        if (!isPress && data.action != MouseButtonAction::release) {
            return EventResult::Ignored;
        }

        auto uiComp = getUIComponent(ctx.sceneState, evt.pickingId);
        if (isPress && uiComp == nullptr) {
            ctx.sceneState->clearUIFocus(makeFocusEvent(
                ctx, UUID::null, data.pos, evt.pickingId.info));
            m_pressedComponent = UUID::null;
            m_pressedPickingId = PickingId::invalid();
            return EventResult::Ignored;
        }

        if (isPress && uiComp != nullptr) {
            const UUID compId = uiComp->getUuid();
            m_pressedComponent = compId;
            m_pressedPickingId = evt.pickingId;
            const auto focusEvent =
                makeFocusEvent(ctx, compId, data.pos, evt.pickingId.info);

            if (uiComp->isFocusable()) {
                ctx.sceneState->focusUIComponent(compId, focusEvent);
            }

            const bool handled = uiComp->onMouseButton({
                .mousePos = data.pos,
                .button = Events::MouseButton::left,
                .action = toSceneMouseAction(data.action),
                .details = evt.pickingId.info,
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
                    .button = Events::MouseButton::left,
                    .action = Events::MouseClickAction::release,
                    .details = m_pressedPickingId.info,
                    .sceneState = ctx.sceneState,
                });
            }

            m_pressedComponent = UUID::null;
            m_pressedPickingId = PickingId::invalid();
            return EventResult::Consumed;
        }

        return uiComp != nullptr ? EventResult::Consumed
                                 : EventResult::Ignored;
    }

    EventResult UIComponentsLayer::handleMouseWheel(SceneEvent &evt,
                                                    SceneEventContext &ctx) {
        if (ctx.sceneState == nullptr) {
            return EventResult::Ignored;
        }

        auto uiComp = getUIComponent(ctx.sceneState, evt.pickingId);
        if (uiComp == nullptr) {
            return EventResult::Ignored;
        }

        const auto &data = evt.data.mouseWheel;
        const bool handled = uiComp->onMouseWheel({
            .mousePos = data.pos,
            .delta = data.delta,
            .details = evt.pickingId.info,
            .sceneState = ctx.sceneState,
        });
        return handled ? EventResult::Consumed : EventResult::Ignored;
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
            prev->onMouseLeave({mousePos, m_hoveredPickingId.info});
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
    }

    void UIComponentsLayer::destroy(SceneLifecycleContext &ctx) {
        reset(ctx);
    }
} // namespace Bess::Canvas
