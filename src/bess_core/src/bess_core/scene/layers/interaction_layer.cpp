#include "bess_core/scene/layers/interaction_layer.h"
#include "bess_core/g_app_context.h"
#include "bess_core/scene/scene_events.h"
#include "bess_core/scene/scene_state/components/behaviours/drag_behaviour.h"
#include "bess_core/scene/scene_state/components/scene_component.h"
#include "bess_core/viewport.h"
#include "common/bess_assert.h"
#include "common/logger.h"
#include "event_dispatcher.h"
#include <algorithm>
#include <cmath>
#include <ranges>

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
    } // namespace

    EventResult InteractionLayer::handleEvent(SceneEvent &evt,
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
        case SceneEvent::Type::none:
            break;
        }

        return EventResult::Ignored;
    }

    EventResult InteractionLayer::handleMouseMove(SceneEvent &evt,
                                                  SceneEventContext &ctx) {
        BESS_ASSERT(ctx.sceneState && ctx.camera && ctx.viewportCtx,
                    "InteractionLayer missing scene context");

        auto &input = ctx.viewportCtx->inputCtx;
        const auto &data = evt.data.mouseMove;

        if (input.isLeftMousePressed &&
            ctx.viewportCtx->drawMode ==
                Core::Viewport::ViewportDrawMode::none) {
            if (ctx.viewportCtx->inputCtx.pickingId.isValid()) {
                const auto &selectedComps =
                    ctx.sceneState->getSelectedComponents();
                for (const auto &compId :
                     selectedComps | std::ranges::views::keys) {
                    auto comp = ctx.sceneState->getComponentByUuid(compId);
                    if (!comp || !comp->isDraggable()) {
                        continue;
                    }

                    auto dragComp = dynamic_cast<IDragBehaviour *>(comp);
                    if (!dragComp) {
                        BESS_ERROR("Component {} of type {} is marked as "
                                   "draggable but does not implement "
                                   "IDragBehaviour",
                                   (uint64_t)comp->getUuid(),
                                   comp->getStaticTypeName());
                        continue;
                    }

                    dragComp->onMouseDragged(Events::MouseDraggedEvent{
                        data.pos,
                        input.dMousePos,
                        ctx.viewportCtx->inputCtx.pickingId.info,
                        selectedComps.size() > 1,
                        ctx.sceneState,
                        ctx.viewportCtx->isSchematicMode(),
                    });

                    if (ctx.sceneState->getConnectionStartSlot() == compId) {
                        ctx.sceneState->setConnectionStartSlot(UUID::null);
                    }
                    input.isDragging = true;
                }
            } else if (!ctx.viewportCtx->selBoxCtx.draw) {
                ctx.viewportCtx->selBoxCtx.draw = true;
                ctx.viewportCtx->selBoxCtx.start = input.mousePos;
            }
        } else if (input.isMiddleMousePressed) {
            ctx.camera->incrementPos(-input.dMousePos);
        }

        return EventResult::Handled;
    }

    EventResult InteractionLayer::handleMouseButton(SceneEvent &evt,
                                                    SceneEventContext &ctx) {
        const auto &data = evt.data.mouseButton;
        const bool isPressed = data.action == MouseButtonAction::press;

        if (data.button == MouseButton::left) {
            return handleLeftMouseButton(evt, ctx, isPressed);
        } else if (data.button == MouseButton::middle) {
            return handleMiddleMouseButton(evt, ctx, isPressed);
        } else if (data.button == MouseButton::right) {
            queueMouseButtonEvent(evt,
                                  ctx,
                                  Events::MouseButton::right,
                                  toSceneMouseAction(data.action));
            return EventResult::Consumed;
        }

        return EventResult::Ignored;
    }

    EventResult InteractionLayer::handleLeftMouseButton(SceneEvent &evt,
                                                        SceneEventContext &ctx,
                                                        bool isPressed) {
        BESS_ASSERT(ctx.sceneState && ctx.viewportCtx,
                    "InteractionLayer missing scene context");

        const auto &pickingId = ctx.viewportCtx->inputCtx.pickingId;
        auto &input = ctx.viewportCtx->inputCtx;
        input.isLeftMousePressed = isPressed;
        const auto action = toSceneMouseAction(evt.data.mouseButton.action);
        queueMouseButtonEvent(evt, ctx, Events::MouseButton::left, action);

        if (evt.data.mouseButton.action == MouseButtonAction::doubleClick) {
            if (pickingId.isValid()) {
                if (auto comp =
                        ctx.sceneState->getComponentByPickingId(pickingId)) {
                    comp->onMouseButton({evt.data.mouseButton.pos,
                                         Events::MouseButton::left,
                                         action,
                                         pickingId.info,
                                         ctx.sceneState});
                }
            }
            return EventResult::Consumed;
        }

        if (!isPressed) {
            const size_t selSize =
                ctx.sceneState->getSelectedComponents().size();
            if (selSize > 1 && !input.isDragging && !evt.isCtrlPressed &&
                pickingId.isValid() &&
                ctx.sceneState->isComponentSelected(pickingId)) {
                ctx.sceneState->clearSelectedComponents();
                ctx.sceneState->addSelectedComponent(pickingId);
            }

            auto &selBox = ctx.viewportCtx->selBoxCtx;

            if (selBox.draw) {
                selBox.draw = false;
                selBox.queueSelInNextFrame = true;
                selBox.end = input.mousePos;
            } else if (input.isDragging) {
                endActiveDrag(ctx);
            } else if (pickingId.isValid()) {
                if (auto comp =
                        ctx.sceneState->getComponentByPickingId(pickingId)) {
                    comp->onMouseButton({evt.data.mouseButton.pos,
                                         Events::MouseButton::left,
                                         action,
                                         pickingId.info,
                                         ctx.sceneState});
                }
            }

            return EventResult::Consumed;
        }

        if (pickingId.isValid()) {
            auto comp = ctx.sceneState->getComponentByPickingId(pickingId);
            if (!comp) {
                return EventResult::Consumed;
            }

            comp->onMouseButton({evt.data.mouseButton.pos,
                                 Events::MouseButton::left,
                                 action,
                                 pickingId.info,
                                 ctx.sceneState});

            if (evt.isCtrlPressed) {
                if (ctx.sceneState->isComponentSelected(pickingId)) {
                    ctx.sceneState->removeSelectedComponent(pickingId);
                } else {
                    ctx.sceneState->addSelectedComponent(pickingId);
                }
            } else {
                const size_t selSize =
                    ctx.sceneState->getSelectedComponents().size();
                if (selSize < 2 || !comp->getIsSelected()) {
                    ctx.sceneState->clearSelectedComponents();
                    ctx.sceneState->addSelectedComponent(pickingId);
                }
            }
        } else {
            ctx.sceneState->clearSelectedComponents();
            ctx.sceneState->setConnectionStartSlot(UUID::null);
            ctx.viewportCtx->drawMode = Core::Viewport::ViewportDrawMode::none;
        }

        return EventResult::Consumed;
    }

    EventResult InteractionLayer::handleMiddleMouseButton(
        SceneEvent &evt, SceneEventContext &ctx, bool isPressed) {
        BESS_ASSERT(ctx.viewportCtx,
                    "InteractionLayer missing viewport context");
        ctx.viewportCtx->inputCtx.isMiddleMousePressed = isPressed;
        queueMouseButtonEvent(evt,
                              ctx,
                              Events::MouseButton::middle,
                              toSceneMouseAction(evt.data.mouseButton.action));
        return EventResult::Consumed;
    }

    EventResult InteractionLayer::handleMouseWheel(SceneEvent &evt,
                                                   SceneEventContext &ctx) {
        BESS_ASSERT(ctx.camera && ctx.viewportCtx,
                    "InteractionLayer missing wheel context");

        if (!isCursorInViewport(ctx)) {
            return EventResult::Consumed;
        }

        const auto &delta = evt.data.mouseWheel.delta;
        if (evt.isCtrlPressed) {
            ctx.camera->incrementZoomToPoint(evt.data.mouseWheel.pos,
                                             delta.y * 0.1f);
        } else {
            glm::vec2 dPos = delta;
            dPos *= 10.f / ctx.camera->getZoom() * -1.f;
            ctx.camera->incrementPos(dPos);
        }

        return EventResult::Consumed;
    }

    void InteractionLayer::queueMouseButtonEvent(
        SceneEvent &evt,
        SceneEventContext &ctx,
        Events::MouseButton button,
        Events::MouseClickAction action) const {
        auto &appCtx = GAppContext::getInstance();
        auto eventDispatcher =
            appCtx.getSubSystem<Bess::EventSystem::EventDispatcher>();

        const auto &pickingId = ctx.viewportCtx->inputCtx.pickingId;
        eventDispatcher->queue(
            Events::MouseButtonEvent{evt.data.mouseButton.pos,
                                     button,
                                     action,
                                     pickingId.info,
                                     ctx.sceneState});
    }

    bool InteractionLayer::isCursorInViewport(SceneEventContext &ctx) const {
        if (!ctx.viewportCtx) {
            return false;
        }

        const auto &viewportSize = ctx.viewportCtx->transform.size;
        const auto &pos = ctx.viewportCtx->inputCtx.mousePos;
        return pos.x >= 1.f && pos.x < viewportSize.x - 1.f && pos.y >= 1.f &&
               pos.y < viewportSize.y - 1.f;
    }

    void InteractionLayer::endActiveDrag(SceneEventContext &ctx) const {
        ctx.viewportCtx->inputCtx.isDragging = false;
        for (const auto &compId : ctx.sceneState->getSelectedComponents() |
                                      std::ranges::views::keys) {
            auto comp = ctx.sceneState->getComponentByUuid(compId);
            if (comp && comp->isDraggable()) {
                if (auto dragComp = dynamic_cast<IDragBehaviour *>(comp)) {
                    dragComp->onMouseDragEnd();
                }
            }
        }
    }

    void InteractionLayer::viewportUpdate(TimeMs ts,
                                          SceneVpUpdateContext &ctx) {
        if (!ctx.viewportCtx) {
            return;
        }

        auto &selCtx = ctx.viewportCtx->selBoxCtx;
        auto &request = ctx.viewportCtx->pickingReadbackRequest;

        if (request.active) {
            return;
        }

        if (selCtx.queueSelInNextFrame) {
            selCtx.queueSelInNextFrame = false;
            selCtx.queueForSel = true;
            return;
        }

        if (!selCtx.queueForSel) {
            return;
        }

        selCtx.queueForSel = false;

        const auto viewportSize = ctx.viewportCtx->transform.size;
        const uint32_t width =
            viewportSize.x > 1.f ? static_cast<uint32_t>(viewportSize.x) : 1u;
        const uint32_t height =
            viewportSize.y > 1.f ? static_cast<uint32_t>(viewportSize.y) : 1u;

        const float minX = std::min(selCtx.start.x, selCtx.end.x);
        const float minY = std::min(selCtx.start.y, selCtx.end.y);
        const float maxX = std::max(selCtx.start.x, selCtx.end.x);
        const float maxY = std::max(selCtx.start.y, selCtx.end.y);

        const auto x0 = static_cast<uint32_t>(
            std::clamp(std::floor(minX), 0.f, static_cast<float>(width - 1u)));
        const auto y0 = static_cast<uint32_t>(
            std::clamp(std::floor(minY), 0.f, static_cast<float>(height - 1u)));
        const auto x1 =
            static_cast<uint32_t>(std::clamp(std::ceil(maxX),
                                             static_cast<float>(x0 + 1u),
                                             static_cast<float>(width)));
        const auto y1 =
            static_cast<uint32_t>(std::clamp(std::ceil(maxY),
                                             static_cast<float>(y0 + 1u),
                                             static_cast<float>(height)));

        request = {
            .x = x0,
            .y = y0,
            .width = x1 - x0,
            .height = y1 - y0,
            .active = true,
        };
    }

    void InteractionLayer::draw(SceneRenderContext &ctx) {
    }

    void InteractionLayer::update(TimeMs ts, SceneUpdateContext &ctx) {};
} // namespace Bess::Canvas
