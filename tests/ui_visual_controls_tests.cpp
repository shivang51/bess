#include "controls/card.h"
#include "controls/image.h"
#include "controls/list_view.h"
#include "controls/numeric_input.h"
#include "controls/render_view.h"
#include "controls/scene_view.h"
#include "controls/tree_node.h"
#include "ui_composer.h"
#include "ui_event.h"
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

    class TestSceneDelegate final : public ISceneViewDelegate {
      public:
        void onAttach(SceneView &) override {
            ++attaches;
        }
        void onDetach(SceneView &) noexcept override {
            ++detaches;
        }
        void render(SceneViewFrameContext &frame) override {
            ++renders;
            lastColor = frame.colorTarget;
            lastPicking = frame.pickingTarget;
            lastExtent = frame.extent;
            lastResized = frame.resized;
        }
        UIEventReply onEvent(SceneView &,
                             WidgetEventContext &,
                             const UIEvent &) override {
            ++events;
            return UIEventReply::handledEvent();
        }
        [[nodiscard]] CursorIcon
        cursor(const SceneView &,
               const WidgetCursorContext &) const noexcept override {
            return CursorIcon::move;
        }

        size_t attaches = 0;
        size_t detaches = 0;
        size_t renders = 0;
        size_t events = 0;
        Core::Renderer::TextureHandle lastColor = 0;
        Core::Renderer::TextureHandle lastPicking = 0;
        Core::Renderer::Renderer2DExtent lastExtent{};
        bool lastResized = false;
    };

    class SceneViewTestTexture final : public Core::Renderer::ITexture {
      public:
        SceneViewTestTexture(glm::vec2 size,
                             Core::Renderer::TextureHandle handle) {
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

    class SceneViewTestTarget final : public Core::Renderer::IRenderTarget2D {
      public:
        explicit SceneViewTestTarget(Core::Renderer::Renderer2DExtent extent)
            : m_extent(extent),
              m_color(std::make_shared<SceneViewTestTexture>(
                  glm::vec2{static_cast<float>(extent.width),
                            static_cast<float>(extent.height)},
                  42)),
              m_picking(std::make_shared<SceneViewTestTexture>(
                  glm::vec2{static_cast<float>(extent.width),
                            static_cast<float>(extent.height)},
                  43)) {
        }

        void destroy() override {
            destroyed = true;
        }
        void
        resize(const Core::Renderer::Renderer2DExtent &extent) override {
            m_extent = extent;
            m_color->setSize({static_cast<float>(extent.width),
                              static_cast<float>(extent.height)});
            m_picking->setSize({static_cast<float>(extent.width),
                                static_cast<float>(extent.height)});
            ++resizes;
        }
        void
        beginFrame(const Core::Renderer::RenderTarget2DFrameInfo &) override {
            ++begins;
        }
        void endFrame() override {
            ++ends;
        }
        [[nodiscard]] Core::Renderer::Renderer2DExtent
        getExtent() const noexcept override {
            return m_extent;
        }
        [[nodiscard]] std::shared_ptr<Core::Renderer::ITexture>
        getColorTexture() const override {
            return m_color;
        }
        [[nodiscard]] std::shared_ptr<Core::Renderer::ITexture>
        getPickingTexture() const override {
            return m_picking;
        }
        [[nodiscard]] PickingId readPickingId(uint32_t, uint32_t) override {
            return PickingId::invalid();
        }

        Core::Renderer::Renderer2DExtent m_extent;
        std::shared_ptr<SceneViewTestTexture> m_color;
        std::shared_ptr<SceneViewTestTexture> m_picking;
        size_t begins = 0;
        size_t ends = 0;
        size_t resizes = 0;
        bool destroyed = false;
    };

    class SceneViewTestRenderer final : public Core::Renderer::IRenderer2D {
      public:
        void init(const Core::Renderer::Renderer2DCreateInfo &) override {
        }
        void destroy() override {
        }
        [[nodiscard]] Core::Renderer::Renderer2DTargetFormat
        getTargetFormatType() const noexcept override {
            return Core::Renderer::Renderer2DTargetFormat::BGRA8Unorm;
        }
        [[nodiscard]] Core::Renderer::Renderer2DTargetFormat
        getPickingFormatType() const noexcept override {
            return Core::Renderer::Renderer2DTargetFormat::RG32Uint;
        }
        [[nodiscard]] std::shared_ptr<Core::Renderer::IRenderTarget2D>
        createTarget(const Core::Renderer::RenderTarget2DCreateInfo &info)
            override {
            lastTarget = std::make_shared<SceneViewTestTarget>(info.extent);
            return lastTarget;
        }
        void resize(const Core::Renderer::Renderer2DExtent &) override {
        }
        void beginFrame(const Core::Renderer::Renderer2DFrameInfo &) override {
            ++begins;
        }
        void endFrame() override {
            ++ends;
        }
        void clear(const Core::Renderer::Color &) override {
        }
        void saveTargetToFile(const std::string &) override {
        }
        [[nodiscard]] Core::Renderer::Renderer2DStats
        getStats() const noexcept override {
            return {};
        }
        [[nodiscard]] Core::Renderer::TextureReadbackResult
        readTexture(const Core::Renderer::TextureReadbackRegion &) override {
            return {};
        }
        void requestPickingIds(
            const Core::Renderer::TextureReadbackRegion &region) override {
            lastPickingRegion = region;
            ++pickingRequests;
        }
        [[nodiscard]] bool tryGetPickingIds(
            Core::Renderer::PickingReadbackResult &) override {
            return false;
        }
        [[nodiscard]] bool isPickingReadbackPending() const noexcept override {
            return false;
        }
        void
        pushScissorRect(const Core::Renderer::RendererScissorRect &) override {
        }
        void popScissorRect() override {
        }
        void clearScissorRects() override {
        }
        void drawQuad(const Core::Renderer::QuadProps &) override {
        }
        [[nodiscard]] Core::Renderer::CustomQuadShaderHandle
        createCustomQuadShader(
            const Core::Renderer::CustomQuadShaderDesc &) override {
            return 1;
        }
        void destroyCustomQuadShader(
            Core::Renderer::CustomQuadShaderHandle) override {
        }
        void
        drawCustomQuad(const Core::Renderer::CustomQuadProps &) override {
        }
        void drawCircle(const Core::Renderer::CircleProps &) override {
        }
        void drawLine(const Core::Renderer::LineProps &) override {
        }
        void drawFont(std::string_view,
                      const Core::Renderer::FontProps &) override {
        }
        [[nodiscard]] glm::vec2
        measureText(std::string_view,
                    const Core::Renderer::FontProps &) override {
            return {};
        }
        [[nodiscard]] float
        textCenterOffsetX(std::string_view,
                          const Core::Renderer::FontProps &) override {
            return 0.f;
        }
        [[nodiscard]] float
        textCenterOffsetY(std::string_view,
                          const Core::Renderer::FontProps &) override {
            return 0.f;
        }
        void drawPath(std::span<const Core::Renderer::PathCommand>,
                      const Core::Renderer::PathProps &) override {
        }
        void beginPath(const Core::Renderer::PathProps &) override {
        }
        void pathMoveTo(const glm::vec2 &) override {
        }
        void pathLineTo(const glm::vec2 &,
                        const Core::Renderer::PathCommandStroke &) override {
        }
        void pathQuadTo(const glm::vec2 &,
                        const glm::vec2 &,
                        const Core::Renderer::PathCommandStroke &) override {
        }
        void pathCubicTo(const glm::vec2 &,
                         const glm::vec2 &,
                         const glm::vec2 &,
                         const Core::Renderer::PathCommandStroke &) override {
        }
        void pathClose(const Core::Renderer::PathCommandStroke &) override {
        }
        void endPath() override {
        }

        std::shared_ptr<SceneViewTestTarget> lastTarget;
        Core::Renderer::TextureReadbackRegion lastPickingRegion{};
        size_t begins = 0;
        size_t ends = 0;
        size_t pickingRequests = 0;
    };

    TEST(SceneViewTests, MountDetachAndDoesNotBeginTargetFrame) {
        WidgetTree tree;
        tree.setViewportSize({200.f, 100.f});
        auto delegate = std::make_shared<TestSceneDelegate>();
        const auto id = tree.emplaceWidget<SceneView>(delegate);
        auto *view = tree.getWidget<SceneView>(id);
        ASSERT_NE(view, nullptr);
        EXPECT_EQ(delegate->attaches, 1U);
        EXPECT_EQ(view->typeName(), "SceneView");
        EXPECT_TRUE(view->traits().preparesRender);
        EXPECT_TRUE(view->traits().focusable);

        auto renderer = std::make_shared<SceneViewTestRenderer>();
        tree.prepareRender(renderer, TimeMs{16.0}, 1.f);

        ASSERT_NE(renderer->lastTarget, nullptr);
        EXPECT_EQ(renderer->lastTarget->begins, 0U);
        EXPECT_EQ(renderer->lastTarget->ends, 0U);
        EXPECT_EQ(delegate->renders, 1U);
        EXPECT_EQ(delegate->lastColor, 42U);
        EXPECT_EQ(delegate->lastPicking, 43U);
        EXPECT_EQ(delegate->lastExtent.width, 200U);
        EXPECT_EQ(delegate->lastExtent.height, 100U);

        EXPECT_TRUE(view->requestPickingId(1, 2));
        EXPECT_EQ(renderer->pickingRequests, 1U);
        EXPECT_EQ(renderer->lastPickingRegion.texture, 43U);
        EXPECT_EQ(renderer->lastPickingRegion.x, 1U);
        EXPECT_EQ(renderer->lastPickingRegion.y, 2U);

        EXPECT_TRUE(tree.removeWidget(id));
        EXPECT_EQ(delegate->detaches, 1U);
        EXPECT_TRUE(renderer->lastTarget->destroyed);
    }

    TEST(SceneViewTests, ResizeTriggersRenderAndPaintsColorTexture) {
        WidgetTree tree;
        tree.setViewportSize({120.f, 80.f});
        auto delegate = std::make_shared<TestSceneDelegate>();
        const auto id = tree.emplaceWidget<SceneView>(
            delegate, SceneViewOptions{.policy = RenderPolicy::onDemand});
        auto *view = tree.getWidget<SceneView>(id);
        ASSERT_NE(view, nullptr);

        auto renderer = std::make_shared<SceneViewTestRenderer>();
        tree.prepareRender(renderer, TimeMs{16.0}, 1.f);
        EXPECT_EQ(delegate->renders, 1U);
        EXPECT_TRUE(delegate->lastResized);

        // onDemand with no request and no resize should skip.
        tree.prepareRender(renderer, TimeMs{16.0}, 1.f);
        EXPECT_EQ(delegate->renders, 1U);

        view->requestRender();
        tree.prepareRender(renderer, TimeMs{16.0}, 1.f);
        EXPECT_EQ(delegate->renders, 2U);

        tree.setViewportSize({160.f, 90.f});
        tree.prepareRender(renderer, TimeMs{16.0}, 1.f);
        EXPECT_EQ(delegate->renders, 3U);
        EXPECT_EQ(delegate->lastExtent.width, 160U);
        EXPECT_EQ(delegate->lastExtent.height, 90U);

        ImageRecordingPainter painter;
        tree.paint(painter);
        ASSERT_EQ(painter.images.size(), 1U);
        EXPECT_EQ(painter.images.front().texture->getHandle(), 42U);
    }

    TEST(SceneViewTests, RoutesEventsAndCursorThroughDelegate) {
        WidgetTree tree;
        auto delegate = std::make_shared<TestSceneDelegate>();
        const auto id = tree.emplaceWidget<SceneView>(delegate);
        auto *view = tree.getWidget<SceneView>(id);
        ASSERT_NE(view, nullptr);

        WidgetEventContext eventContext{
            .state = tree,
            .id = id,
            .target = id,
            .phase = UIEventPhase::target,
            .bounds = {.center = {50.f, 50.f}, .size = {100.f, 100.f}},
            .pointerPosition = {50.f, 50.f},
            .hasPointerPosition = true,
        };
        const auto reply =
            view->onEvent(eventContext, UIEvent{Input::MouseMoveEvent{}});
        EXPECT_TRUE(reply.handled);
        EXPECT_EQ(delegate->events, 1U);

        WidgetCursorContext cursorContext{
            .state = tree,
            .id = id,
            .bounds = eventContext.bounds,
            .pointerPosition = {50.f, 50.f},
        };
        EXPECT_EQ(view->cursor(cursorContext), CursorIcon::move);
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

    TEST(TreeNodeTests, ContentIndentAlignsWithLabelTextStart) {
        WidgetTree tree;
        tree.setViewportSize({400.f, 300.f});
        const auto id = tree.emplaceWidget<TreeNode>(
            "Root",
            TreeNodeOptions{
                .expanded = true,
                .indentation = 0.f,
                .horizontalPadding = 5.f,
                .disclosureSlotWidth = 17.f,
            });
        tree.performLayout();
        auto *layout = tree.getLayout(id);
        ASSERT_NE(layout, nullptr);
        // Label starts after horizontal padding + disclosure slot.
        EXPECT_FLOAT_EQ(layout->getPadding().left, 22.f);
        EXPECT_FLOAT_EQ(layout->getPadding().top, 24.f);

        EXPECT_TRUE(tree.mutateWidget<TreeNode>(
            id, WidgetInvalidation::layout, [](TreeNode &value) {
                value.setIcon("X");
            }));
        tree.performLayout();
        // With an icon slot the label (and children) shift further right.
        EXPECT_FLOAT_EQ(layout->getPadding().left, 40.f);
    }

    TEST(LabelTests, AutoSizeMatchesMeasuredTextWidth) {
        class NarrowTextPainter final : public UIPainter {
          public:
            explicit NarrowTextPainter(float widthScale = 0.35f)
                : m_widthScale(widthScale) {
            }

            [[nodiscard]] glm::vec2 viewportSize() const noexcept override {
                return {400.f, 200.f};
            }
            void drawBox(const BoxPaint &) override {
            }
            void drawText(std::string_view, const TextPaint &) override {
            }
            [[nodiscard]] glm::vec2
            measureText(std::string_view text,
                        float fontSize,
                        float letterSpacing) const override {
                return {static_cast<float>(text.size()) *
                            (fontSize * m_widthScale + letterSpacing),
                        fontSize};
            }
            void pushClip(WidgetBounds) override {
            }
            void popClip() override {
            }

          private:
            float m_widthScale = 0.35f;
        };

        // Roots always fill the viewport; measure the label as a child so its
        // intrinsic width is the arranged size under test.
        WidgetTree tree;
        tree.setViewportSize({400.f, 200.f});
        const auto root = tree.emplaceWidget<FlexContainer>(FlexContainerOptions{
            .direction = LayoutDirection::vertical,
            .mainAxisAlignment = LayoutAlignment::start,
            .crossAxisAlignment = LayoutAlignment::start,
            .stretchWidth = true,
            .stretchHeight = true,
        });
        const auto id = tree.emplaceChild<Label>(root, "Properties");
        tree.performLayout();

        NarrowTextPainter painter{0.35f};
        tree.paint(painter);
        tree.performLayout();

        const auto style = tree.theme().label;
        const auto expected =
            painter.measureText("Properties", style.fontSize, style.letterSpacing);
        const auto bounds = tree.getBounds(id);
        EXPECT_NEAR(bounds.size.x, expected.x, 0.01f);
        EXPECT_NEAR(bounds.size.y, expected.y, 0.01f);

        // Empty labels collapse to zero width instead of reserving a stub.
        EXPECT_TRUE(tree.mutateWidget<Label>(
            id, WidgetInvalidation::layout | WidgetInvalidation::paint,
            [](Label &label) { label.setText(""); }));
        tree.paint(painter);
        tree.performLayout();
        EXPECT_NEAR(tree.getBounds(id).size.x, 0.f, 0.01f);
    }

    TEST(CardTests, ComposesChildrenUnderPrivateContentHost) {
        WidgetTree tree;
        UIComposer ui{tree};
        auto card = ui.card(CardOptions{.padding = {8.f}}, [](UIComposer &c) {
            c.label("Title");
            c.label("Body");
        });
        ASSERT_TRUE(card);
        auto *widget = card.get();
        ASSERT_NE(widget, nullptr);
        ASSERT_TRUE(widget->contentRoot());
        EXPECT_EQ(tree.getChildren(widget->contentRoot()).size(), 2u);
        EXPECT_EQ(tree.getParent(widget->contentRoot()), card.id());
    }

    TEST(ListViewTests, MutateApiAddsRemovesAndClearsItems) {
        WidgetTree tree;
        UIComposer ui{tree};
        auto list = ui.listView(ListViewOptions{.gap = 2.f}, [](UIComposer &c) {
            c.label("A");
            c.label("B");
        });
        ASSERT_TRUE(list);
        auto *view = list.get();
        ASSERT_NE(view, nullptr);
        EXPECT_EQ(view->itemCount(), 2u);

        WidgetId added;
        EXPECT_TRUE(list.mutate([&](ListView &value) {
            added = value.emplaceItem<Label>("C");
            EXPECT_TRUE(added);
        }));
        EXPECT_EQ(view->itemCount(), 3u);

        // Removals requested inside mutate are deferred until the callback
        // ends, matching WidgetTree's callback lifetime contract.
        EXPECT_TRUE(list.mutate([&](ListView &value) {
            EXPECT_TRUE(value.removeItem(added));
        }));
        EXPECT_EQ(view->itemCount(), 2u);

        EXPECT_TRUE(list.mutate([](ListView &value) {
            EXPECT_TRUE(value.clearItems());
        }));
        EXPECT_EQ(view->itemCount(), 0u);

        EXPECT_TRUE(list.mutate([](ListView &value) {
            EXPECT_TRUE(value.emplaceItem<Label>("Only"));
        }));
        EXPECT_EQ(view->itemCount(), 1u);
        EXPECT_EQ(tree.getChildren(view->contentRoot()).size(), 1u);
    }

    TEST(NumericInputTests, IntegerAndFloatModesFilterAndCommit) {
        WidgetTree tree;
        const auto intId = tree.emplaceWidget<NumericInput>(
            NumericInputKind::integer,
            std::make_shared<NumericModel>(3.0),
            NumericInput::Changed{},
            NumericInput::Submitted{},
            NumericInputOptions{.step = 1.0});
        auto *integer = tree.getWidget<NumericInput>(intId);
        ASSERT_NE(integer, nullptr);
        EXPECT_DOUBLE_EQ(integer->value(), 3.0);
        EXPECT_EQ(integer->text(), "3");
        EXPECT_EQ(integer->typeName(), "IntInput");

        const auto floatId = tree.emplaceWidget<NumericInput>(
            NumericInputKind::floatingPoint,
            std::make_shared<NumericModel>(1.5),
            NumericInput::Changed{},
            NumericInput::Submitted{},
            NumericInputOptions{.step = 0.1, .precision = 2});
        auto *floating = tree.getWidget<NumericInput>(floatId);
        ASSERT_NE(floating, nullptr);
        EXPECT_DOUBLE_EQ(floating->value(), 1.5);
        EXPECT_EQ(floating->typeName(), "FloatInput");

        EXPECT_TRUE(tree.mutateWidget<NumericInput>(
            intId, WidgetInvalidation::paint, [](NumericInput &input) {
                EXPECT_TRUE(input.setValue(10.0));
            }));
        EXPECT_EQ(integer->text(), "10");

        EXPECT_TRUE(tree.mutateWidget<NumericInput>(
            floatId, WidgetInvalidation::paint, [](NumericInput &input) {
                input.setRange(0.0, 2.0, true);
                EXPECT_TRUE(input.setValue(5.0));
                EXPECT_DOUBLE_EQ(input.value(), 2.0);
            }));
    }

    TEST(NumericInputTests, ComposerIntAndFloatHelpers) {
        WidgetTree tree;
        UIComposer ui{tree};
        auto integer = ui.intInput(std::make_shared<NumericModel>(0),
                                   {},
                                   {},
                                   {.placeholder = "int"});
        auto floating = ui.floatInput(std::make_shared<NumericModel>(0.25),
                                      {},
                                      {},
                                      {.placeholder = "float", .precision = 3});
        ASSERT_TRUE(integer);
        ASSERT_TRUE(floating);
        EXPECT_EQ(integer.get()->kind(), NumericInputKind::integer);
        EXPECT_EQ(floating.get()->kind(), NumericInputKind::floatingPoint);
        EXPECT_DOUBLE_EQ(floating.get()->value(), 0.25);
    }

} // namespace
