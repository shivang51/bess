#include "controls/basic_widgets.h"
#include "controls/reorderable_list.h"
#include "drag_drop.h"
#include "widget_tree.h"

#include <gtest/gtest.h>

#include <array>
#include <limits>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace {
    using namespace Bess;
    using namespace Bess::UI;

    DragPayload textPayload(std::string value = "payload") {
        DragPayloadBuilder builder;
        EXPECT_TRUE(builder.set(DragFormats::plainText, std::move(value)));
        return std::move(builder).build();
    }

    TEST(DragPayloadTests, StoresMultipleTypedFormatsAndRejectsTypeConflicts) {
        const DragFormat<std::vector<uint64_t>> assetIds{
            "application/x-bess-asset-ids"};
        const DragFormat<std::string> conflictingAssetIds{
            "application/x-bess-asset-ids"};

        DragPayloadBuilder builder;
        EXPECT_TRUE(builder.set(DragFormats::plainText, std::string{"Asset"}));
        EXPECT_TRUE(builder.set(assetIds, std::vector<uint64_t>{4, 8, 15}));
        EXPECT_FALSE(builder.set(conflictingAssetIds, std::string{"unsafe"}));
        EXPECT_EQ(builder.size(), 2U);

        const auto payload = builder.build();
        ASSERT_NE(payload.get(DragFormats::plainText), nullptr);
        EXPECT_EQ(*payload.get(DragFormats::plainText), "Asset");
        ASSERT_NE(payload.get(assetIds), nullptr);
        EXPECT_EQ(*payload.get(assetIds), (std::vector<uint64_t>{4, 8, 15}));
        EXPECT_EQ(payload.get(conflictingAssetIds), nullptr);
        EXPECT_TRUE(payload.has("text/plain"));
        EXPECT_FALSE(payload.has("application/x-missing"));

        EXPECT_TRUE(
            builder.set(DragFormats::plainText, std::string{"Replacement"}));
        EXPECT_EQ(*builder.build().get(DragFormats::plainText), "Replacement");
        // Previously built payloads are immutable snapshots.
        EXPECT_EQ(*payload.get(DragFormats::plainText), "Asset");
    }

    TEST(DragDropServiceTests,
         DelaysPayloadCreationAndRoutesToDeepestAcceptingTarget) {
        DragDropService service{{.dragThreshold = 4.f}};
        std::vector<std::string> events;
        size_t payloadCreations = 0;
        std::optional<DragCompletedEvent> completed;

        auto outer = service.registerTarget({
            .propose =
                [](const DragTargetEvent &) {
                    return DragProposal{DragOperation::move};
                },
            .onEnter =
                [&](const DragTargetEvent &) {
                    events.push_back("outer-enter");
                },
            .onOver =
                [&](const DragTargetEvent &) {
                    events.push_back("outer-over");
                },
            .onLeave =
                [&](const DragLeaveEvent &) {
                    events.push_back("outer-leave");
                },
            .onDrop =
                [&](const DropEvent &) {
                    events.push_back("outer-drop");
                    return true;
                },
        });
        auto inner = service.registerTarget({
            .propose =
                [](const DragTargetEvent &event) {
                    return event.payload.has(DragFormats::plainText.id())
                               ? DragProposal{DragOperation::move}
                               : DragProposal{};
                },
            .onEnter =
                [&](const DragTargetEvent &) {
                    events.push_back("inner-enter");
                },
            .onOver =
                [&](const DragTargetEvent &) {
                    events.push_back("inner-over");
                },
            .onLeave =
                [&](const DragLeaveEvent &event) {
                    events.push_back(event.reason ==
                                             DragLeaveReason::targetChanged
                                         ? "inner-leave-changed"
                                         : "inner-leave-finished");
                },
            .onDrop = [](const DropEvent &) { return true; },
        });

        const auto source = DragSourceId::generate();
        const auto session = service.arm({
            .source = source,
            .pressPosition = {10.f, 10.f},
            .createPayload =
                [&] {
                    ++payloadCreations;
                    return textPayload();
                },
            .allowedOperations = DragOperation::copy | DragOperation::move,
            .preferredOperation = DragOperation::move,
            .callbacks = {
                .onStarted =
                    [&](const DragStartedEvent &) {
                        events.push_back("started");
                    },
                .onCompleted =
                    [&](const DragCompletedEvent &event) {
                        completed = event;
                        events.push_back("completed");
                    },
            },
        });
        ASSERT_TRUE(session);

        std::vector<DropTargetCandidate> candidates{
            {.target = inner.id(), .localPosition = {2.f, 3.f}},
            {.target = outer.id(), .localPosition = {12.f, 13.f}},
        };
        auto update = service.updatePointer({
            .position = {12.f, 12.f},
            .candidates = candidates,
        });
        EXPECT_FALSE(update.dragging);
        EXPECT_EQ(payloadCreations, 0U);

        update = service.updatePointer({
            .position = {15.f, 10.f},
            .candidates = candidates,
        });
        EXPECT_TRUE(update.started);
        EXPECT_TRUE(update.dragging);
        EXPECT_TRUE(update.accepted);
        EXPECT_EQ(update.target, inner.id());
        EXPECT_EQ(payloadCreations, 1U);
        EXPECT_EQ(
            events,
            (std::vector<std::string>{"started", "inner-enter", "inner-over"}));

        candidates.erase(candidates.begin());
        update = service.updatePointer({
            .position = {20.f, 10.f},
            .candidates = candidates,
        });
        EXPECT_EQ(update.target, outer.id());
        EXPECT_EQ(events,
                  (std::vector<std::string>{"started",
                                            "inner-enter",
                                            "inner-over",
                                            "inner-leave-changed",
                                            "outer-enter",
                                            "outer-over"}));

        const auto result = service.drop({
            .position = {20.f, 10.f},
            .candidates = candidates,
        });
        EXPECT_TRUE(result.accepted);
        EXPECT_EQ(result.reason, DragCompletionReason::dropped);
        EXPECT_EQ(result.target, outer.id());
        EXPECT_EQ(result.operation, DragOperation::move);
        ASSERT_TRUE(completed.has_value());
        EXPECT_TRUE(completed->accepted);
        EXPECT_FALSE(service.hasSession());
        EXPECT_EQ(events.back(), "completed");
    }

    TEST(DragDropServiceTests,
         FallsBackToAncestorAndRevalidatesOperationOnEveryUpdate) {
        DragDropService service;
        auto rejectingChild = service.registerTarget({
            .propose = [](const DragTargetEvent &) { return DragProposal{}; },
            .onDrop = [](const DropEvent &) { return false; },
        });
        auto copyParent = service.registerTarget({
            .propose =
                [](const DragTargetEvent &event) {
                    EXPECT_EQ(event.requestedOperation, DragOperation::copy);
                    return DragProposal{DragOperation::copy};
                },
            .onDrop = [](const DropEvent &) { return true; },
        });

        ASSERT_TRUE(service.arm({
            .source = DragSourceId::generate(),
            .pressPosition = {0.f, 0.f},
            .payload = textPayload(),
            .allowedOperations = DragOperation::copy | DragOperation::move,
            .preferredOperation = DragOperation::move,
            .threshold = 0.f,
        }));
        const std::vector<DropTargetCandidate> candidates{
            {.target = rejectingChild.id()},
            {.target = copyParent.id()},
        };
        const auto update = service.updatePointer({
            .position = {1.f, 0.f},
            .requestedOperation = DragOperation::copy,
            .candidates = candidates,
        });
        EXPECT_EQ(update.target, copyParent.id());
        EXPECT_EQ(update.operation, DragOperation::copy);
    }

    TEST(DragDropServiceTests,
         TargetMayUnregisterAndCancelFromInsideCallbacks) {
        DragDropService service{{.dragThreshold = 0.f}};
        std::optional<DropTargetRegistration> registration;
        std::optional<DragCompletedEvent> completion;

        registration.emplace(service.registerTarget({
            .propose =
                [](const DragTargetEvent &) {
                    return DragProposal{DragOperation::move};
                },
            .onOver =
                [&](const DragTargetEvent &) {
                    EXPECT_TRUE(registration->reset());
                    EXPECT_TRUE(service.cancel(DragCompletionReason::escape));
                },
            .onDrop = [](const DropEvent &) { return false; },
        }));
        const auto target = registration->id();
        ASSERT_TRUE(service.arm({
            .source = DragSourceId::generate(),
            .pressPosition = {0.f, 0.f},
            .payload = textPayload(),
            .callbacks =
                {
                    .onCompleted =
                        [&](const DragCompletedEvent &event) {
                            completion = event;
                        },
                },
        }));
        const std::vector<DropTargetCandidate> candidates{{.target = target}};
        EXPECT_NO_THROW(static_cast<void>(service.updatePointer({
            .position = {1.f, 0.f},
            .candidates = candidates,
        })));
        EXPECT_FALSE(service.hasSession());
        EXPECT_FALSE(service.containsTarget(target));
        ASSERT_TRUE(completion.has_value());
        EXPECT_EQ(completion->reason, DragCompletionReason::escape);
    }

    TEST(DragDropServiceTests, ExternalSessionsUseTheSameTargetLifecycle) {
        DragDropService service;
        size_t enters = 0;
        size_t drops = 0;
        auto target = service.registerTarget({
            .propose =
                [](const DragTargetEvent &event) {
                    EXPECT_TRUE(event.external);
                    return DragProposal{DragOperation::copy};
                },
            .onEnter = [&](const DragTargetEvent &) { ++enters; },
            .onDrop =
                [&](const DropEvent &event) {
                    EXPECT_TRUE(event.external);
                    ++drops;
                    return true;
                },
        });
        ASSERT_TRUE(service.beginExternal({
            .position = {10.f, 20.f},
            .payload = textPayload("external"),
        }));

        const std::vector<DropTargetCandidate> candidates{
            {.target = target.id(), .localPosition = {1.f, 2.f}}};
        const auto result = service.drop({
            .position = {11.f, 22.f},
            .candidates = candidates,
        });
        EXPECT_TRUE(result.accepted);
        EXPECT_TRUE(result.external);
        EXPECT_EQ(enters, 1U);
        EXPECT_EQ(drops, 1U);
    }

    TEST(DragDropServiceTests,
         LazyPayloadMayCancelItsSessionWithoutInvalidatingCallbackState) {
        DragDropService service{{.dragThreshold = 0.f}};
        size_t completions = 0;
        ASSERT_TRUE(service.arm({
            .source = DragSourceId::generate(),
            .pressPosition = {0.f, 0.f},
            .createPayload =
                [&] {
                    EXPECT_TRUE(service.cancel(DragCompletionReason::escape));
                    return textPayload("unused");
                },
            .callbacks =
                {
                    .onCompleted =
                        [&](const DragCompletedEvent &event) {
                            ++completions;
                            EXPECT_EQ(event.reason,
                                      DragCompletionReason::escape);
                        },
                },
        }));

        DragUpdateResult update;
        EXPECT_NO_THROW(update = service.updatePointer({
                            .position = {1.f, 0.f},
                        }));
        EXPECT_TRUE(update.thresholdCrossed);
        EXPECT_FALSE(service.hasSession());
        EXPECT_EQ(completions, 1U);
    }

    TEST(DragDropServiceTests,
         RemovingActiveTargetPublishesStableTargetRemovedLeave) {
        DragDropService service{{.dragThreshold = 0.f}};
        std::optional<DragLeaveEvent> retainedLeave;
        auto target = service.registerTarget({
            .propose =
                [](const DragTargetEvent &) {
                    return DragProposal{DragOperation::move};
                },
            .onLeave =
                [&](const DragLeaveEvent &event) { retainedLeave = event; },
            .onDrop = [](const DropEvent &) { return true; },
        });
        ASSERT_TRUE(service.arm({
            .source = DragSourceId::generate(),
            .pressPosition = {},
            .payload = textPayload("retained"),
            .threshold = 0.f,
        }));
        const std::vector<DropTargetCandidate> candidates{
            {.target = target.id()}};
        ASSERT_TRUE(service
                        .updatePointer(
                            {.position = {1.f, 0.f}, .candidates = candidates})
                        .accepted);

        EXPECT_TRUE(target.reset());
        ASSERT_TRUE(retainedLeave.has_value());
        EXPECT_EQ(retainedLeave->reason, DragLeaveReason::targetRemoved);
        EXPECT_EQ(*retainedLeave->payload.get(DragFormats::plainText),
                  "retained");
        EXPECT_FALSE(service.activeTarget());
    }

    TEST(DragDropServiceTests, NonFiniteDropCannotCommitAStaleTarget) {
        DragDropService service{{.dragThreshold = 0.f}};
        size_t drops = 0;
        auto target = service.registerTarget({
            .propose =
                [](const DragTargetEvent &) {
                    return DragProposal{DragOperation::move};
                },
            .onDrop =
                [&](const DropEvent &) {
                    ++drops;
                    return true;
                },
        });
        ASSERT_TRUE(service.arm({
            .source = DragSourceId::generate(),
            .pressPosition = {},
            .payload = textPayload(),
            .threshold = 0.f,
        }));
        const std::vector<DropTargetCandidate> candidates{
            {.target = target.id()}};
        ASSERT_TRUE(service
                        .updatePointer(
                            {.position = {1.f, 0.f}, .candidates = candidates})
                        .accepted);

        const auto result = service.drop({
            .position = {std::numeric_limits<float>::quiet_NaN(), 0.f},
            .candidates = candidates,
        });
        EXPECT_EQ(result.reason, DragCompletionReason::rejected);
        EXPECT_FALSE(result.accepted);
        EXPECT_EQ(drops, 0U);
        EXPECT_FALSE(service.hasSession());
    }

    TEST(DragDropServiceTests,
         TargetRegisteredFromDropCallbackIsAvailableOnTheNextGesture) {
        DragDropService service{{.dragThreshold = 0.f}};
        DropTargetRegistration nextTarget;
        size_t nextDrops = 0;
        auto firstTarget = service.registerTarget({
            .propose =
                [](const DragTargetEvent &) {
                    return DragProposal{DragOperation::move};
                },
            .onDrop =
                [&](const DropEvent &) {
                    nextTarget = service.registerTarget({
                        .propose =
                            [](const DragTargetEvent &) {
                                return DragProposal{DragOperation::move};
                            },
                        .onDrop =
                            [&](const DropEvent &) {
                                ++nextDrops;
                                return true;
                            },
                    });
                    return true;
                },
        });
        ASSERT_TRUE(firstTarget);

        ASSERT_TRUE(service.arm({
            .source = DragSourceId::generate(),
            .pressPosition = {},
            .payload = textPayload(),
            .threshold = 0.f,
        }));
        const std::array firstCandidate{
            DropTargetCandidate{.target = firstTarget.id()},
        };
        EXPECT_TRUE(
            service.drop({.position = {1.f, 0.f}, .candidates = firstCandidate})
                .accepted);
        ASSERT_TRUE(nextTarget);

        ASSERT_TRUE(service.arm({
            .source = DragSourceId::generate(),
            .pressPosition = {},
            .payload = textPayload(),
            .threshold = 0.f,
        }));
        const std::array nextCandidate{
            DropTargetCandidate{.target = nextTarget.id()},
        };
        EXPECT_TRUE(
            service.drop({.position = {1.f, 0.f}, .candidates = nextCandidate})
                .accepted);
        EXPECT_EQ(nextDrops, 1U);
    }

    TEST(WidgetTreeDragDropTests,
         DragThresholdCancelsPressedChildAndRoutesDropToAncestorZone) {
        WidgetTree tree;
        tree.setViewportSize({320.f, 200.f});
        bool dropped = false;

        const auto zone = tree.emplaceWidget<DropZone>(
            tree.dragDrop(),
            DropZoneOptions{
                .callbacks = {
                    .propose =
                        [](const DragTargetEvent &event) {
                            return event.payload.has(
                                       DragFormats::plainText.id())
                                       ? DragProposal{DragOperation::move}
                                       : DragProposal{};
                        },
                    .onDrop =
                        [&](const DropEvent &) {
                            dropped = true;
                            return true;
                        },
                },
                .showFeedback = false,
            });
        const auto draggable = tree.emplaceChild<Draggable>(
            zone,
            tree.dragDrop(),
            DraggableOptions{
                .payload = textPayload("row"),
                .threshold = 3.f,
                .draggingCursor = CursorIcon::pointer,
                .allowFromInteractiveDescendants = true,
            });
        const auto button = tree.emplaceChild<Button>(draggable, "Drag me");
        ASSERT_TRUE(tree.mutateLayout(draggable, [](LayoutNode &layout) {
            layout.setWidth(100.f);
            layout.setHeight(32.f);
        }));
        ASSERT_TRUE(tree.mutateLayout(button, [](LayoutNode &layout) {
            layout.setWidth(100.f);
            layout.setHeight(32.f);
        }));
        tree.performLayout();

        const glm::vec2 viewportOffset = tree.getViewportSize() * 0.5f;
        const glm::vec2 pressPosition =
            tree.getBounds(button).center + viewportOffset;
        static_cast<void>(tree.dispatchEvent(Input::MouseButtonEvent{
            .button = MouseButton::left,
            .action = MouseButtonAction::press,
            .pos = pressPosition,
        }));
        ASSERT_TRUE(tree.dragDrop().hasSession());
        ASSERT_NE(tree.getWidget<Button>(button), nullptr);
        EXPECT_TRUE(tree.getWidget<Button>(button)->isPressed());
        EXPECT_EQ(tree.getPointerCapture(), button);

        static_cast<void>(tree.dispatchEvent(Input::MouseMoveEvent{
            .pos = pressPosition + glm::vec2{12.f, 0.f},
        }));
        EXPECT_TRUE(tree.dragDrop().isDragging());
        EXPECT_FALSE(tree.getWidget<Button>(button)->isPressed());
        EXPECT_FALSE(tree.getPointerCapture());
        EXPECT_EQ(tree.getCursorShape(), CursorIcon::pointer);

        const auto release = tree.dispatchEvent(Input::MouseButtonEvent{
            .button = MouseButton::left,
            .action = MouseButtonAction::release,
            .pos = pressPosition + glm::vec2{12.f, 0.f},
        });
        EXPECT_TRUE(release.handled);
        EXPECT_TRUE(dropped);
        EXPECT_FALSE(tree.dragDrop().hasSession());
    }

    TEST(WidgetTreeDragDropTests,
         FarReleaseWithoutAMoveCancelsTheCapturedPress) {
        WidgetTree tree;
        tree.setViewportSize({240.f, 120.f});
        size_t activations = 0;
        const auto draggable = tree.emplaceWidget<Draggable>(
            tree.dragDrop(),
            DraggableOptions{
                .payload = textPayload("row"),
                .threshold = 3.f,
                .allowFromInteractiveDescendants = true,
            });
        const auto button = tree.emplaceChild<Button>(
            draggable, "Drag", [&] { ++activations; });
        ASSERT_TRUE(tree.mutateLayout(draggable, [](LayoutNode &layout) {
            layout.setWidth(100.f);
            layout.setHeight(30.f);
        }));
        ASSERT_TRUE(tree.mutateLayout(button, [](LayoutNode &layout) {
            layout.setWidth(100.f);
            layout.setHeight(30.f);
        }));
        tree.performLayout();
        const glm::vec2 position =
            tree.getBounds(button).center + tree.getViewportSize() * 0.5f;

        static_cast<void>(tree.dispatchEvent(Input::MouseButtonEvent{
            .button = MouseButton::left,
            .action = MouseButtonAction::press,
            .pos = position,
        }));
        ASSERT_TRUE(tree.getWidget<Button>(button)->isPressed());
        ASSERT_EQ(tree.getPointerCapture(), button);

        const auto release = tree.dispatchEvent(Input::MouseButtonEvent{
            .button = MouseButton::left,
            .action = MouseButtonAction::release,
            .pos = position + glm::vec2{30.f, 0.f},
        });
        EXPECT_TRUE(release.handled);
        EXPECT_FALSE(tree.dragDrop().hasSession());
        EXPECT_FALSE(tree.getPointerCapture());
        EXPECT_FALSE(tree.getWidget<Button>(button)->isPressed());
        EXPECT_EQ(activations, 0U);
    }

    TEST(WidgetTreeDragDropTests,
         EscapeCancelsTheCapturedPressBeforeItsReleaseCanActivate) {
        WidgetTree tree;
        tree.setViewportSize({240.f, 120.f});
        size_t activations = 0;
        const auto draggable = tree.emplaceWidget<Draggable>(
            tree.dragDrop(),
            DraggableOptions{
                .payload = textPayload("row"),
                .allowFromInteractiveDescendants = true,
            });
        const auto button = tree.emplaceChild<Button>(
            draggable, "Drag", [&] { ++activations; });
        ASSERT_TRUE(tree.mutateLayout(draggable, [](LayoutNode &layout) {
            layout.setWidth(100.f);
            layout.setHeight(30.f);
        }));
        ASSERT_TRUE(tree.mutateLayout(button, [](LayoutNode &layout) {
            layout.setWidth(100.f);
            layout.setHeight(30.f);
        }));
        tree.performLayout();
        const glm::vec2 position =
            tree.getBounds(button).center + tree.getViewportSize() * 0.5f;

        static_cast<void>(tree.dispatchEvent(Input::MouseButtonEvent{
            .button = MouseButton::left,
            .action = MouseButtonAction::press,
            .pos = position,
        }));
        ASSERT_TRUE(tree.getWidget<Button>(button)->isPressed());
        ASSERT_EQ(tree.getPointerCapture(), button);
        EXPECT_TRUE(tree.dispatchEvent(Input::KeyEvent{
                                           .key = KeyCode::escape,
                                           .action = KeyAction::press,
                                       })
                        .handled);
        EXPECT_FALSE(tree.dragDrop().hasSession());
        EXPECT_FALSE(tree.getPointerCapture());
        EXPECT_FALSE(tree.getWidget<Button>(button)->isPressed());

        static_cast<void>(tree.dispatchEvent(Input::MouseButtonEvent{
            .button = MouseButton::left,
            .action = MouseButtonAction::release,
            .pos = position,
        }));
        EXPECT_EQ(activations, 0U);
    }

    TEST(WidgetTreeDragDropTests,
         ExternalLifecycleUsesTopLeftCoordinatesAndClearCancelsSafely) {
        WidgetTree tree;
        tree.setViewportSize({300.f, 180.f});
        glm::vec2 observedLocal{-1.f, -1.f};
        size_t leaves = 0;

        static_cast<void>(tree.emplaceWidget<DropZone>(
            tree.dragDrop(),
            DropZoneOptions{
                .callbacks = {
                    .propose =
                        [](const DragTargetEvent &) {
                            return DragProposal{DragOperation::copy};
                        },
                    .onOver =
                        [&](const DragTargetEvent &event) {
                            observedLocal = event.localPosition;
                        },
                    .onLeave = [&](const DragLeaveEvent &) { ++leaves; },
                    .onDrop = [](const DropEvent &) { return true; },
                },
                .showFeedback = false,
            }));
        tree.performLayout();

        const auto move = tree.dispatchEvent(ExternalDragEvent{
            .phase = ExternalDragEventPhase::move,
            .payload = textPayload("native"),
            .pos = {75.f, 45.f},
        });
        EXPECT_TRUE(move.handled);
        EXPECT_EQ(observedLocal, (glm::vec2{75.f, 45.f}));
        EXPECT_TRUE(tree.dragDrop().hasSession());

        EXPECT_NO_THROW(tree.clear());
        EXPECT_EQ(leaves, 1U);
        EXPECT_FALSE(tree.dragDrop().hasSession());
    }

    TEST(WidgetTreeDragDropTests,
         ServiceDestructionPublishesTerminalLeaveWithoutDanglingWidgets) {
        WidgetTree tree;
        tree.setViewportSize({200.f, 100.f});
        auto service = std::make_unique<DragDropService>();
        const auto zone = tree.emplaceWidget<DropZone>(
            *service,
            DropZoneOptions{
                .callbacks = {
                    .propose =
                        [](const DragTargetEvent &) {
                            return DragProposal{DragOperation::copy};
                        },
                    .onDrop = [](const DropEvent &) { return true; },
                },
                .showFeedback = false,
            });
        tree.performLayout();
        auto *dropZone = tree.getWidget<DropZone>(zone);
        ASSERT_NE(dropZone, nullptr);

        ASSERT_TRUE(service->beginExternal({
            .position = {},
            .payload = textPayload(),
            .allowedOperations = DragOperation::copy,
        }));
        const std::array candidates{
            DropTargetCandidate{.target = dropZone->dropTargetId()},
        };
        ASSERT_TRUE(service
                        ->updatePointer({
                            .position = {},
                            .candidates = candidates,
                        })
                        .accepted);
        ASSERT_TRUE(dropZone->isDragOver());

        service.reset();
        EXPECT_FALSE(dropZone->isDragOver());
        EXPECT_FALSE(dropZone->dropTargetId());
        EXPECT_NO_THROW(tree.clear());
    }

    TEST(ReorderableListTests,
         ResolvesStableBeforeItemAndCommitsThroughTheModelCallback) {
        DragDropService service;
        WidgetTree tree;
        tree.setViewportSize({300.f, 240.f});

        const auto sourceList = ReorderListId::generate();
        const auto targetList = ReorderListId::generate();
        const auto first = ReorderItemId::generate();
        const auto second = ReorderItemId::generate();
        const auto third = ReorderItemId::generate();
        const auto dragged = ReorderItemId::generate();
        std::optional<ReorderRequest> observed;
        std::vector<ReorderItemId> observedItems;

        const auto list = tree.emplaceWidget<ReorderableList>(
            service,
            ReorderableListOptions{
                .onReorder =
                    [&](const ReorderRequest &request) {
                        observedItems.assign(request.items.begin(),
                                             request.items.end());
                        observed = request;
                        return true;
                    },
            },
            targetList);
        const auto firstWidget = tree.emplaceChild<DraggableListItem>(
            list, service, targetList, first);
        const auto secondWidget = tree.emplaceChild<DraggableListItem>(
            list, service, targetList, second);
        const auto thirdWidget = tree.emplaceChild<DraggableListItem>(
            list, service, targetList, third);
        for (const auto item : {firstWidget, secondWidget, thirdWidget}) {
            ASSERT_TRUE(tree.mutateLayout(item, [](LayoutNode &layout) {
                layout.setWidth(200.f);
                layout.setHeight(40.f);
                layout.setFlexShrink(0.f);
            }));
        }
        tree.performLayout();

        const auto *listWidget = tree.getWidget<ReorderableList>(list);
        ASSERT_NE(listWidget, nullptr);
        const auto firstBounds = tree.getBounds(firstWidget);
        const auto secondBounds = tree.getBounds(secondWidget);
        const float betweenFirstAndSecond =
            (firstBounds.center.y + secondBounds.center.y) * 0.5f;

        DragPayloadBuilder payload;
        ASSERT_TRUE(payload.set(
            DragFormats::reorderItems,
            ReorderDragData{.source = sourceList, .items = {dragged}}));
        ASSERT_TRUE(service.beginExternal({
            .position = {0.f, betweenFirstAndSecond},
            .payload = std::move(payload).build(),
            .allowedOperations = DragOperation::move,
        }));
        const std::vector<DropTargetCandidate> candidates{
            {.target = listWidget->dropTargetId(),
             .localPosition = {150.f, betweenFirstAndSecond + 120.f}}};
        const auto update = service.updatePointer({
            .position = {0.f, betweenFirstAndSecond},
            .candidates = candidates,
        });
        ASSERT_TRUE(update.accepted);
        EXPECT_EQ(listWidget->insertion().before, second);

        const auto result = service.drop({
            .position = {0.f, betweenFirstAndSecond},
            .candidates = candidates,
        });
        EXPECT_TRUE(result.accepted);
        ASSERT_TRUE(observed.has_value());
        EXPECT_EQ(observed->source, sourceList);
        EXPECT_EQ(observed->target, targetList);
        EXPECT_EQ(observed->before, second);
        EXPECT_EQ(observedItems, (std::vector<ReorderItemId>{dragged}));
    }

    TEST(ReorderableListTests, RejectsItemsWithoutStableModelIdentity) {
        DragDropService service;

        EXPECT_THROW(static_cast<void>(DraggableListItem{
                         service, ReorderListId{}, ReorderItemId::generate()}),
                     std::invalid_argument);
        EXPECT_THROW(static_cast<void>(DraggableListItem{
                         service, ReorderListId::generate(), ReorderItemId{}}),
                     std::invalid_argument);
    }

} // namespace
