#include "controls/image.h"
#include "controls/render_view.h"
#include "controls/tree_node.h"
#include "widget_tree.h"

#include <gtest/gtest.h>

#include <functional>
#include <memory>
#include <stdexcept>
#include <string_view>
#include <utility>
#include <vector>

namespace {
    using namespace Bess;
    using namespace Bess::UI;

    class TestTexture final : public Core::Renderer::ITexture {
      public:
        TestTexture(glm::vec2 size, Core::Renderer::TextureHandle handle) {
            setSize(size);
            setHandle(handle);
        }

        void init() override {
        }
        void destroy() override {
        }
        void *getView() const override {
            return nullptr;
        }
    };

    class ImageRecordingPainter final : public UIPainter {
      public:
        [[nodiscard]] glm::vec2 viewportSize() const noexcept override {
            return {800.f, 600.f};
        }
        void drawBox(const BoxPaint &paint) override {
            boxes.push_back(paint);
        }
        void drawText(std::string_view, const TextPaint &) override {
        }
        void drawImage(const ImagePaint &paint) override {
            images.push_back(paint);
        }
        [[nodiscard]] glm::vec2
        measureText(std::string_view, float, float) const override {
            return {};
        }
        void pushClip(WidgetBounds bounds) override {
            clips.push_back(bounds);
        }
        void popClip() override {
            ++clipPops;
        }

        std::vector<BoxPaint> boxes;
        std::vector<ImagePaint> images;
        std::vector<WidgetBounds> clips;
        size_t clipPops = 0;
    };

    class TestRenderDelegate final : public IRenderViewDelegate {
      public:
        void onAttach(RenderView &view) override {
            ++attaches;
            if (attached) {
                attached(view);
            }
        }
        void onDetach(RenderView &view) noexcept override {
            ++detaches;
            if (detached) {
                detached(view);
            }
        }
        void render(RenderViewFrameContext &) override {
            ++renders;
        }

        std::function<void(RenderView &)> attached;
        std::function<void(RenderView &)> detached;
        size_t attaches = 0;
        size_t detaches = 0;
        size_t renders = 0;
    };

    WidgetPaintContext
    paintContext(WidgetTree &tree, UIPainter &painter, WidgetBounds bounds) {
        return {
            .state = tree,
            .painter = painter,
            .bounds = bounds,
        };
    }

    TEST(ImageTests, ResolvesContainAndCoverWithoutGeometryOverflow) {
        WidgetTree tree;
        ImageRecordingPainter painter;
        const auto texture =
            std::make_shared<TestTexture>(glm::vec2{200.f, 100.f}, 7);
        const WidgetBounds slot{.center = {0.f, 0.f}, .size = {100.f, 100.f}};

        Image contained{texture, {.fit = ImageFit::contain}};
        auto containedContext = paintContext(tree, painter, slot);
        contained.paint(containedContext);
        ASSERT_EQ(painter.images.size(), 1u);
        EXPECT_EQ(painter.images.back().bounds.center, glm::vec2(0.f));
        EXPECT_EQ(painter.images.back().bounds.size, glm::vec2(100.f, 50.f));
        EXPECT_EQ(painter.images.back().uvRect, glm::vec4(0.f, 0.f, 1.f, 1.f));

        painter.images.clear();
        Image covered{texture, {.fit = ImageFit::cover}};
        auto coveredContext = paintContext(tree, painter, slot);
        covered.paint(coveredContext);
        ASSERT_EQ(painter.images.size(), 1u);
        EXPECT_EQ(painter.images.back().bounds.center, slot.center);
        EXPECT_EQ(painter.images.back().bounds.size, slot.size);
        EXPECT_EQ(painter.images.back().uvRect,
                  glm::vec4(0.25f, 0.f, 0.75f, 1.f));
        EXPECT_TRUE(painter.clips.empty());
    }

    TEST(ImageTests, ClipsIntrinsicImageAndSupportsEdgeAlignment) {
        WidgetTree tree;
        ImageRecordingPainter painter;
        const auto texture =
            std::make_shared<TestTexture>(glm::vec2{200.f, 100.f}, 8);
        Image image{texture,
                    {.fit = ImageFit::none,
                     .horizontalAlignment = ImageAlignment::end,
                     .verticalAlignment = ImageAlignment::start}};
        auto context = paintContext(
            tree, painter, {.center = {0.f, 0.f}, .size = {100.f, 50.f}});
        image.paint(context);

        ASSERT_EQ(painter.images.size(), 1u);
        EXPECT_EQ(painter.images.front().bounds.center, glm::vec2(-50.f, 25.f));
        ASSERT_EQ(painter.clips.size(), 1u);
        EXPECT_EQ(painter.clipPops, 1u);
    }

    TEST(ImageTests,
         IntrinsicRefreshPreservesCallerOwnedDimensionsAndDynamicBindings) {
        WidgetTree tree;
        tree.setViewportSize({300.f, 200.f});
        auto first = std::make_shared<TestTexture>(glm::vec2{40.f, 20.f}, 10);
        const auto root =
            tree.emplaceWidget<FlexContainer>(FlexContainerOptions{
                .direction = LayoutDirection::vertical,
                .crossAxisAlignment = LayoutAlignment::start,
            });
        const auto imageId = tree.emplaceChild<Image>(root, first);
        tree.performLayout();
        EXPECT_EQ(tree.getBounds(imageId).size, glm::vec2(40.f, 20.f));

        ASSERT_TRUE(tree.mutateLayout(
            imageId, [](LayoutNode &layout) { layout.setWidth(100.f); }));
        first->setSize({80.f, 30.f});
        tree.update(TimeMs{0});
        tree.performLayout();
        EXPECT_EQ(tree.getBounds(imageId).size, glm::vec2(100.f, 30.f));

        ImageRecordingPainter painter;
        std::shared_ptr<Core::Renderer::ITexture> current = first;
        Image dynamic{
            ImageTextureProvider{[&] { return current; }},
            {.fit = ImageFit::fill, .autoSize = false},
        };
        auto context =
            paintContext(tree, painter, {.center = {}, .size = {100.f, 50.f}});
        dynamic.paint(context);
        ASSERT_EQ(painter.images.size(), 1U);
        EXPECT_EQ(painter.images.back().texture->getHandle(), 10U);

        current = std::make_shared<TestTexture>(glm::vec2{100.f, 50.f}, 11);
        dynamic.paint(context);
        ASSERT_EQ(painter.images.size(), 2U);
        EXPECT_EQ(painter.images.back().texture->getHandle(), 11U);
    }

    TEST(RenderViewTests,
         ReentrantDelegateReplacementConvergesAndDetachesExactlyOnce) {
        WidgetTree tree;
        auto first = std::make_shared<TestRenderDelegate>();
        auto skipped = std::make_shared<TestRenderDelegate>();
        auto final = std::make_shared<TestRenderDelegate>();
        first->detached = [final](RenderView &view) {
            view.setDelegate(final);
        };

        const auto id = tree.emplaceWidget<RenderView>(first);
        auto *view = tree.getWidget<RenderView>(id);
        ASSERT_NE(view, nullptr);
        EXPECT_EQ(first->attaches, 1U);
        EXPECT_NO_THROW(view->setDelegate(skipped));
        EXPECT_EQ(view->delegate(), final);
        EXPECT_EQ(first->detaches, 1U);
        EXPECT_EQ(skipped->attaches, 0U);
        EXPECT_EQ(final->attaches, 1U);

        EXPECT_TRUE(tree.removeWidget(id));
        EXPECT_EQ(final->detaches, 1U);
    }

    TEST(RenderViewTests,
         ThrowingAttachStillReconcilesAReentrantDelegateReplacement) {
        WidgetTree tree;
        auto initial = std::make_shared<TestRenderDelegate>();
        auto failing = std::make_shared<TestRenderDelegate>();
        auto replacement = std::make_shared<TestRenderDelegate>();
        const auto id = tree.emplaceWidget<RenderView>(initial);
        auto *view = tree.getWidget<RenderView>(id);
        ASSERT_NE(view, nullptr);

        failing->attached = [replacement](RenderView &mounted) {
            mounted.setDelegate(replacement);
            throw std::runtime_error("attach failure");
        };
        EXPECT_THROW(view->setDelegate(failing), std::runtime_error);
        EXPECT_EQ(initial->detaches, 1U);
        EXPECT_EQ(failing->attaches, 1U);
        EXPECT_EQ(failing->detaches, 1U);
        EXPECT_EQ(view->delegate(), replacement);
        EXPECT_EQ(replacement->attaches, 1U);

        // Assigning the already-configured delegate is a valid lifecycle
        // retry, but a reconciled view must not attach it twice.
        EXPECT_NO_THROW(view->setDelegate(replacement));
        EXPECT_EQ(replacement->attaches, 1U);
        EXPECT_TRUE(tree.removeWidget(id));
        EXPECT_EQ(replacement->detaches, 1U);
    }

    TEST(RenderViewTests, RenderSurfacePermitsOnlyOneMountedProducer) {
        WidgetTree tree;
        const auto surface = std::make_shared<RenderSurface>();
        const auto first = tree.emplaceWidget<RenderView>(
            std::shared_ptr<IRenderViewDelegate>{},
            RenderViewOptions{},
            surface);
        ASSERT_TRUE(first);
        EXPECT_THROW(static_cast<void>(tree.emplaceWidget<RenderView>(
                         std::shared_ptr<IRenderViewDelegate>{},
                         RenderViewOptions{},
                         surface)),
                     std::logic_error);
        EXPECT_TRUE(tree.removeWidget(first));
        EXPECT_TRUE(tree.emplaceWidget<RenderView>(
            std::shared_ptr<IRenderViewDelegate>{},
            RenderViewOptions{},
            surface));
    }

    TEST(TreeNodeTests, CollapsesPrivateHostWithoutOverwritingDescendants) {
        WidgetTree tree;
        size_t notifications = 0;
        bool lastExpanded = true;
        const auto id = tree.emplaceWidget<TreeNode>(
            "Assets",
            TreeNodeOptions{.expanded = false},
            [&notifications, &lastExpanded](bool expanded) {
                ++notifications;
                lastExpanded = expanded;
            });
        auto *node = tree.getWidget<TreeNode>(id);
        ASSERT_NE(node, nullptr);
        const auto content = node->contentRoot();
        ASSERT_TRUE(content);
        EXPECT_EQ(tree.getVisibility(content), WidgetVisibility::collapsed);

        const auto child = tree.emplaceChild<Label>(content, "Textures");
        ASSERT_TRUE(child);
        EXPECT_TRUE(tree.setVisibility(child, WidgetVisibility::hidden));

        EXPECT_TRUE(node->setExpanded(true));
        EXPECT_TRUE(lastExpanded);
        EXPECT_EQ(notifications, 1u);
        EXPECT_EQ(tree.getVisibility(content), WidgetVisibility::visible);
        EXPECT_EQ(tree.getVisibility(child), WidgetVisibility::hidden);

        EXPECT_TRUE(node->setExpanded(false));
        EXPECT_FALSE(lastExpanded);
        EXPECT_EQ(notifications, 2u);
        EXPECT_EQ(tree.getVisibility(content), WidgetVisibility::collapsed);
        EXPECT_EQ(tree.getVisibility(child), WidgetVisibility::hidden);
    }

    TEST(TreeNodeTests, RestrictsItsOwnHitTargetToTheHeader) {
        TreeNode node{"Node", {.headerHeight = 24.f}};
        const WidgetBounds bounds{.center = {0.f, 0.f}, .size = {200.f, 100.f}};

        EXPECT_TRUE(node.hitTest(bounds, {0.f, -40.f}));
        EXPECT_FALSE(node.hitTest(bounds, {0.f, 0.f}));
    }

    TEST(TreeNodeTests, NonCollapsibleNodesArePresentationOnly) {
        TreeNode node{"Leaf", {.collapsible = false}};
        EXPECT_FALSE(node.traits().focusable);
        EXPECT_FALSE(node.traits().hitTestVisible);

        WidgetTree tree;
        WidgetId id;
        id = tree.emplaceWidget<TreeNode>(
            "Self removing", TreeNodeOptions{}, [&](bool) {
                EXPECT_TRUE(tree.removeWidget(id));
            });
        auto *mounted = tree.getWidget<TreeNode>(id);
        ASSERT_NE(mounted, nullptr);
        EXPECT_TRUE(mounted->setExpanded(false));
        EXPECT_FALSE(tree.contains(id));
    }

} // namespace
