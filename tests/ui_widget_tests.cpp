#include "ui_core.h"

#include <gtest/gtest.h>

#include <memory>
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
