#include "ui_core.h"
#include "ui_composer.h"

#include <gtest/gtest.h>

#include <algorithm>
#include <memory>
#include <string_view>
#include <vector>

namespace {
    using namespace Bess;
    using namespace Bess::UI;

    class MinimumContent final : public Widget {
      public:
        explicit MinimumContent(glm::vec2 minimum) : m_minimum(minimum) {
        }

        WidgetTraits traits() const noexcept override {
            return {.hitTestVisible = true};
        }

        void onMount(WidgetMountContext &context) override {
            context.layout.setMinSize(m_minimum);
        }

      private:
        glm::vec2 m_minimum;
    };

    class ScrollRecordingPainter final : public UIPainter {
      public:
        glm::vec2 viewportSize() const noexcept override {
            return {300.f, 200.f};
        }

        void drawBox(const BoxPaint &paint) override {
            boxes.push_back(paint);
        }

        void drawText(std::string_view, const TextPaint &) override {
        }

        glm::vec2 measureText(std::string_view, float, float) const override {
            return {};
        }

        void pushClip(WidgetBounds bounds) override {
            clips.push_back(bounds);
        }

        void popClip() override {
        }

        std::vector<BoxPaint> boxes;
        std::vector<WidgetBounds> clips;
    };

    TEST(ScrollViewTests,
         AutoScrollbarsPreserveNestedMinimumContentAndWheelScroll) {
        WidgetTree state;
        state.setViewportSize({300.f, 200.f});
        const WidgetId scrollId = state.emplaceWidget<ScrollView>();
        const WidgetId contentId = state.addWidget(
            std::make_unique<FlexContainer>(FlexContainerOptions{
                .direction = LayoutDirection::vertical,
                .crossAxisAlignment = LayoutAlignment::start,
                .stretchWidth = true,
                .stretchHeight = true,
            }),
            scrollId);
        ASSERT_TRUE(contentId);
        ASSERT_TRUE(state.addWidget(
            std::make_unique<MinimumContent>(glm::vec2{500.f, 400.f}),
            contentId));

        state.performLayout();
        auto *scroll = state.getWidget<ScrollView>(scrollId);
        ASSERT_NE(scroll, nullptr);
        EXPECT_TRUE(scroll->hasHorizontalScrollbar());
        EXPECT_TRUE(scroll->hasVerticalScrollbar());
        EXPECT_GE(scroll->contentExtent().x, 500.f);
        EXPECT_GE(scroll->contentExtent().y, 400.f);
        EXPECT_LT(scroll->viewportBounds().size.x, 300.f);
        EXPECT_LT(scroll->viewportBounds().size.y, 200.f);
        EXPECT_EQ(scroll->scrollOffset(), glm::vec2(0.f));

        const float scrollRight = state.getBounds(scrollId).bottomRight().x;
        const glm::vec2 verticalGutterPoint{
            (scroll->viewportBounds().bottomRight().x + scrollRight) * 0.5f,
            0.f,
        };
        EXPECT_EQ(state.hitTest(verticalGutterPoint), scrollId);

        const auto result = state.dispatchEvent(UIEvent{Input::MouseWheelEvent{
            .pos = {150.f, 100.f}, .offset = {0.f, -1.f}}});
        EXPECT_TRUE(result.handled);
        EXPECT_GT(scroll->scrollOffset().y, 0.f);
        state.performLayout();
        EXPECT_LT(state.getBounds(contentId).topLeft().y,
                  scroll->viewportBounds().topLeft().y);

        const float previousX = scroll->scrollOffset().x;
        const auto horizontal = state.dispatchEvent(
            UIEvent{Input::MouseWheelEvent{.pos = {150.f, 100.f},
                                           .offset = {0.f, -1.f}},
                    Input::Modifiers{.shift = true}});
        EXPECT_TRUE(horizontal.handled);
        EXPECT_GT(scroll->scrollOffset().x, previousX);
    }

    TEST(ScrollViewTests, ThumbDragCapturesPointerAndReachesVerticalExtent) {
        WidgetTree state;
        state.setViewportSize({300.f, 200.f});
        const WidgetId scrollId = state.emplaceWidget<ScrollView>();
        ASSERT_TRUE(state.addWidget(
            std::make_unique<MinimumContent>(glm::vec2{500.f, 500.f}),
            scrollId));
        state.performLayout();

        auto *scroll = state.getWidget<ScrollView>(scrollId);
        ASSERT_NE(scroll, nullptr);
        ASSERT_TRUE(scroll->hasVerticalScrollbar());

        ScrollRecordingPainter painter;
        state.paint(painter);
        ASSERT_FALSE(painter.clips.empty());
        EXPECT_EQ(painter.clips.front().center,
                  scroll->viewportBounds().center);
        EXPECT_EQ(painter.clips.front().size, scroll->viewportBounds().size);
        const auto thumbColor = state.theme().scroll.thumb.background.toHex();
        const auto verticalThumb =
            std::find_if(painter.boxes.begin(),
                         painter.boxes.end(),
                         [thumbColor](const BoxPaint &box) {
                             return box.color.toHex() == thumbColor &&
                                    box.bounds.size.y > box.bounds.size.x;
                         });
        ASSERT_NE(verticalThumb, painter.boxes.end());

        const glm::vec2 surfaceOffset = state.getViewportSize() * 0.5f;
        const glm::vec2 press = verticalThumb->bounds.center + surfaceOffset;
        static_cast<void>(state.dispatchEvent(Input::MouseButtonEvent{
            .button = MouseButton::left,
            .action = MouseButtonAction::press,
            .pos = press,
        }));
        EXPECT_EQ(state.getPointerCapture(), scrollId);

        const glm::vec2 bottom{
            press.x,
            state.getViewportSize().y - 1.f,
        };
        static_cast<void>(
            state.dispatchEvent(Input::MouseMoveEvent{.pos = bottom}));
        static_cast<void>(state.dispatchEvent(Input::MouseButtonEvent{
            .button = MouseButton::left,
            .action = MouseButtonAction::release,
            .pos = bottom,
        }));
        EXPECT_FALSE(state.getPointerCapture());
        EXPECT_NEAR(
            scroll->scrollOffset().y, scroll->maximumScrollOffset().y, 0.01f);
    }

    TEST(ScrollViewTests, ContentThatFitsDoesNotReserveScrollbarGutters) {
        WidgetTree state;
        state.setViewportSize({300.f, 200.f});
        const WidgetId scrollId = state.emplaceWidget<ScrollView>();
        ASSERT_TRUE(state.addWidget(
            std::make_unique<MinimumContent>(glm::vec2{100.f, 80.f}),
            scrollId));
        state.performLayout();

        const auto *scroll = state.getWidget<ScrollView>(scrollId);
        ASSERT_NE(scroll, nullptr);
        EXPECT_FALSE(scroll->hasHorizontalScrollbar());
        EXPECT_FALSE(scroll->hasVerticalScrollbar());
        EXPECT_EQ(scroll->viewportBounds().size, state.getViewportSize());
    }

    TEST(ScrollViewTests, NestedScrollViewDoesNotInflateOuterOverflow) {
        // DockPanel is a ScrollView; catalog content nests another ScrollView
        // for the tree. Outer must treat the inner host as an opaque box.
        WidgetTree state;
        state.setViewportSize({300.f, 200.f});
        const WidgetId outerId = state.emplaceWidget<ScrollView>(
            ScrollViewOptions{.horizontal = false, .vertical = true});
        const WidgetId shellId = state.addWidget(
            std::make_unique<FlexContainer>(FlexContainerOptions{
                .direction = LayoutDirection::vertical,
                .crossAxisAlignment = LayoutAlignment::start,
                .stretchWidth = true,
                .stretchHeight = true,
            }),
            outerId);
        ASSERT_TRUE(shellId);
        ASSERT_TRUE(state.emplaceChild<Label>(shellId, "Title"));
        const WidgetId innerId = state.addWidget(
            std::make_unique<ScrollView>(
                ScrollViewOptions{.horizontal = false, .vertical = true}),
            shellId);
        ASSERT_TRUE(innerId);
        auto *innerLayout = state.getLayout(innerId);
        ASSERT_NE(innerLayout, nullptr);
        innerLayout->setWidthPercent(1.f);
        innerLayout->setFlexGrow(1.f);
        innerLayout->setFlexShrink(1.f);
        innerLayout->setFlexBasis(0.f);
        innerLayout->setMinSize({0.f, 0.f});

        const WidgetId listId = state.addWidget(
            std::make_unique<FlexContainer>(FlexContainerOptions{
                .direction = LayoutDirection::vertical,
                .crossAxisAlignment = LayoutAlignment::start,
                .stretchWidth = true,
                .stretchHeight = false,
            }),
            innerId);
        ASSERT_TRUE(listId);
        for (int i = 0; i < 40; ++i) {
            ASSERT_TRUE(state.emplaceChild<Label>(
                listId, "Item " + std::to_string(i)));
        }
        if (auto *listLayout = state.getLayout(listId)) {
            listLayout->setHeightFitContent();
            listLayout->setWidthPercent(1.f);
        }

        state.performLayout();
        state.performLayout();

        auto *outer = state.getWidget<ScrollView>(outerId);
        auto *inner = state.getWidget<ScrollView>(innerId);
        ASSERT_NE(outer, nullptr);
        ASSERT_NE(inner, nullptr);
        EXPECT_FALSE(outer->hasVerticalScrollbar())
            << "outer maxY=" << outer->maximumScrollOffset().y
            << " contentY=" << outer->contentExtent().y
            << " viewportY=" << outer->viewportBounds().size.y;
        EXPECT_FLOAT_EQ(outer->maximumScrollOffset().y, 0.f);
        EXPECT_TRUE(inner->hasVerticalScrollbar());
        EXPECT_GT(inner->maximumScrollOffset().y, 0.f);
    }

    TEST(ScrollViewTests, EmptyStretchContentDoesNotShowStickyScrollbars) {
        WidgetTree state;
        state.setViewportSize({300.f, 200.f});
        const WidgetId scrollId = state.emplaceWidget<ScrollView>();
        // Matches typical catalog usage: a stretch-width / non-stretch-height
        // column that is empty, or a stretch-height column filling the view.
        ASSERT_TRUE(state.addWidget(
            std::make_unique<FlexContainer>(FlexContainerOptions{
                .direction = LayoutDirection::vertical,
                .crossAxisAlignment = LayoutAlignment::start,
                .stretchWidth = true,
                .stretchHeight = true,
            }),
            scrollId));
        state.performLayout();
        // Second layout pass: ScrollView previously wrote contentExtent back
        // onto the content root; that size must not be re-read as overflow.
        state.performLayout();

        const auto *scroll = state.getWidget<ScrollView>(scrollId);
        ASSERT_NE(scroll, nullptr);
        EXPECT_FALSE(scroll->hasHorizontalScrollbar());
        EXPECT_FALSE(scroll->hasVerticalScrollbar());
        EXPECT_FLOAT_EQ(scroll->maximumScrollOffset().x, 0.f);
        EXPECT_FLOAT_EQ(scroll->maximumScrollOffset().y, 0.f);
        EXPECT_EQ(scroll->viewportBounds().size, state.getViewportSize());
    }

    TEST(ScrollViewTests,
         StackWithColumnAndLabelDoesNotCreateSpuriousHorizontalScrollbar) {
        WidgetTree state;
        state.setViewportSize({320.f, 240.f});
        const WidgetId scrollId = state.emplaceWidget<ScrollView>();
        UIComposer ui{state, scrollId};
        ui.stack([](UIComposer &overlay) {
            overlay.column([](UIComposer &column) {
                column.label("Project Explorer");
                column.label("Migrating from ImGui — placeholder");
            });
        });
        state.performLayout();

        const auto *scroll = state.getWidget<ScrollView>(scrollId);
        ASSERT_NE(scroll, nullptr);
        EXPECT_FALSE(scroll->hasHorizontalScrollbar())
            << "contentExtent.x=" << scroll->contentExtent().x
            << " viewport.x=" << scroll->viewportBounds().size.x;
        EXPECT_LE(scroll->contentExtent().x,
                  scroll->viewportBounds().size.x + 0.5f);
        EXPECT_EQ(scroll->scrollOffset(), glm::vec2(0.f));
    }

    TEST(ScrollViewTests,
         HorizontalOverflowDoesNotPromoteFlexibleHeightToVerticalOverflow) {
        WidgetTree state;
        state.setViewportSize({300.f, 200.f});
        const WidgetId scrollId = state.emplaceWidget<ScrollView>();
        const WidgetId contentId = state.addWidget(
            std::make_unique<FlexContainer>(FlexContainerOptions{
                .direction = LayoutDirection::vertical,
                .crossAxisAlignment = LayoutAlignment::start,
                .stretchWidth = true,
                .stretchHeight = true,
            }),
            scrollId);
        ASSERT_TRUE(contentId);
        ASSERT_TRUE(state.addWidget(
            std::make_unique<MinimumContent>(glm::vec2{500.f, 20.f}),
            contentId));
        ASSERT_TRUE(state.addWidget(std::make_unique<Spacer>(), contentId));
        ASSERT_TRUE(state.addWidget(
            std::make_unique<MinimumContent>(glm::vec2{40.f, 20.f}),
            contentId));

        state.performLayout();
        const auto *scroll = state.getWidget<ScrollView>(scrollId);
        ASSERT_NE(scroll, nullptr);
        EXPECT_TRUE(scroll->hasHorizontalScrollbar());
        EXPECT_FALSE(scroll->hasVerticalScrollbar());
        EXPECT_FLOAT_EQ(scroll->maximumScrollOffset().y, 0.f);
    }

    TEST(ScrollViewTests, HorizontalGutterStillRevealsGenuineVerticalOverflow) {
        WidgetTree state;
        state.setViewportSize({300.f, 200.f});
        const WidgetId scrollId = state.emplaceWidget<ScrollView>();
        ASSERT_TRUE(state.addWidget(
            std::make_unique<MinimumContent>(glm::vec2{500.f, 190.f}),
            scrollId));

        state.performLayout();
        const auto *scroll = state.getWidget<ScrollView>(scrollId);
        ASSERT_NE(scroll, nullptr);
        EXPECT_TRUE(scroll->hasHorizontalScrollbar());
        EXPECT_TRUE(scroll->hasVerticalScrollbar());
        EXPECT_GT(scroll->maximumScrollOffset().y, 0.f);
    }

    TEST(ScrollViewTests,
         FixedContentDimensionRemainsOverflowAfterCrossAxisReflow) {
        WidgetTree state;
        state.setViewportSize({300.f, 200.f});
        const WidgetId scrollId = state.emplaceWidget<ScrollView>();
        const WidgetId contentId = state.addWidget(
            std::make_unique<MinimumContent>(glm::vec2{}), scrollId);
        ASSERT_TRUE(contentId);
        auto *layout = state.getLayout(contentId);
        ASSERT_NE(layout, nullptr);
        layout->setWidth(500.f);
        layout->setHeight(200.f);

        state.performLayout();
        const auto *scroll = state.getWidget<ScrollView>(scrollId);
        ASSERT_NE(scroll, nullptr);
        EXPECT_TRUE(scroll->hasHorizontalScrollbar());
        EXPECT_TRUE(scroll->hasVerticalScrollbar());
        EXPECT_GE(scroll->contentExtent().y, 200.f);
    }

    TEST(ScrollViewTests, DisabledAxisCannotBeScrolled) {
        WidgetTree state;
        state.setViewportSize({300.f, 200.f});
        const WidgetId scrollId = state.emplaceWidget<ScrollView>(
            ScrollViewOptions{.horizontal = false, .vertical = true});
        ASSERT_TRUE(state.addWidget(
            std::make_unique<MinimumContent>(glm::vec2{500.f, 500.f}),
            scrollId));
        state.performLayout();

        auto *scroll = state.getWidget<ScrollView>(scrollId);
        ASSERT_NE(scroll, nullptr);
        EXPECT_FALSE(scroll->hasHorizontalScrollbar());
        EXPECT_TRUE(scroll->hasVerticalScrollbar());
        EXPECT_FLOAT_EQ(scroll->maximumScrollOffset().x, 0.f);
        EXPECT_FALSE(scroll->setScrollOffset({100.f, 0.f}));
        EXPECT_FLOAT_EQ(scroll->scrollOffset().x, 0.f);
    }

    TEST(ScrollViewTests, ComposerRejectsMultipleContentRootsAtomically) {
        WidgetTree state;
        UIComposer composer{state};

        EXPECT_THROW(composer.scrollView([](UIComposer &content) {
            content.label("First");
            content.label("Second");
        }),
                     std::logic_error);
        EXPECT_TRUE(state.getRoots().empty());
    }
} // namespace
