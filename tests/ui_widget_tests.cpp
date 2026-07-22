#include "ui_core.h"

#include <gtest/gtest.h>

#include <memory>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace {
    using namespace Bess;
    using namespace Bess::UI;

    struct RoutedEvent {
        std::string widget;
        UIEventPhase phase;
    };

    class ProbeWidget final : public Widget {
      public:
        ProbeWidget(std::string name,
                    glm::vec2 size = {100.f, 100.f},
                    std::vector<RoutedEvent> *events = nullptr,
                    bool interactive = true)
            : m_name(std::move(name)),
              m_size(size),
              m_events(events),
              m_interactive(interactive) {
        }

        std::string_view typeName() const noexcept override {
            return "ProbeWidget";
        }

        WidgetTraits traits() const noexcept override {
            return {.focusable = m_interactive,
                    .hitTestVisible = m_interactive};
        }

        void onMount(WidgetMountContext &context) override {
            context.layout.setWidth(m_size.x);
            context.layout.setHeight(m_size.y);
        }

        UIEventReply onEvent(WidgetEventContext &context,
                             const UIEvent &event) override {
            if (event.is<Input::MouseButtonEvent>() && m_events != nullptr) {
                m_events->push_back({m_name, context.phase});
            }
            if (m_removeOnPress && context.phase == UIEventPhase::target) {
                if (const auto *button = event.getIf<Input::MouseButtonEvent>();
                    button != nullptr &&
                    button->action == MouseButtonAction::press) {
                    context.state.removeWidget(context.id);
                }
            }
            if (m_clearOnPress && context.phase == UIEventPhase::target) {
                if (const auto *button = event.getIf<Input::MouseButtonEvent>();
                    button != nullptr &&
                    button->action == MouseButtonAction::press) {
                    context.state.clear();
                }
            }
            if (m_interactive && context.phase == UIEventPhase::target &&
                event.is<Input::MouseButtonEvent>()) {
                const auto *button = event.getIf<Input::MouseButtonEvent>();
                if (button->action == MouseButtonAction::press) {
                    return {.handled = true,
                            .requestFocus = true,
                            .capturePointer = true};
                }
                if (button->action == MouseButtonAction::release) {
                    return {.handled = true, .releasePointer = true};
                }
            }
            return {};
        }

        void removeOnPress() noexcept {
            m_removeOnPress = true;
        }

        void clearOnPress() noexcept {
            m_clearOnPress = true;
        }

      private:
        std::string m_name;
        glm::vec2 m_size;
        std::vector<RoutedEvent> *m_events = nullptr;
        bool m_interactive = true;
        bool m_removeOnPress = false;
        bool m_clearOnPress = false;
    };

    enum class ThrowingCallback { event, update, arrange };

    class ThrowingCallbackWidget final : public Widget {
      public:
        explicit ThrowingCallbackWidget(ThrowingCallback callback)
            : m_callback(callback) {
        }

        std::string_view typeName() const noexcept override {
            return "ThrowingCallbackWidget";
        }

        WidgetTraits traits() const noexcept override {
            return {.focusable = true, .hitTestVisible = true};
        }

        void onMount(WidgetMountContext &context) override {
            context.layout.setWidth(100.f);
            context.layout.setHeight(60.f);
        }

        UIEventReply onEvent(WidgetEventContext &context,
                             const UIEvent &event) override {
            if (m_callback == ThrowingCallback::event &&
                context.phase == UIEventPhase::target &&
                event.is<Input::MouseButtonEvent>()) {
                static_cast<void>(context.state.removeWidget(context.id));
                throw std::runtime_error("event callback failed");
            }
            return {};
        }

        void update(WidgetUpdateContext &context) override {
            if (m_callback == ThrowingCallback::update) {
                static_cast<void>(context.state.removeWidget(context.id));
                throw std::runtime_error("update callback failed");
            }
        }

        void arrange(WidgetArrangeContext &context) override {
            if (m_callback == ThrowingCallback::arrange) {
                static_cast<void>(context.state.removeWidget(context.id));
                throw std::runtime_error("arrange callback failed");
            }
        }

      private:
        ThrowingCallback m_callback;
    };

    class ChildVisibilityContainer final : public Widget {
      public:
        std::string_view typeName() const noexcept override {
            return "ChildVisibilityContainer";
        }

        WidgetTraits traits() const noexcept override {
            return {.hitTestVisible = false};
        }

        void arrange(WidgetArrangeContext &context) override {
            for (const auto child : context.children()) {
                static_cast<void>(context.setChildVisible(child, false));
            }
        }
    };

    class ThrowingUnmountWidget final : public Widget {
      public:
        std::string_view typeName() const noexcept override {
            return "ThrowingUnmountWidget";
        }

        void onUnmount(WidgetTree &, WidgetId) override {
            throw std::runtime_error("unmount callback failed");
        }
    };

    struct UnmountMutationProbe {
        WidgetId destination;
        bool childInsertionRejected = false;
        bool reparentRejected = false;
        bool duplicateRemovalAccepted = false;
    };

    class UnmountMutationWidget final : public Widget {
      public:
        explicit UnmountMutationWidget(UnmountMutationProbe &probe)
            : m_probe(probe) {
        }

        void onUnmount(WidgetTree &state, WidgetId id) override {
            m_probe.childInsertionRejected =
                !state.addWidget(std::make_unique<Label>("Late child"), id);
            m_probe.reparentRejected =
                !state.reparentWidget(id, m_probe.destination);
            m_probe.duplicateRemovalAccepted = state.removeWidget(id);
        }

      private:
        UnmountMutationProbe &m_probe;
    };

    class RecordingPainter final : public UIPainter {
      public:
        glm::vec2 viewportSize() const noexcept override {
            return {400.f, 300.f};
        }

        void drawBox(const BoxPaint &paint) override {
            boxes.push_back(paint);
            boxLayers.push_back(layer);
        }

        void drawText(std::string_view text, const TextPaint &paint) override {
            texts.emplace_back(std::string{text}, paint);
            textLayers.push_back(layer);
        }

        glm::vec2 measureText(std::string_view text,
                              float fontSize,
                              float letterSpacing) const override {
            return {static_cast<float>(text.size()) *
                        (fontSize * 0.6f + letterSpacing),
                    fontSize};
        }

        void pushClip(WidgetBounds bounds) override {
            clips.push_back(bounds);
        }

        void popClip() override {
            ++clipPops;
        }

        void pushLayer(float zOffset) override {
            layerStack.push_back(layer);
            layer += zOffset;
        }

        void popLayer() override {
            layer = layerStack.back();
            layerStack.pop_back();
        }

        std::vector<BoxPaint> boxes;
        std::vector<std::pair<std::string, TextPaint>> texts;
        std::vector<float> boxLayers;
        std::vector<float> textLayers;
        std::vector<WidgetBounds> clips;
        std::vector<float> layerStack;
        float layer = 0.f;
        size_t clipPops = 0;
    };

    Input::MouseButtonEvent mouseButton(glm::vec2 position,
                                        MouseButtonAction action) {
        return {
            .button = MouseButton::left,
            .action = action,
            .pos = position,
        };
    }

    TEST(WidgetTreeTests,
         OwnsHierarchyRejectsCyclesAndInvalidatesStaleHandles) {
        WidgetTree state;
        state.setViewportSize({400.f, 300.f});

        const WidgetId root =
            state.emplaceWidget<ProbeWidget>("root", glm::vec2{100.f});
        const WidgetId child =
            state.emplaceChild<ProbeWidget>(root, "child", glm::vec2{80.f});
        const WidgetId grandchild = state.emplaceChild<ProbeWidget>(
            child, "grandchild", glm::vec2{40.f});

        ASSERT_TRUE(root);
        ASSERT_TRUE(child);
        ASSERT_TRUE(grandchild);
        EXPECT_EQ(state.getParent(child), root);
        EXPECT_EQ(state.getParent(grandchild), child);
        EXPECT_FALSE(state.reparentWidget(root, grandchild));
        EXPECT_EQ(state.getParent(root), WidgetId{});

        const PickingId stalePicking = state.getPickingId(child);
        ASSERT_TRUE(stalePicking.isValid());
        EXPECT_EQ(state.resolvePickingId(stalePicking), child);

        EXPECT_TRUE(state.removeWidget(root));
        EXPECT_FALSE(state.contains(root));
        EXPECT_FALSE(state.contains(child));
        EXPECT_FALSE(state.contains(grandchild));
        EXPECT_FALSE(state.resolvePickingId(stalePicking));

        const WidgetId replacement =
            state.emplaceWidget<ProbeWidget>("replacement");
        EXPECT_NE(state.getPickingId(replacement).runtimeId,
                  stalePicking.runtimeId);
    }

    TEST(WidgetTreeTests, RoutesCaptureTargetBubbleAndHonorsPointerCapture) {
        WidgetTree state;
        state.setViewportSize({400.f, 300.f});
        std::vector<RoutedEvent> events;

        const WidgetId root = state.emplaceWidget<ProbeWidget>(
            "root", glm::vec2{400.f, 300.f}, &events);
        const WidgetId child = state.emplaceChild<ProbeWidget>(
            root, "child", glm::vec2{100.f, 80.f}, &events);
        state.performLayout();

        const auto childBounds = state.getBounds(child);
        const glm::vec2 surfacePosition =
            childBounds.center + state.getViewportSize() * 0.5f;
        const auto press = state.dispatchEvent(
            mouseButton(surfacePosition, MouseButtonAction::press));

        ASSERT_EQ(press.target, child);
        ASSERT_TRUE(press.handled);
        ASSERT_EQ(events.size(), 3);
        EXPECT_EQ(events[0].widget, "root");
        EXPECT_EQ(events[0].phase, UIEventPhase::capture);
        EXPECT_EQ(events[1].widget, "child");
        EXPECT_EQ(events[1].phase, UIEventPhase::target);
        EXPECT_EQ(events[2].widget, "root");
        EXPECT_EQ(events[2].phase, UIEventPhase::bubble);
        EXPECT_EQ(state.getFocusedWidget(), child);
        EXPECT_EQ(state.getPointerCapture(), child);

        events.clear();
        const auto release = state.dispatchEvent(
            mouseButton({600.f, 500.f}, MouseButtonAction::release));
        EXPECT_EQ(release.target, child);
        EXPECT_FALSE(state.getPointerCapture());
        ASSERT_EQ(events.size(), 3);
        EXPECT_EQ(events[1].widget, "child");
    }

    TEST(WidgetTreeTests, DefersSelfRemovalUntilEventCallbackCompletes) {
        WidgetTree state;
        state.setViewportSize({200.f, 100.f});
        const WidgetId id = state.emplaceWidget<ProbeWidget>("self");
        ASSERT_TRUE(id);
        state.getWidget<ProbeWidget>(id)->removeOnPress();
        state.performLayout();

        const auto result = state.dispatchEvent(
            mouseButton({100.f, 50.f}, MouseButtonAction::press));
        EXPECT_EQ(result.target, id);
        EXPECT_TRUE(result.handled);
        EXPECT_FALSE(state.contains(id));
    }

    TEST(WidgetTreeTests, DefersWholeTreeClearUntilCallbackCompletes) {
        WidgetTree state;
        state.setViewportSize({200.f, 100.f});
        const WidgetId root = state.emplaceWidget<ProbeWidget>("root");
        const WidgetId child = state.emplaceChild<ProbeWidget>(root, "child");
        ASSERT_TRUE(root && child);
        state.getWidget<ProbeWidget>(child)->clearOnPress();
        state.performLayout();
        const auto childBounds = state.getBounds(child);
        const glm::vec2 surfacePosition =
            childBounds.center + state.getViewportSize() * 0.5f;

        static_cast<void>(state.dispatchEvent(
            mouseButton(surfacePosition, MouseButtonAction::press)));
        EXPECT_TRUE(state.getRoots().empty());
        EXPECT_FALSE(state.contains(root));
        EXPECT_FALSE(state.contains(child));
    }

    TEST(WidgetTreeTests,
         RetargetsHoverAndClearsInteractionWhenEligibilityChanges) {
        WidgetTree state;
        state.setViewportSize({240.f, 120.f});
        const WidgetId stack = state.emplaceWidget<StackContainer>();
        const WidgetId back = state.emplaceChild<Button>(stack, "Back");
        const WidgetId front = state.emplaceChild<Button>(stack, "Front");
        ASSERT_TRUE(stack && back && front);
        state.performLayout();

        static_cast<void>(
            state.dispatchEvent(Input::MouseMoveEvent{.pos = {120.f, 60.f}}));
        ASSERT_EQ(state.getHoveredWidget(), front);
        ASSERT_TRUE(state.setFocus(front));
        ASSERT_TRUE(state.capturePointer(front));

        WidgetRef<Button> frontRef{state, front};
        EXPECT_TRUE(frontRef.hide());
        EXPECT_EQ(state.getHoveredWidget(), back);
        EXPECT_FALSE(state.getFocusedWidget());
        EXPECT_FALSE(state.getPointerCapture());

        EXPECT_TRUE(frontRef.show());
        EXPECT_EQ(state.getHoveredWidget(), front);
        EXPECT_TRUE(frontRef.setEnabled(false));
        EXPECT_EQ(state.getHoveredWidget(), back);
        EXPECT_TRUE(frontRef.setEnabled(true));
        EXPECT_EQ(state.getHoveredWidget(), front);
        EXPECT_TRUE(frontRef.setHitTestVisible(false));
        EXPECT_EQ(state.getHoveredWidget(), back);
        EXPECT_TRUE(frontRef.setHitTestVisible(true));
        EXPECT_EQ(state.getHoveredWidget(), front);
    }

    TEST(WidgetTreeTests, ReconcilesFocusAndCaptureAfterCustomArrangement) {
        WidgetTree state;
        state.setViewportSize({200.f, 100.f});
        const WidgetId root = state.emplaceWidget<ChildVisibilityContainer>();
        const WidgetId child = state.emplaceChild<Button>(root, "Hidden");
        ASSERT_TRUE(root && child);
        ASSERT_TRUE(state.setFocus(child));
        ASSERT_TRUE(state.capturePointer(child));

        state.performLayout();
        EXPECT_FALSE(state.getFocusedWidget());
        EXPECT_FALSE(state.getPointerCapture());
        EXPECT_FALSE(state.hitTest({0.f, 0.f}));
    }

    TEST(WidgetTreeTests, ReconcilesInteractionAfterReparenting) {
        WidgetTree state;
        const WidgetId enabledParent = state.emplaceWidget<FlexContainer>();
        const WidgetId disabledParent = state.emplaceWidget<FlexContainer>();
        const WidgetId child =
            state.emplaceChild<Button>(enabledParent, "Focusable");
        ASSERT_TRUE(enabledParent && disabledParent && child);
        ASSERT_TRUE(state.setEnabled(disabledParent, false));
        ASSERT_TRUE(state.setFocus(child));
        ASSERT_TRUE(state.capturePointer(child));

        EXPECT_TRUE(state.reparentWidget(child, disabledParent));
        EXPECT_FALSE(state.getFocusedWidget());
        EXPECT_FALSE(state.getPointerCapture());
    }

    TEST(WidgetTreeTests, FlushesDeferredRemovalWhenCallbacksThrow) {
        WidgetTree state;
        state.setViewportSize({200.f, 100.f});
        const WidgetId eventWidget =
            state.emplaceWidget<ThrowingCallbackWidget>(
                ThrowingCallback::event);
        state.performLayout();
        EXPECT_THROW(static_cast<void>(state.dispatchEvent(
                         mouseButton({100.f, 50.f}, MouseButtonAction::press))),
                     std::runtime_error);
        EXPECT_FALSE(state.contains(eventWidget));

        const WidgetId updateWidget =
            state.emplaceWidget<ThrowingCallbackWidget>(
                ThrowingCallback::update);
        EXPECT_THROW(state.update(TimeMs{1.0}), std::runtime_error);
        EXPECT_FALSE(state.contains(updateWidget));

        const WidgetId arrangeWidget =
            state.emplaceWidget<ThrowingCallbackWidget>(
                ThrowingCallback::arrange);
        EXPECT_THROW(state.performLayout(), std::runtime_error);
        EXPECT_FALSE(state.contains(arrangeWidget));

        // A leaked callback depth would defer this ordinary removal forever.
        const WidgetId replacement =
            state.emplaceWidget<ProbeWidget>("replacement");
        ASSERT_TRUE(replacement);
        EXPECT_TRUE(state.removeWidget(replacement));
        EXPECT_FALSE(state.contains(replacement));
    }

    TEST(WidgetTreeTests, CompletesStructuralRemovalWhenUnmountThrows) {
        WidgetTree state;
        const WidgetId throwing = state.emplaceWidget<ThrowingUnmountWidget>();
        const WidgetId ordinary = state.emplaceWidget<ProbeWidget>("ordinary");
        ASSERT_TRUE(throwing && ordinary);

        EXPECT_THROW(state.removeWidget(throwing), std::runtime_error);
        EXPECT_FALSE(state.contains(throwing));
        EXPECT_TRUE(state.contains(ordinary));

        const WidgetId anotherThrowing =
            state.emplaceWidget<ThrowingUnmountWidget>();
        ASSERT_TRUE(anotherThrowing);
        EXPECT_THROW(state.clear(), std::runtime_error);
        EXPECT_TRUE(state.getRoots().empty());
        EXPECT_FALSE(state.contains(ordinary));
        EXPECT_FALSE(state.contains(anotherThrowing));
    }

    TEST(WidgetTreeTests, RejectsHierarchyMutationOfUnmountingWidgets) {
        WidgetTree state;
        const WidgetId destination =
            state.emplaceWidget<ProbeWidget>("destination");
        const WidgetId removingRoot =
            state.emplaceWidget<ProbeWidget>("removing root");
        UnmountMutationProbe probe{.destination = destination};
        const WidgetId child = state.addWidget(
            std::make_unique<UnmountMutationWidget>(probe), removingRoot);
        ASSERT_TRUE(destination && removingRoot && child);

        EXPECT_TRUE(state.removeWidget(removingRoot));
        EXPECT_TRUE(probe.childInsertionRejected);
        EXPECT_TRUE(probe.reparentRejected);
        EXPECT_TRUE(probe.duplicateRemovalAccepted);
        EXPECT_FALSE(state.contains(removingRoot));
        EXPECT_FALSE(state.contains(child));
        EXPECT_TRUE(state.getChildren(destination).empty());
    }

    TEST(BasicWidgetTests, ButtonActivatesOnlyOnACompletedInsidePress) {
        WidgetTree state;
        state.setViewportSize({200.f, 100.f});
        size_t activations = 0;
        const WidgetId button = state.emplaceWidget<Button>(
            "Create", [&activations] { ++activations; });
        state.performLayout();

        static_cast<void>(state.dispatchEvent(
            mouseButton({100.f, 50.f}, MouseButtonAction::press)));
        static_cast<void>(state.dispatchEvent(
            mouseButton({100.f, 50.f}, MouseButtonAction::release)));
        EXPECT_EQ(activations, 1);
        EXPECT_EQ(state.getFocusedWidget(), button);

        static_cast<void>(state.dispatchEvent(
            mouseButton({100.f, 50.f}, MouseButtonAction::press)));
        static_cast<void>(
            state.dispatchEvent(Input::MouseMoveEvent{.pos = {300.f, 200.f}}));
        static_cast<void>(state.dispatchEvent(
            mouseButton({300.f, 200.f}, MouseButtonAction::release)));
        EXPECT_EQ(activations, 1);
    }

    TEST(BasicWidgetTests,
         ButtonPaintsOpaqueThemeBackgroundAboveContainingSurface) {
        WidgetTree state;
        state.setViewportSize({200.f, 100.f});
        const WidgetId surface = state.emplaceWidget<Surface>();
        const WidgetId button = state.emplaceChild<Button>(surface, "Create");
        ASSERT_TRUE(surface && button);

        RecordingPainter painter;
        state.paint(painter);

        ASSERT_EQ(painter.boxes.size(), 2);
        ASSERT_EQ(painter.texts.size(), 1);
        const auto &surfacePaint = painter.boxes[0];
        const auto &buttonPaint = painter.boxes[1];
        EXPECT_EQ(surfacePaint.color.toHex(),
                  state.theme().surface.background.toHex());
        EXPECT_EQ(buttonPaint.color.toHex(),
                  state.theme().button.normal.background.toHex());
        EXPECT_NE(buttonPaint.color.toHex(), surfacePaint.color.toHex());
        EXPECT_GT(buttonPaint.color.a, 0.f);
        EXPECT_GT(buttonPaint.zIndex, surfacePaint.zIndex);
        EXPECT_GT(painter.texts[0].second.zIndex, buttonPaint.zIndex);
    }

    TEST(BasicWidgetTests,
         PaintsThroughRendererNeutralPainterAndClipsChildren) {
        WidgetTree state;
        state.setViewportSize({400.f, 300.f});
        const WidgetId panel = state.emplaceWidget<Surface>();
        const WidgetId label = state.emplaceChild<Label>(panel, "Properties");
        state.getLayout(panel)->setZVal(0.25f);
        state.getLayout(label)->setZVal(0.10f);

        RecordingPainter painter;
        state.paint(painter);

        ASSERT_EQ(painter.boxes.size(), 1);
        ASSERT_EQ(painter.texts.size(), 1);
        EXPECT_EQ(painter.boxes[0].borderThickness, glm::vec4(0.f));
        EXPECT_EQ(painter.texts[0].first, "Properties");
        EXPECT_EQ(painter.clips.size(), 1);
        EXPECT_EQ(painter.clipPops, 1);
        ASSERT_EQ(painter.boxLayers.size(), 1);
        ASSERT_EQ(painter.textLayers.size(), 1);
        EXPECT_FLOAT_EQ(painter.boxLayers[0], 0.25f);
        EXPECT_FLOAT_EQ(painter.textLayers[0], 0.35f);
        EXPECT_TRUE(painter.layerStack.empty());
        EXPECT_FLOAT_EQ(painter.layer, 0.f);
    }
} // namespace
