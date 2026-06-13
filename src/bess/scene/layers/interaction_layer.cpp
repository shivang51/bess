#include "interaction_layer.h"
#include "bess_core/g_app_context.h"
#include "common/bess_assert.h"
#include "common/logger.h"
#include "event_dispatcher.h"
#include "scene/scene_state/components/behaviours/drag_behaviour.h"
#include "scene/scene_state/components/scene_component.h"
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
                                              SceneContext &ctx) {
        switch (evt.type) {
        case SceneEvent::Type::mouseMove:
            return handleMouseMove(evt, ctx);
        case SceneEvent::Type::mouseButton:
            return handleMouseButton(evt, ctx);
        case SceneEvent::Type::mouseWheel:
            return handleMouseWheel(evt, ctx);
        case SceneEvent::Type::key:
        case SceneEvent::Type::none:
            break;
        }

        return EventResult::Ignored;
    }

    EventResult InteractionLayer::handleMouseMove(SceneEvent &evt,
                                                  SceneContext &ctx) {
        BESS_ASSERT(ctx.sceneState && ctx.camera && ctx.mousePos &&
                        ctx.dMousePos && ctx.isLeftMousePressed &&
                        ctx.isMiddleMousePressed && ctx.isDragging &&
                        ctx.selBoxContext && ctx.drawMode && ctx.pickingId,
                    "InteractionLayer missing scene context");

        const auto &data = evt.data.mouseMove;
        ctx.sceneState->setMousePos(data.pos);

        *ctx.dMousePos = data.pos - ctx.camera->toWorldPos(*ctx.mousePos);
        *ctx.mousePos = data.viewportPos;

        if (*ctx.isLeftMousePressed && *ctx.drawMode == SceneDrawMode::none) {
            if (ctx.pickingId->isValid()) {
                const auto &selectedComps =
                    ctx.sceneState->getSelectedComponents();
                for (const auto &compId :
                     selectedComps | std::ranges::views::keys) {
                    auto comp = ctx.sceneState->getComponentByUuid(compId);
                    if (!comp || !comp->isDraggable()) {
                        continue;
                    }

                    auto dragComp =
                        std::dynamic_pointer_cast<IDragBehaviour>(comp);
                    if (!dragComp) {
                        BESS_ERROR("Component {} of type {} is marked as "
                                   "draggable but does not implement "
                                   "IDragBehaviour",
                                   (uint64_t)comp->getUuid(),
                                   comp->getStaticTypeName());
                        continue;
                    }

                    dragComp->onMouseDragged(
                        {data.pos, *ctx.dMousePos, ctx.pickingId->info,
                         selectedComps.size() > 1, ctx.sceneState});

                    if (ctx.sceneState->getConnectionStartSlot() == compId) {
                        ctx.sceneState->setConnectionStartSlot(UUID::null);
                    }
                    *ctx.isDragging = true;
                }
            } else if (!ctx.selBoxContext->draw) {
                ctx.selBoxContext->draw = true;
                ctx.selBoxContext->start = *ctx.mousePos;
            }
        } else if (*ctx.isMiddleMousePressed) {
            ctx.camera->incrementPos(-*ctx.dMousePos);
        }

        return EventResult::Handled;
    }

    EventResult InteractionLayer::handleMouseButton(SceneEvent &evt,
                                                    SceneContext &ctx) {
        const auto &data = evt.data.mouseButton;
        const bool isPressed = data.action == MouseButtonAction::press;

        if (data.button == MouseButton::left) {
            return handleLeftMouseButton(evt, ctx, isPressed);
        } else if (data.button == MouseButton::middle) {
            return handleMiddleMouseButton(evt, ctx, isPressed);
        } else if (data.button == MouseButton::right) {
            queueMouseButtonEvent(evt, ctx, Events::MouseButton::right,
                                  toSceneMouseAction(data.action));
            return EventResult::Consumed;
        }

        return EventResult::Ignored;
    }

    EventResult InteractionLayer::handleLeftMouseButton(SceneEvent &evt,
                                                        SceneContext &ctx,
                                                        bool isPressed) {
        BESS_ASSERT(ctx.sceneState && ctx.mousePos && ctx.isLeftMousePressed &&
                        ctx.isDragging && ctx.selBoxContext && ctx.drawMode &&
                        ctx.pickingId,
                    "InteractionLayer missing scene context");

        *ctx.isLeftMousePressed = isPressed;
        const auto action = toSceneMouseAction(evt.data.mouseButton.action);
        queueMouseButtonEvent(evt, ctx, Events::MouseButton::left, action);

        if (evt.data.mouseButton.action == MouseButtonAction::doubleClick) {
            if (ctx.pickingId->isValid()) {
                if (auto comp = ctx.sceneState->getComponentByPickingId(
                        *ctx.pickingId)) {
                    comp->onMouseButton({evt.data.mouseButton.pos,
                                         Events::MouseButton::left, action,
                                         ctx.pickingId->info, ctx.sceneState});
                }
            }
            return EventResult::Consumed;
        }

        if (!isPressed) {
            const size_t selSize =
                ctx.sceneState->getSelectedComponents().size();
            if (selSize > 1 && !*ctx.isDragging && !evt.isCtrlPressed &&
                ctx.sceneState->isComponentSelected(*ctx.pickingId)) {
                ctx.sceneState->clearSelectedComponents();
                ctx.sceneState->addSelectedComponent(*ctx.pickingId);
            }

            if (ctx.selBoxContext->draw) {
                ctx.selBoxContext->draw = false;
                ctx.selBoxContext->queueSelInNextFrame = true;
                ctx.selBoxContext->end = *ctx.mousePos;
            } else if (*ctx.isDragging) {
                endActiveDrag(ctx);
            } else if (ctx.pickingId->isValid()) {
                if (auto comp = ctx.sceneState->getComponentByPickingId(
                        *ctx.pickingId)) {
                    comp->onMouseButton({evt.data.mouseButton.pos,
                                         Events::MouseButton::left, action,
                                         ctx.pickingId->info, ctx.sceneState});
                }
            }

            return EventResult::Consumed;
        }

        if (ctx.pickingId->isValid()) {
            auto comp = ctx.sceneState->getComponentByPickingId(*ctx.pickingId);
            if (!comp) {
                return EventResult::Consumed;
            }

            comp->onMouseButton({evt.data.mouseButton.pos,
                                 Events::MouseButton::left, action,
                                 ctx.pickingId->info, ctx.sceneState});

            if (evt.isCtrlPressed) {
                if (ctx.sceneState->isComponentSelected(*ctx.pickingId)) {
                    ctx.sceneState->removeSelectedComponent(*ctx.pickingId);
                } else {
                    ctx.sceneState->addSelectedComponent(*ctx.pickingId);
                }
            } else {
                const size_t selSize =
                    ctx.sceneState->getSelectedComponents().size();
                if (selSize < 2 || !comp->getIsSelected()) {
                    ctx.sceneState->clearSelectedComponents();
                    ctx.sceneState->addSelectedComponent(*ctx.pickingId);
                }
            }
        } else {
            ctx.sceneState->clearSelectedComponents();
            ctx.sceneState->setConnectionStartSlot(UUID::null);
            *ctx.drawMode = SceneDrawMode::none;
        }

        return EventResult::Consumed;
    }

    EventResult InteractionLayer::handleMiddleMouseButton(SceneEvent &evt,
                                                          SceneContext &ctx,
                                                          bool isPressed) {
        BESS_ASSERT(ctx.isMiddleMousePressed,
                    "InteractionLayer missing middle mouse state");
        *ctx.isMiddleMousePressed = isPressed;
        queueMouseButtonEvent(evt, ctx, Events::MouseButton::middle,
                              toSceneMouseAction(evt.data.mouseButton.action));
        return EventResult::Consumed;
    }

    EventResult InteractionLayer::handleMouseWheel(SceneEvent &evt,
                                                   SceneContext &ctx) {
        BESS_ASSERT(ctx.camera && ctx.mousePos,
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
        SceneEvent &evt, SceneContext &ctx, Events::MouseButton button,
        Events::MouseClickAction action) const {
        auto &appCtx = GAppContext::getInstance();
        auto eventDispatcher =
            appCtx.getSubSystem<Bess::EventSystem::EventDispatcher>();

        const uint32_t details =
            ctx.pickingId != nullptr ? ctx.pickingId->info : 0u;
        eventDispatcher->queue(Events::MouseButtonEvent{
            evt.data.mouseButton.pos, button, action, details, ctx.sceneState});
    }

    bool InteractionLayer::isCursorInViewport(SceneContext &ctx) const {
        if (!ctx.viewportTransform || !ctx.mousePos) {
            return false;
        }

        const auto &viewportSize = ctx.viewportTransform->size;
        const auto &pos = *ctx.mousePos;
        return pos.x >= 1.f && pos.x < viewportSize.x - 1.f && pos.y >= 1.f &&
               pos.y < viewportSize.y - 1.f;
    }

    void InteractionLayer::endActiveDrag(SceneContext &ctx) const {
        *ctx.isDragging = false;
        for (const auto &compId : ctx.sceneState->getSelectedComponents() |
                                      std::ranges::views::keys) {
            auto comp = ctx.sceneState->getComponentByUuid(compId);
            if (comp && comp->isDraggable()) {
                if (auto dragComp =
                        std::dynamic_pointer_cast<IDragBehaviour>(comp)) {
                    dragComp->onMouseDragEnd();
                }
            }
        }
    }

    void InteractionLayer::update(TimeMs ts, SceneContext &ctx) {
        if (!ctx.selBoxContext || !ctx.pickingReadbackRequest ||
            !ctx.viewportTransform) {
            return;
        }

        auto &selCtx = *ctx.selBoxContext;
        auto &request = *ctx.pickingReadbackRequest;

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

        const auto viewportSize = ctx.viewportTransform->size;
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
        const auto x1 = static_cast<uint32_t>(
            std::clamp(std::ceil(maxX), static_cast<float>(x0 + 1u),
                       static_cast<float>(width)));
        const auto y1 = static_cast<uint32_t>(
            std::clamp(std::ceil(maxY), static_cast<float>(y0 + 1u),
                       static_cast<float>(height)));

        request = {
            .x = x0,
            .y = y0,
            .width = x1 - x0,
            .height = y1 - y0,
            .active = true,
        };
    }

    void InteractionLayer::draw(SceneContext &ctx) {}
} // namespace Bess::Canvas
