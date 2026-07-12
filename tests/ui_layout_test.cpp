#include "bess_core/g_app_context.h"
#include "bess_core/renderer/renderer_2d.h"
#include "bess_core/scene/layers/ui_components_layer.h"
#include "bess_core/scene/scene_event.h"
#include "bess_core/scene/scene_state/scene_state.h"
#include "bess_core/scene/scene_ui/controls/container_comp.h"
#include "bess_core/scene/scene_ui/controls/editable_label_comp.h"
#include "bess_core/scene/scene_ui/controls/label_comp.h"
#include "bess_core/scene/scene_ui/controls/spacer_comp.h"
#include "bess_core/scene/scene_ui/controls/text_box_comp.h"
#include "bess_core/scene/scene_ui/layout.h"
#include "bess_core/scene/scene_ui/ui_view.h"
#include "bess_core/scene/widgets/scene_widgets.h"
#include "bess_core/scene/widgets/scene_widgets_internal.h"
#include "bess_core/style/bess_theme.h"
#include "common/bess_uuid.h"
#include "common/logger.h"
#include "event_dispatcher.h"
#include <gtest/gtest.h>
#include <memory>
#include <vector>

namespace {
    void expectVec2(const glm::vec2 &actual, float expectedX, float expectedY) {
        EXPECT_FLOAT_EQ(actual.x, expectedX);
        EXPECT_FLOAT_EQ(actual.y, expectedY);
    }

    Bess::Core::Renderer::ShadowProps testShadow() {
        return {
            .enabled = true,
            .offset = {2.f, 3.f},
            .blur = 5.f,
            .spread = 1.f,
            .color = Bess::Core::Style::Color{0.1f, 0.2f, 0.3f, 0.4f},
        };
    }

    void expectShadow(const Bess::Core::Renderer::ShadowProps &actual,
                      const Bess::Core::Renderer::ShadowProps &expected) {
        EXPECT_EQ(actual.enabled, expected.enabled);
        expectVec2(actual.offset, expected.offset.x, expected.offset.y);
        EXPECT_FLOAT_EQ(actual.blur, expected.blur);
        EXPECT_FLOAT_EQ(actual.spread, expected.spread);
        EXPECT_EQ(actual.color.toHex(), expected.color.toHex());
    }

    glm::vec2 boxEdges(const glm::vec4 &edges) {
        return {edges.y + edges.w, edges.x + edges.z};
    }

    void setPointSize(Bess::Canvas::UI::UINode &node, const glm::vec2 &size) {
        node.setWidth(size.x);
        node.setHeight(size.y);
    }

    void setFitContent(Bess::Canvas::UI::UINode &node) {
        node.setWidthFitContent();
        node.setHeightFitContent();
    }

    Bess::Canvas::UI::UIElementStyle zeroSpacingStyle() {
        Bess::Canvas::UI::UIElementStyle style;
        style.padding = Bess::Core::Style::Padding::zero();
        style.margin = Bess::Core::Style::Margin::zero();
        return style;
    }

    class LayoutTestRenderer2D final
        : public Bess::Core::Renderer::IRenderer2D {
      public:
        void init(const Bess::Core::Renderer::Renderer2DCreateInfo &) override {
        }
        void destroy() override {
        }
        void resize(const Bess::Core::Renderer::Renderer2DExtent &) override {
        }
        void
        beginFrame(const Bess::Core::Renderer::Renderer2DFrameInfo &) override {
        }
        void endFrame() override {
        }
        void clear(const Bess::Core::Renderer::Color &) override {
        }
        void saveTargetToFile(const std::string &) override {
        }
        [[nodiscard]] Bess::Core::Renderer::Renderer2DStats
        getStats() const noexcept override {
            return {};
        }
        [[nodiscard]] Bess::Core::Renderer::TextureReadbackResult readTexture(
            const Bess::Core::Renderer::TextureReadbackRegion &) override {
            return {};
        }
        void requestPickingIds(
            const Bess::Core::Renderer::TextureReadbackRegion &) override {
        }
        [[nodiscard]] bool tryGetPickingIds(
            Bess::Core::Renderer::PickingReadbackResult &) override {
            return false;
        }
        [[nodiscard]] bool isPickingReadbackPending() const noexcept override {
            return false;
        }
        void pushScissorRect(
            const Bess::Core::Renderer::RendererScissorRect &) override {
        }
        void popScissorRect() override {
        }
        void clearScissorRects() override {
        }
        void drawQuad(const Bess::Core::Renderer::QuadProps &props) override {
            quads.push_back(props);
        }
        [[nodiscard]] Bess::Core::Renderer::CustomQuadShaderHandle
        createCustomQuadShader(
            const Bess::Core::Renderer::CustomQuadShaderDesc &) override {
            return 1;
        }
        void destroyCustomQuadShader(
            Bess::Core::Renderer::CustomQuadShaderHandle) override {
        }
        void
        drawCustomQuad(const Bess::Core::Renderer::CustomQuadProps &) override {
        }
        void drawCircle(const Bess::Core::Renderer::CircleProps &) override {
        }
        void drawLine(const Bess::Core::Renderer::LineProps &) override {
        }
        void
        drawFont(std::string_view,
                 const Bess::Core::Renderer::FontProps &props = {}) override {
            fonts.push_back(props);
        }
        [[nodiscard]] glm::vec2 measureText(
            std::string_view text,
            const Bess::Core::Renderer::FontProps &props = {}) override {
            return getTextRenderSize(text, props);
        }
        [[nodiscard]] float textCenterOffsetY(
            std::string_view,
            const Bess::Core::Renderer::FontProps &props = {}) override {
            return props.fontSize * 0.35f;
        }
        void drawPath(std::span<const Bess::Core::Renderer::PathCommand>,
                      const Bess::Core::Renderer::PathProps & = {}) override {
        }
        void beginPath(const Bess::Core::Renderer::PathProps & = {}) override {
        }
        void pathMoveTo(const glm::vec2 &) override {
        }
        void pathLineTo(
            const glm::vec2 &,
            const Bess::Core::Renderer::PathCommandStroke & = {}) override {
        }
        void pathQuadTo(
            const glm::vec2 &,
            const glm::vec2 &,
            const Bess::Core::Renderer::PathCommandStroke & = {}) override {
        }
        void pathCubicTo(
            const glm::vec2 &,
            const glm::vec2 &,
            const glm::vec2 &,
            const Bess::Core::Renderer::PathCommandStroke & = {}) override {
        }
        void pathClose(
            const Bess::Core::Renderer::PathCommandStroke & = {}) override {
        }
        void endPath() override {
        }

        std::vector<Bess::Core::Renderer::QuadProps> quads;
        std::vector<Bess::Core::Renderer::FontProps> fonts;
    };

    Bess::Canvas::SceneEvent
    keyEvent(Bess::KeyCode key, bool ctrl = false, bool shift = false) {
        Bess::Canvas::SceneEvent::Data data;
        data.keyPress = {.keycode = key, .action = Bess::KeyAction::press};
        return {
            .type = Bess::Canvas::SceneEvent::Type::key,
            .data = data,
            .isCtrlPressed = ctrl,
            .isShiftPressed = shift,
        };
    }

    Bess::Canvas::SceneEvent textInputEvent(char32_t codepoint) {
        Bess::Canvas::SceneEvent::Data data;
        data.textInput = {.codepoint = codepoint};
        return {
            .type = Bess::Canvas::SceneEvent::Type::textInput,
            .data = data,
        };
    }

    Bess::Canvas::SceneEvent mouseMoveEvent(const Bess::PickingId &pickingId) {
        Bess::Canvas::SceneEvent::Data data;
        data.mouseMove = {
            .pos = {0.f, 0.f},
            .delta = {0.f, 0.f},
            .viewportPos = {0.f, 0.f},
        };
        return {
            .type = Bess::Canvas::SceneEvent::Type::mouseMove,
            .data = data,
            .pickingId = pickingId,
        };
    }

    Bess::SceneDrawContext textBoxDrawContext(
        Bess::Canvas::SceneState &sceneState,
        const std::shared_ptr<LayoutTestRenderer2D> &renderer,
        Bess::Canvas::SceneWidgets::SceneWidgetsState &widgetsState) {
        return {
            .sceneState = &sceneState,
            .renderer = renderer,
            .sceneWidgetsState = &widgetsState,
        };
    }

    Bess::SceneDrawContext
    uiDrawContext(Bess::Canvas::SceneState &sceneState,
                  const std::shared_ptr<LayoutTestRenderer2D> &renderer) {
        return {
            .sceneState = &sceneState,
            .renderer = renderer,
        };
    }

    Bess::Canvas::SceneWidgets::TextBoxResult
    drawTextBox(Bess::SceneDrawContext &context,
                const Bess::PickingId &id,
                std::string &value) {
        return Bess::Canvas::SceneWidgets::textBox(
            id, &value, {0.f, 0.f, 0.f}, {180.f, 24.f}, context);
    }

    glm::vec2
    textBoxCursorPointer(const std::shared_ptr<LayoutTestRenderer2D> &renderer,
                         std::string_view text,
                         size_t cursor) {
        constexpr float textBoxLeft = -90.f + 4.f;
        cursor = std::min(cursor, text.size());
        const auto prefix = text.substr(0, cursor);
        return {
            textBoxLeft + renderer->measureText(prefix, {.fontSize = 8.f}).x,
            0.f,
        };
    }

    void focusTextBox(Bess::SceneDrawContext &context,
                      Bess::Canvas::SceneWidgets::SceneWidgetsState &widgets,
                      const Bess::PickingId &id,
                      std::string &value) {
        drawTextBox(context, id, value);
        Bess::Canvas::SceneWidgets::queuePress(&widgets, id, {86.f, 0.f});
        drawTextBox(context, id, value);
        Bess::Canvas::SceneWidgets::queueRelease(&widgets, id, {86.f, 0.f});
        drawTextBox(context, id, value);
    }

    Bess::SceneUIPrepareCtx
    uiPrepareContext(Bess::Canvas::SceneState &sceneState,
                     const std::shared_ptr<LayoutTestRenderer2D> &renderer) {
        return {
            .sceneState = &sceneState,
            .renderer = renderer,
            .parentNode = nullptr,
            .theme = Bess::Core::Style::BessTheme::defaultTheme(),
        };
    }

    void prepareEditableLabel(
        Bess::Canvas::UI::EditableLabelComp &label,
        Bess::Canvas::SceneState &sceneState,
        const std::shared_ptr<LayoutTestRenderer2D> &renderer) {
        auto prepareCtx = uiPrepareContext(sceneState, renderer);
        label.prepareUI(prepareCtx);
        ASSERT_NE(label.getUINode(), nullptr);
        label.getUINode()->measure(*sceneState.getUINodeRegistry(),
                                   Bess::UUID::null);
    }

    void beginEditableLabelEdit(
        Bess::Canvas::UI::EditableLabelComp &label,
        Bess::SceneDrawContext &drawCtx,
        Bess::Canvas::SceneState &sceneState,
        const std::shared_ptr<LayoutTestRenderer2D> &renderer) {
        const auto *node = label.getUINode();
        ASSERT_NE(node, nullptr);
        const auto nodePos = node->getDrawPos();
        const auto nodeSize = node->getDrawSize();
        const auto clickPos = glm::vec2{
            nodePos.x + (nodeSize.x * 0.5f),
            nodePos.y,
        };

        EXPECT_TRUE(label.onMouseButton({
            .mousePos = clickPos,
            .button = Bess::Canvas::Events::MouseButton::left,
            .action = Bess::Canvas::Events::MouseClickAction::doubleClick,
            .details = 0,
            .sceneState = &sceneState,
        }));
        label.onFocusGained({
            .entityUuid = label.getUuid(),
            .mousePos = clickPos,
            .details = 0,
            .sceneState = &sceneState,
        });

        prepareEditableLabel(label, sceneState, renderer);
        label.draw(drawCtx);
        label.draw(drawCtx);
        EXPECT_TRUE(label.wantsKeyboardInput());
    }
} // namespace

class UiLayoutTests : public testing::Test {
  protected:
    void SetUp() override {
        auto &appCtx = Bess::GAppContext::getInstance();
        if (!appCtx.hasSubSystem<Bess::EventSystem::EventDispatcher>()) {
            appCtx.addSubSystem<Bess::EventSystem::EventDispatcher>()->onInit();
        } else {
            appCtx.getSubSystem<Bess::EventSystem::EventDispatcher>()->clear();
        }
    }
};

TEST_F(UiLayoutTests, UINodeRegistryAddGetRemoveNode) {
    Bess::Canvas::UI::UINodeRegistry registry;

    Bess::Canvas::UI::UINode node;
    registry.addNode(node);

    auto retrievedNode = registry.getNode(node.getId());
    ASSERT_NE(retrievedNode, nullptr);
    EXPECT_EQ(retrievedNode->getId(), node.getId());

    registry.removeNode(node.getId());
    retrievedNode = registry.getNode(node.getId());
    EXPECT_EQ(retrievedNode, nullptr);
}

TEST_F(UiLayoutTests, UINodeAddChildReparentsYogaNode) {
    Bess::Canvas::UI::UINodeRegistry registry;

    auto parentNode1 = registry.addNode(Bess::UUID());
    auto parentNode2 = registry.addNode(Bess::UUID());
    auto childNode = registry.addNode(Bess::UUID());
    ASSERT_NE(parentNode1, nullptr);
    ASSERT_NE(parentNode2, nullptr);
    ASSERT_NE(childNode, nullptr);

    setPointSize(*parentNode1, glm::vec2(100, 40));
    setPointSize(*parentNode2, glm::vec2(100, 40));
    setPointSize(*childNode, glm::vec2(20, 10));

    parentNode1->addChild(childNode);
    ASSERT_EQ(childNode->getParentId(), parentNode1->getId());
    EXPECT_EQ(YGNodeGetChildCount(parentNode1->getYogaNode()), 1u);

    parentNode2->addChild(childNode);

    EXPECT_EQ(childNode->getParentId(), parentNode2->getId());
    EXPECT_EQ(parentNode1->getChildren().find(childNode->getId()),
              parentNode1->getChildren().end());
    EXPECT_NE(parentNode2->getChildren().find(childNode->getId()),
              parentNode2->getChildren().end());
    EXPECT_EQ(YGNodeGetChildCount(parentNode1->getYogaNode()), 0u);
    EXPECT_EQ(YGNodeGetChildCount(parentNode2->getYogaNode()), 1u);

    parentNode2->measure(registry, Bess::UUID::null);
    expectVec2(childNode->getDrawSize(), 20, 10);
}

TEST_F(UiLayoutTests, UINodeMeasure) {
    Bess::Canvas::UI::UINodeRegistry registry;

    constexpr glm::vec2 size(100, 50);
    constexpr Bess::Core::Style::Padding padding(10, 5, 9, 6);
    constexpr Bess::Core::Style::Margin margin(2, 3, 4, 5);
    const glm::vec2 paddingSize = boxEdges(padding.toVec4());
    const glm::vec2 marginSize = boxEdges(margin.toVec4());

    Bess::Canvas::UI::UINode node;
    setPointSize(node, size);
    EXPECT_EQ(node.getSizeDirty(), true);

    node.setPadding(padding);
    EXPECT_EQ(node.getSizeDirty(), true);
    EXPECT_EQ(node.getPadding(), padding);

    node.setMargin(margin);
    EXPECT_EQ(node.getSizeDirty(), true);
    EXPECT_EQ(node.getMargin(), margin);

    auto measuredSize = node.measure(registry, Bess::UUID::null);
    auto worldSize = node.getCachedSize();

    expectVec2(measuredSize, worldSize.x, worldSize.y);

    // Fixed size describes the drawn box. Margin contributes to measure size.
    expectVec2(node.getDrawSize(), size.x, size.y);
    expectVec2(worldSize, size.x + marginSize.x, size.y + marginSize.y);

    BESS_INFO("Measured with fixed constraint");

    setFitContent(node);
    measuredSize = node.measure(registry, Bess::UUID::null);
    worldSize = node.getCachedSize();

    // Wrap content uses padding as the drawn box when there are no children.
    expectVec2(measuredSize, worldSize.x, worldSize.y);

    expectVec2(node.getDrawSize(), paddingSize.x, paddingSize.y);
    expectVec2(
        worldSize, paddingSize.x + marginSize.x, paddingSize.y + marginSize.y);

    BESS_INFO("Measured with wrap_content constraint");

    Bess::Canvas::UI::UINode childNode;
    setPointSize(childNode, glm::vec2(20, 10));
    auto childNodePtr = registry.addNode(childNode);
    node.addChild(childNodePtr);

    measuredSize = node.measure(registry, Bess::UUID::null);
    worldSize = node.getCachedSize();

    // Child measure footprints are placed inside the padded content box.
    expectVec2(measuredSize, worldSize.x, worldSize.y);

    expectVec2(node.getDrawSize(), paddingSize.x + 20, paddingSize.y + 10);
    expectVec2(worldSize,
               paddingSize.x + 20 + marginSize.x,
               paddingSize.y + 10 + marginSize.y);

    ASSERT_NE(childNodePtr, nullptr);
    expectVec2(childNodePtr->getCachedSize(), 20, 10);
    expectVec2(childNodePtr->getDrawSize(), 20, 10);

    BESS_INFO("Measured with wrap_content constraint and child node");

    node.setMinSize(glm::vec2(150, 100));
    measuredSize = node.measure(registry, Bess::UUID::null);
    worldSize = node.getCachedSize();

    // Fit-content size with child and min size.
    expectVec2(measuredSize, worldSize.x, worldSize.y);

    expectVec2(node.getDrawSize(), 150, 100);
    expectVec2(worldSize, 150 + marginSize.x, 100 + marginSize.y);

    BESS_INFO("Measured with wrap_content constraint, child node and min size");

    node.setMaxSize(glm::vec2(120, 80));
    measuredSize = node.measure(registry, Bess::UUID::null);
    worldSize = node.getCachedSize();

    // The wrapper reports Yoga's min/max conflict resolution directly.
    expectVec2(measuredSize, worldSize.x, worldSize.y);

    expectVec2(node.getDrawSize(), 150, 80);
    expectVec2(worldSize, 150 + marginSize.x, 80 + marginSize.y);

    node.setMinSize(glm::vec2(50, 40));
    setPointSize(node, glm::vec2(200, 150));

    measuredSize = node.measure(registry, Bess::UUID::null);
    worldSize = node.getCachedSize();

    expectVec2(measuredSize, worldSize.x, worldSize.y);

    expectVec2(node.getDrawSize(), 120, 80);
    expectVec2(worldSize, 120 + marginSize.x, 80 + marginSize.y);

    BESS_INFO("Checked min max size overrides");
}

TEST_F(UiLayoutTests, TextBoxPrepareUIRespectsSizeAndPadding) {
    Bess::Canvas::SceneState sceneState;
    const auto renderer = std::make_shared<LayoutTestRenderer2D>();
    Bess::SceneUIPrepareCtx prepareCtx{
        .sceneState = &sceneState,
        .renderer = renderer,
        .parentNode = nullptr,
        .theme = Bess::Core::Style::BessTheme::defaultTheme(),
    };

    Bess::Canvas::UI::TextBoxComp fixedBox;
    fixedBox.setValue("1234567890");
    fixedBox.setTextBoxSize({80.f, 24.f});
    fixedBox.getStyle().padding =
        Bess::Core::Style::Padding::fromSymmetric(9.f, 4.f);
    fixedBox.getStyle().margin = Bess::Core::Style::Margin(1.f, 2.f, 3.f, 4.f);

    fixedBox.prepareUI(prepareCtx);
    auto *fixedNode = fixedBox.getUINode();
    ASSERT_NE(fixedNode, nullptr);
    fixedNode->measure(*sceneState.getUINodeRegistry(), Bess::UUID::null);

    expectVec2(fixedNode->getDrawSize(), 80.f, 24.f);
    EXPECT_EQ(fixedNode->getPadding(), Bess::Core::Style::Padding::zero());
    EXPECT_EQ(fixedNode->getMargin(),
              Bess::Core::Style::Margin(1.f, 2.f, 3.f, 4.f));

    Bess::Canvas::UI::TextBoxComp autoBox;
    autoBox.setPlaceholder("abcdefghij");
    autoBox.setTextBoxSize({0.f, 0.f});
    autoBox.getStyle().fontSize = 10.f;
    autoBox.getStyle().padding =
        Bess::Core::Style::Padding::fromSymmetric(5.f, 3.f);

    autoBox.prepareUI(prepareCtx);
    auto *autoNode = autoBox.getUINode();
    ASSERT_NE(autoNode, nullptr);
    autoNode->measure(*sceneState.getUINodeRegistry(), Bess::UUID::null);

    expectVec2(autoNode->getDrawSize(), 70.f, 16.f);
    EXPECT_EQ(autoNode->getPadding(), Bess::Core::Style::Padding::zero());
}

TEST_F(UiLayoutTests, UIElementStyleAppliesLayoutPropertiesToNode) {
    Bess::Canvas::SceneState sceneState;
    const auto renderer = std::make_shared<LayoutTestRenderer2D>();
    Bess::SceneUIPrepareCtx prepareCtx{
        .sceneState = &sceneState,
        .renderer = renderer,
        .parentNode = nullptr,
        .theme = Bess::Core::Style::BessTheme::defaultTheme(),
    };

    Bess::Canvas::UI::TextBoxComp box;
    box.setTextBoxSize({80.f, 24.f});

    Bess::Canvas::UI::UIFlex flex;
    flex.grow = 2.f;
    flex.shrink = 3.f;
    flex.basis = 4.f;

    auto &style = box.getStyle();
    style.width = 120.f;
    style.height = 30.f;
    style.pos = glm::vec2(10.f, 12.f);
    style.posMode = Bess::Canvas::UI::PosMode::absolute;
    style.direction = Bess::Canvas::UI::LayoutDirection::vertical;
    style.mainAxisAlignment = Bess::Canvas::UI::LayoutAlignment::center;
    style.crossAxisAlignment = Bess::Canvas::UI::LayoutAlignment::end;
    style.alignSelf = Bess::Canvas::UI::LayoutSelfAlignment::stretch;
    style.flex = flex;
    style.flexGrow = 5.f;
    style.zVal = 7.f;
    style.drawPivot = Bess::Canvas::UI::DrawPivot::topLeft;

    box.prepareUI(prepareCtx);
    auto *node = box.getUINode();
    ASSERT_NE(node, nullptr);

    node->measure(*sceneState.getUINodeRegistry(), Bess::UUID::null);

    expectVec2(node->getDrawSize(), 120.f, 30.f);
    expectVec2(node->getPos(), 10.f, 12.f);
    EXPECT_EQ(node->getPosMode(), Bess::Canvas::UI::PosMode::absolute);
    EXPECT_EQ(node->getDirection(),
              Bess::Canvas::UI::LayoutDirection::vertical);
    EXPECT_EQ(node->getMainAxisAlignment(),
              Bess::Canvas::UI::LayoutAlignment::center);
    EXPECT_EQ(node->getCrossAxisAlignment(),
              Bess::Canvas::UI::LayoutAlignment::end);
    EXPECT_EQ(node->getAlignSelf(),
              Bess::Canvas::UI::LayoutSelfAlignment::stretch);
    EXPECT_FLOAT_EQ(node->getFlexGrow(), 5.f);
    EXPECT_FLOAT_EQ(node->getFlexShrink(), 3.f);
    EXPECT_FLOAT_EQ(node->getZVal(), 7.f);
    EXPECT_EQ(node->getDrawPivot(), Bess::Canvas::UI::DrawPivot::topLeft);
}

TEST_F(UiLayoutTests, SpacerCompExpandsAcrossHorizontalParent) {
    Bess::Canvas::SceneState sceneState;
    const auto renderer = std::make_shared<LayoutTestRenderer2D>();

    auto rootStyle = zeroSpacingStyle();
    rootStyle.widthMode = Bess::Canvas::UI::LayoutSizeMode::point;
    rootStyle.width = 200.f;
    rootStyle.heightMode = Bess::Canvas::UI::LayoutSizeMode::point;
    rootStyle.height = 40.f;

    const auto childStyle = zeroSpacingStyle();

    auto left = Bess::Canvas::UI::TextBoxComp::create();
    left->setStyle(childStyle);
    left->setTextBoxSize({20.f, 10.f});

    auto spacer = Bess::Canvas::UI::SpacerComp::create();

    auto right = Bess::Canvas::UI::TextBoxComp::create();
    right->setStyle(childStyle);
    right->setTextBoxSize({20.f, 10.f});

    auto root = Bess::Canvas::UI::ContainerComp::create(
        Bess::Canvas::UI::LayoutDirection::horizontal,
        {
            .sceneState = &sceneState,
            .children = {left, spacer, right},
            .style = rootStyle,
        });

    auto prepareCtx = uiPrepareContext(sceneState, renderer);
    root->prepareUI(prepareCtx);

    auto *rootNode = root->getUINode();
    auto *leftNode = left->getUINode();
    auto *spacerNode = spacer->getUINode();
    auto *rightNode = right->getUINode();
    ASSERT_NE(rootNode, nullptr);
    ASSERT_NE(leftNode, nullptr);
    ASSERT_NE(spacerNode, nullptr);
    ASSERT_NE(rightNode, nullptr);

    rootNode->measure(*sceneState.getUINodeRegistry(), Bess::UUID::null);

    expectVec2(rootNode->getDrawSize(), 200.f, 40.f);
    expectVec2(leftNode->getCachedPos(), -90.f, 0.f);
    expectVec2(spacerNode->getDrawSize(), 160.f, 0.f);
    expectVec2(rightNode->getCachedPos(), 90.f, 0.f);
}

TEST_F(UiLayoutTests, SpacerCompExpandsAcrossVerticalParent) {
    Bess::Canvas::SceneState sceneState;
    const auto renderer = std::make_shared<LayoutTestRenderer2D>();

    auto rootStyle = zeroSpacingStyle();
    rootStyle.widthMode = Bess::Canvas::UI::LayoutSizeMode::point;
    rootStyle.width = 40.f;
    rootStyle.heightMode = Bess::Canvas::UI::LayoutSizeMode::point;
    rootStyle.height = 100.f;

    const auto childStyle = zeroSpacingStyle();

    auto top = Bess::Canvas::UI::TextBoxComp::create();
    top->setStyle(childStyle);
    top->setTextBoxSize({20.f, 10.f});

    auto spacer = Bess::Canvas::UI::SpacerComp::create();

    auto bottom = Bess::Canvas::UI::TextBoxComp::create();
    bottom->setStyle(childStyle);
    bottom->setTextBoxSize({20.f, 10.f});

    auto root = Bess::Canvas::UI::ContainerComp::create(
        Bess::Canvas::UI::LayoutDirection::vertical,
        {
            .sceneState = &sceneState,
            .children = {top, spacer, bottom},
            .style = rootStyle,
        });

    auto prepareCtx = uiPrepareContext(sceneState, renderer);
    root->prepareUI(prepareCtx);

    auto *rootNode = root->getUINode();
    auto *topNode = top->getUINode();
    auto *spacerNode = spacer->getUINode();
    auto *bottomNode = bottom->getUINode();
    ASSERT_NE(rootNode, nullptr);
    ASSERT_NE(topNode, nullptr);
    ASSERT_NE(spacerNode, nullptr);
    ASSERT_NE(bottomNode, nullptr);

    rootNode->measure(*sceneState.getUINodeRegistry(), Bess::UUID::null);

    expectVec2(rootNode->getDrawSize(), 40.f, 100.f);
    expectVec2(topNode->getCachedPos(), 0.f, -45.f);
    expectVec2(spacerNode->getDrawSize(), 0.f, 80.f);
    expectVec2(bottomNode->getCachedPos(), 0.f, 45.f);
}

TEST_F(UiLayoutTests, SpacerCompDoesNotDraw) {
    Bess::Canvas::SceneState sceneState;
    const auto renderer = std::make_shared<LayoutTestRenderer2D>();
    auto prepareCtx = uiPrepareContext(sceneState, renderer);

    auto spacer = Bess::Canvas::UI::SpacerComp::create({
        .sceneState = &sceneState,
    });
    spacer->prepareUI(prepareCtx);

    auto *node = spacer->getUINode();
    ASSERT_NE(node, nullptr);
    node->measure(*sceneState.getUINodeRegistry(), Bess::UUID::null);

    auto drawCtx = uiDrawContext(sceneState, renderer);
    spacer->draw(drawCtx);

    EXPECT_TRUE(renderer->quads.empty());
    EXPECT_TRUE(renderer->fonts.empty());
}

TEST_F(UiLayoutTests, UIStyleShadowAppliesToBaseAndCustomFrameControls) {
    Bess::Canvas::SceneState sceneState;
    Bess::Canvas::UI::View ui{sceneState};
    const auto renderer = std::make_shared<LayoutTestRenderer2D>();
    const auto shadow = testShadow();

    auto button = ui.button("base",
                            nullptr,
                            Bess::Canvas::UI::UIElementStyle{
                                .shadow = shadow,
                            });
    auto progress =
        ui.progressBar("load",
                       0.5f,
                       0.f,
                       1.f,
                       Bess::Canvas::UI::UIElementStyle{
                           .shadow = shadow,
                       });
    progress->setShowLabel(false);
    progress->setShowValue(false);
    auto tree = ui.treeNode("tree",
                            Bess::Canvas::UI::UIElementStyle{
                                .shadow = shadow,
                            });

    auto prepareCtx = uiPrepareContext(sceneState, renderer);
    button->prepareUI(prepareCtx);
    progress->prepareUI(prepareCtx);
    tree->prepareUI(prepareCtx);
    button->getUINode()->measure(*sceneState.getUINodeRegistry(),
                                 Bess::UUID::null);
    progress->getUINode()->measure(*sceneState.getUINodeRegistry(),
                                   Bess::UUID::null);
    tree->getUINode()->measure(*sceneState.getUINodeRegistry(),
                               Bess::UUID::null);

    auto drawCtx = uiDrawContext(sceneState, renderer);
    button->draw(drawCtx);

    ASSERT_FALSE(renderer->quads.empty());
    expectShadow(renderer->quads.front().shadow, shadow);

    renderer->quads.clear();
    progress->draw(drawCtx);

    ASSERT_GE(renderer->quads.size(), 2u);
    expectShadow(renderer->quads[0].shadow, shadow);
    EXPECT_FALSE(renderer->quads[1].shadow.enabled);

    renderer->quads.clear();
    tree->draw(drawCtx);

    ASSERT_FALSE(renderer->quads.empty());
    EXPECT_FALSE(renderer->quads.front().shadow.enabled);

    tree->setDrawHeaderBackground(true);
    renderer->quads.clear();
    tree->draw(drawCtx);

    ASSERT_FALSE(renderer->quads.empty());
    expectShadow(renderer->quads.front().shadow, shadow);
}

TEST_F(UiLayoutTests, ToggleButtonRespectsConfiguredBorderAndShadowStyle) {
    Bess::Canvas::SceneState sceneState;
    Bess::Canvas::UI::View ui{sceneState};
    const auto renderer = std::make_shared<LayoutTestRenderer2D>();
    const auto shadow = testShadow();

    auto toggle = ui.toggleButton("toggle",
                                  Bess::Canvas::UI::UIElementStyle{
                                      .shadow = shadow,
                                      .borderSize =
                                          Bess::Core::Style::BorderSize(2.f),
                                  });
    toggle->setShowLabel(false);

    auto prepareCtx = uiPrepareContext(sceneState, renderer);
    toggle->prepareUI(prepareCtx);

    auto drawCtx = uiDrawContext(sceneState, renderer);
    toggle->draw(drawCtx);

    ASSERT_GE(renderer->quads.size(), 2u);
    const auto &track = renderer->quads[0];
    EXPECT_FLOAT_EQ(track.thickness.x, 2.f);
    EXPECT_FLOAT_EQ(track.thickness.y, 2.f);
    EXPECT_FLOAT_EQ(track.thickness.z, 2.f);
    EXPECT_FLOAT_EQ(track.thickness.w, 2.f);
    expectShadow(track.shadow, shadow);
    EXPECT_FALSE(renderer->quads[1].shadow.enabled);
}

TEST_F(UiLayoutTests, ToggleButtonTrackStaysAboveTransparentParentRow) {
    Bess::Canvas::SceneState sceneState;
    Bess::Canvas::UI::View ui{sceneState};
    const auto renderer = std::make_shared<LayoutTestRenderer2D>();

    auto toggle = ui.toggleButton("Schematic Mode",
                                  nullptr,
                                  false,
                                  Bess::Canvas::UI::UIElementStyle{
                                      .padding = 0.f,
                                      .margin = 0.f,
                                  });
    auto row = ui.row(Bess::Canvas::UI::CompConfig{
        .children =
            {
                toggle,
            },
        .style =
            Bess::Canvas::UI::UIElementStyle{
                .backgroundColor =
                    Bess::Core::Style::Color{0.2f, 0.3f, 0.4f, 0.5f},
                .padding = Bess::Core::Style::Padding(4.f),
                .zVal = 5000.f,
                .drawBg = true,
            },
    });

    auto prepareCtx = uiPrepareContext(sceneState, renderer);
    row->prepareUI(prepareCtx);
    row->getUINode()->measure(*sceneState.getUINodeRegistry(),
                              Bess::UUID::null);

    auto drawCtx = uiDrawContext(sceneState, renderer);
    row->draw(drawCtx);

    ASSERT_GE(renderer->quads.size(), 3u);
    const auto parentZ = renderer->quads.front().zIndex;

    size_t toggleQuadCount = 0;
    for (const auto &quad : renderer->quads) {
        if (quad.id.runtimeId != toggle->getRuntimeId() ||
            quad.id.info != 1u) {
            continue;
        }
        ++toggleQuadCount;
        EXPECT_EQ(quad.renderPass,
                  Bess::Core::Renderer::QuadRenderPass::Transparent);
        EXPECT_GT(quad.zIndex, parentZ);
    }

    EXPECT_EQ(toggleQuadCount, 2u);
}

TEST_F(UiLayoutTests, CompConfigMountsComponentTreeAndStyle) {
    Bess::Canvas::SceneState sceneState;

    const auto margin = Bess::Core::Style::Margin::onlyTop(3.f);
    auto nestedChild = Bess::Canvas::UI::ContainerComp::create({
        .sceneState = &sceneState,
        .children =
            {
                Bess::Canvas::UI::LabelComp::create("inline"),
            },
    });
    auto secondChild = Bess::Canvas::UI::LabelComp::create("second");

    auto root = Bess::Canvas::UI::ContainerComp::create({
        .sceneState = &sceneState,
        .children =
            {
                nestedChild,
                secondChild,
            },
        .style =
            Bess::Canvas::UI::UIElementStyle{
                .margin = margin,
            },
    });

    EXPECT_TRUE(sceneState.isComponentValid(root->getUuid()));
    EXPECT_TRUE(sceneState.isComponentValid(nestedChild->getUuid()));
    EXPECT_TRUE(sceneState.isComponentValid(secondChild->getUuid()));
    EXPECT_TRUE(sceneState.isRootComponent(root->getUuid()));
    EXPECT_FALSE(sceneState.isRootComponent(nestedChild->getUuid()));
    EXPECT_FALSE(sceneState.isRootComponent(secondChild->getUuid()));
    EXPECT_EQ(nestedChild->getParentComponent(), root->getUuid());
    EXPECT_EQ(secondChild->getParentComponent(), root->getUuid());
    EXPECT_TRUE(root->getChildComponents().contains(nestedChild->getUuid()));
    EXPECT_TRUE(root->getChildComponents().contains(secondChild->getUuid()));
    ASSERT_EQ(nestedChild->getChildComponents().size(), 1u);
    const auto inlineChildId = *nestedChild->getChildComponents().begin();
    EXPECT_TRUE(sceneState.isComponentValid(inlineChildId));
    EXPECT_FALSE(sceneState.isRootComponent(inlineChildId));
    EXPECT_EQ(
        sceneState.getComponentByUuid(inlineChildId)->getParentComponent(),
        nestedChild->getUuid());
    ASSERT_TRUE(root->getStyle().margin.has_value());
    EXPECT_EQ(root->getStyle().margin.value(), margin);
}

TEST_F(UiLayoutTests, ViewMountsComponentTreeAndStyles) {
    Bess::Canvas::SceneState sceneState;
    Bess::Canvas::UI::View ui{sceneState};

    const auto leafMargin = Bess::Core::Style::Margin::onlyBottom(2.f);
    const auto rootPadding = Bess::Core::Style::Padding(4.f);

    auto inlineLabel = ui.label("inline",
                                Bess::Canvas::UI::UIElementStyle{
                                    .fontSize = 9.f,
                                });
    auto action = ui.button("action",
                            Bess::Canvas::UI::UIElementStyle{
                                .margin = leafMargin,
                            });
    auto nested = ui.column({
        .children =
            {
                inlineLabel,
            },
    });
    auto root = ui.row({
        .children =
            {
                nested,
                action,
            },
        .style =
            Bess::Canvas::UI::UIElementStyle{
                .padding = rootPadding,
            },
    });

    EXPECT_TRUE(sceneState.isComponentValid(root->getUuid()));
    EXPECT_TRUE(sceneState.isComponentValid(nested->getUuid()));
    EXPECT_TRUE(sceneState.isComponentValid(inlineLabel->getUuid()));
    EXPECT_TRUE(sceneState.isComponentValid(action->getUuid()));
    EXPECT_TRUE(sceneState.isRootComponent(root->getUuid()));
    EXPECT_FALSE(sceneState.isRootComponent(nested->getUuid()));
    EXPECT_FALSE(sceneState.isRootComponent(inlineLabel->getUuid()));
    EXPECT_FALSE(sceneState.isRootComponent(action->getUuid()));

    EXPECT_EQ(root->getDirection(),
              Bess::Canvas::UI::LayoutDirection::horizontal);
    EXPECT_EQ(nested->getDirection(),
              Bess::Canvas::UI::LayoutDirection::vertical);
    EXPECT_EQ(nested->getParentComponent(), root->getUuid());
    EXPECT_EQ(action->getParentComponent(), root->getUuid());
    EXPECT_EQ(inlineLabel->getParentComponent(), nested->getUuid());
    EXPECT_TRUE(root->getChildComponents().contains(nested->getUuid()));
    EXPECT_TRUE(root->getChildComponents().contains(action->getUuid()));
    EXPECT_TRUE(nested->getChildComponents().contains(inlineLabel->getUuid()));

    ASSERT_TRUE(root->getStyle().padding.has_value());
    EXPECT_EQ(root->getStyle().padding.value(), rootPadding);
    ASSERT_TRUE(action->getStyle().margin.has_value());
    EXPECT_EQ(action->getStyle().margin.value(), leafMargin);
    ASSERT_TRUE(inlineLabel->getStyle().fontSize.has_value());
    EXPECT_FLOAT_EQ(inlineLabel->getStyle().fontSize.value(), 9.f);
}

TEST_F(UiLayoutTests, LabelDrawPickingIdUsesInvalidOrParentId) {
    Bess::Canvas::SceneState sceneState;
    Bess::Canvas::UI::View ui{sceneState};
    const auto renderer = std::make_shared<LayoutTestRenderer2D>();

    auto standaloneLabel = ui.label("standalone");
    auto childLabel = ui.label("child");
    auto parentRow = ui.row({
        .children =
            {
                childLabel,
            },
    });

    auto prepareCtx = uiPrepareContext(sceneState, renderer);
    standaloneLabel->prepareUI(prepareCtx);
    parentRow->prepareUI(prepareCtx);

    auto drawCtx = uiDrawContext(sceneState, renderer);
    standaloneLabel->draw(drawCtx);

    ASSERT_EQ(renderer->fonts.size(), 1u);
    EXPECT_EQ(renderer->fonts.front().id, Bess::PickingId::invalid());
    EXPECT_EQ(standaloneLabel->getCursor(),
              Bess::Core::Viewport::SceneCursor::normal);

    renderer->fonts.clear();
    parentRow->draw(drawCtx);

    ASSERT_EQ(renderer->fonts.size(), 1u);
    EXPECT_EQ(renderer->fonts.front().id.runtimeId, parentRow->getRuntimeId());
    EXPECT_EQ(renderer->fonts.front().id.info,
              Bess::PickingId::InfoFlags::passiveCursor);
    EXPECT_EQ(childLabel->getCursor(),
              Bess::Core::Viewport::SceneCursor::normal);
}

TEST_F(UiLayoutTests, PassiveLabelPickingHoversParentWithoutPointerCursor) {
    Bess::Canvas::SceneState sceneState;
    Bess::Canvas::UI::View ui{sceneState};
    const auto renderer = std::make_shared<LayoutTestRenderer2D>();

    const auto background = Bess::Core::Style::Color{0.1f, 0.2f, 0.3f, 0.4f};
    const auto hover = Bess::Core::Style::Color{0.3f, 0.4f, 0.5f, 0.4f};
    auto button = ui.button(Bess::Canvas::UI::CompConfig{
        .children =
            {
                ui.label("child"),
            },
        .style =
            Bess::Canvas::UI::UIElementStyle{
                .backgroundColor = background,
                .hoverColor = hover,
            },
    });

    auto prepareCtx = uiPrepareContext(sceneState, renderer);
    button->prepareUI(prepareCtx);

    auto viewportCtx =
        std::make_shared<Bess::Core::Viewport::ViewportContext>();
    Bess::Canvas::SceneEventContext eventCtx;
    eventCtx.sceneState = &sceneState;
    eventCtx.viewportCtx = viewportCtx;

    Bess::Canvas::UIComponentsLayer layer;
    auto hoverEvent = mouseMoveEvent(Bess::PickingId{
        .runtimeId = button->getRuntimeId(),
        .info = Bess::PickingId::InfoFlags::passiveCursor,
    });

    EXPECT_EQ(layer.handleEvent(hoverEvent, eventCtx),
              Bess::Canvas::EventResult::Consumed);
    EXPECT_EQ(viewportCtx->inputCtx.cursorRequest.cursor,
              Bess::Core::Viewport::SceneCursor::normal);

    auto drawCtx = uiDrawContext(sceneState, renderer);
    button->draw(drawCtx);

    ASSERT_FALSE(renderer->quads.empty());
    EXPECT_EQ(renderer->quads.front().color.toHex(), hover.toHex());
}

TEST_F(UiLayoutTests, ButtonWithChildrenDoesNotAddTextLabelNode) {
    Bess::Canvas::SceneState sceneState;
    Bess::Canvas::UI::View ui{sceneState};
    const auto renderer = std::make_shared<LayoutTestRenderer2D>();

    auto child = ui.label("child",
                          Bess::Canvas::UI::UIElementStyle{
                              .padding = Bess::Core::Style::Padding::zero(),
                              .margin = Bess::Core::Style::Margin::zero(),
                          });
    auto childRow = ui.row(Bess::Canvas::UI::CompConfig{
        .children =
            {
                child,
            },
        .style =
            Bess::Canvas::UI::UIElementStyle{
                .padding = Bess::Core::Style::Padding::zero(),
                .margin = Bess::Core::Style::Margin::zero(),
            },
    });

    auto button =
        ui.button("ignored label",
                  nullptr,
                  Bess::Canvas::UI::CompConfig{
                      .children =
                          {
                              childRow,
                          },
                      .style =
                          Bess::Canvas::UI::UIElementStyle{
                              .padding = Bess::Core::Style::Padding(3.f),
                          },
                  });

    auto prepareCtx = uiPrepareContext(sceneState, renderer);
    button->prepareUI(prepareCtx);

    ASSERT_NE(button->getUINode(), nullptr);
    ASSERT_NE(child->getUINode(), nullptr);

    const auto &nodeChildren = button->getUINode()->getChildren();
    ASSERT_EQ(nodeChildren.size(), 1u);
    EXPECT_TRUE(nodeChildren.contains(childRow->getUINode()->getId()));
    ASSERT_TRUE(childRow->getDrawRuntimeId().has_value());
    ASSERT_TRUE(child->getDrawRuntimeId().has_value());
    EXPECT_EQ(childRow->getDrawRuntimeId().value(), button->getRuntimeId());
    EXPECT_EQ(child->getDrawRuntimeId().value(), button->getRuntimeId());

    auto drawCtx = uiDrawContext(sceneState, renderer);
    button->draw(drawCtx);

    ASSERT_EQ(renderer->fonts.size(), 1u);
    EXPECT_EQ(renderer->fonts.front().id.runtimeId, button->getRuntimeId());
    EXPECT_EQ(renderer->fonts.front().id.info,
              Bess::PickingId::InfoFlags::passiveCursor);
}

TEST_F(UiLayoutTests, CustomBackgroundButtonHoverPreservesAlpha) {
    Bess::Canvas::SceneState sceneState;
    Bess::Canvas::UI::View ui{sceneState};
    const auto renderer = std::make_shared<LayoutTestRenderer2D>();

    const auto background = Bess::Core::Style::Color{0.2f, 0.3f, 0.4f, 0.5f};
    auto button = ui.button("hover",
                            nullptr,
                            Bess::Canvas::UI::UIElementStyle{
                                .backgroundColor = background,
                            });

    auto prepareCtx = uiPrepareContext(sceneState, renderer);
    button->prepareUI(prepareCtx);

    auto drawCtx = uiDrawContext(sceneState, renderer);
    button->onMouseEnter({});
    button->draw(drawCtx);

    ASSERT_FALSE(renderer->quads.empty());
    const auto hoverColor = renderer->quads.front().color;
    EXPECT_FLOAT_EQ(hoverColor.a, background.a);
    EXPECT_NE(hoverColor.toHex(), background.toHex());
}

TEST_F(UiLayoutTests, HoveringButtonDoesNotChangeSiblingRowBackgrounds) {
    Bess::Canvas::SceneState sceneState;
    Bess::Canvas::UI::View ui{sceneState};
    const auto renderer = std::make_shared<LayoutTestRenderer2D>();

    const auto background = Bess::Core::Style::Color{0.2f, 0.3f, 0.4f, 0.5f};
    const auto containerStyle = Bess::Canvas::UI::UIElementStyle{
        .backgroundColor = background,
        .padding = Bess::Core::Style::Padding::zero(),
        .margin = Bess::Core::Style::Margin::zero(),
        .drawBg = true,
    };
    const auto labelStyle = Bess::Canvas::UI::UIElementStyle{
        .padding = Bess::Core::Style::Padding::zero(),
        .margin = Bess::Core::Style::Margin::zero(),
    };

    auto camButton = ui.button(Bess::Canvas::UI::CompConfig{
        .children =
            {
                ui.label("cam", labelStyle),
            },
        .style = containerStyle,
    });
    auto zoomRow = ui.row(Bess::Canvas::UI::CompConfig{
        .children =
            {
                ui.label("zoom", labelStyle),
            },
        .style = containerStyle,
    });
    auto topRow = ui.row(Bess::Canvas::UI::CompConfig{
        .children =
            {
                ui.label("top", labelStyle),
            },
        .style = containerStyle,
    });

    auto bottomRoot = ui.row(Bess::Canvas::UI::CompConfig{
        .children =
            {
                camButton,
                zoomRow,
            },
        .style =
            Bess::Canvas::UI::UIElementStyle{
                .padding = Bess::Core::Style::Padding::zero(),
                .margin = Bess::Core::Style::Margin::zero(),
            },
    });
    auto topRoot = ui.row(Bess::Canvas::UI::CompConfig{
        .children =
            {
                topRow,
            },
        .style =
            Bess::Canvas::UI::UIElementStyle{
                .padding = Bess::Core::Style::Padding::zero(),
                .margin = Bess::Core::Style::Margin::zero(),
            },
    });

    auto prepareCtx = uiPrepareContext(sceneState, renderer);
    bottomRoot->prepareUI(prepareCtx);
    topRoot->prepareUI(prepareCtx);
    bottomRoot->getUINode()->measure(*sceneState.getUINodeRegistry(),
                                     Bess::UUID::null);
    topRoot->getUINode()->measure(*sceneState.getUINodeRegistry(),
                                  Bess::UUID::null);

    auto drawCtx = uiDrawContext(sceneState, renderer);
    camButton->onMouseEnter({});
    bottomRoot->draw(drawCtx);
    topRoot->draw(drawCtx);

    ASSERT_EQ(renderer->quads.size(), 3u);
    EXPECT_NE(renderer->quads[0].color.toHex(), background.toHex());
    EXPECT_FLOAT_EQ(renderer->quads[0].color.a, background.a);
    EXPECT_EQ(renderer->quads[1].color.toHex(), background.toHex());
    EXPECT_EQ(renderer->quads[2].color.toHex(), background.toHex());
}

TEST_F(UiLayoutTests, UIComponentsLayerResetsCursorAfterLeavingTextBox) {
    Bess::Canvas::SceneState sceneState;
    auto textBox = Bess::Canvas::UI::TextBoxComp::create();
    sceneState.addComponent(textBox, false, false);
    auto sceneComp = std::make_shared<Bess::Canvas::SceneComponent>();
    sceneState.addComponent(sceneComp, false, false);

    auto viewportCtx =
        std::make_shared<Bess::Core::Viewport::ViewportContext>();
    Bess::Canvas::SceneEventContext eventCtx;
    eventCtx.sceneState = &sceneState;
    eventCtx.viewportCtx = viewportCtx;

    Bess::Canvas::UIComponentsLayer layer;
    const auto textBoxPickingId = Bess::PickingId{
        .runtimeId = textBox->getRuntimeId(),
        .info = 0,
    };

    auto enterEvent = mouseMoveEvent(textBoxPickingId);
    EXPECT_EQ(layer.handleEvent(enterEvent, eventCtx),
              Bess::Canvas::EventResult::Consumed);
    EXPECT_EQ(viewportCtx->inputCtx.cursorRequest.cursor,
              Bess::Core::Viewport::SceneCursor::text);

    viewportCtx->inputCtx.resetCursorRequest();
    auto leaveEvent = mouseMoveEvent(Bess::PickingId::invalid());
    EXPECT_EQ(layer.handleEvent(leaveEvent, eventCtx),
              Bess::Canvas::EventResult::Ignored);
    EXPECT_EQ(viewportCtx->inputCtx.cursorRequest.cursor,
              Bess::Core::Viewport::SceneCursor::normal);

    auto reenterEvent = mouseMoveEvent(textBoxPickingId);
    EXPECT_EQ(layer.handleEvent(reenterEvent, eventCtx),
              Bess::Canvas::EventResult::Consumed);
    EXPECT_EQ(viewportCtx->inputCtx.cursorRequest.cursor,
              Bess::Core::Viewport::SceneCursor::text);

    viewportCtx->inputCtx.resetCursorRequest();
    const auto sceneCompPickingId = Bess::PickingId{
        .runtimeId = sceneComp->getRuntimeId(),
        .info = 0,
    };
    auto leaveToSceneCompEvent = mouseMoveEvent(sceneCompPickingId);
    EXPECT_EQ(layer.handleEvent(leaveToSceneCompEvent, eventCtx),
              Bess::Canvas::EventResult::Ignored);
    EXPECT_EQ(viewportCtx->inputCtx.cursorRequest.cursor,
              Bess::Core::Viewport::SceneCursor::normal);
}

TEST_F(UiLayoutTests, EditableLabelCommitsSubmittedEdit) {
    Bess::Canvas::SceneState sceneState;
    const auto renderer = std::make_shared<LayoutTestRenderer2D>();
    auto drawCtx = uiDrawContext(sceneState, renderer);

    Bess::Canvas::UI::EditableLabelComp label;
    label.setName("Alpha");
    label.setRuntimeId(0x501);

    int changedCount = 0;
    int submittedCount = 0;
    std::string changedValue;
    std::string submittedValue;
    label.setChangedCallback([&](const std::string &value) {
        ++changedCount;
        changedValue = value;
    });
    label.setSubmittedCallback([&](const std::string &value) {
        ++submittedCount;
        submittedValue = value;
    });

    prepareEditableLabel(label, sceneState, renderer);
    beginEditableLabelEdit(label, drawCtx, sceneState, renderer);

    auto textEvt = textInputEvent(U'X');
    EXPECT_TRUE(label.onKeyEvent(textEvt));
    label.draw(drawCtx);
    EXPECT_EQ(label.getName(), "Alpha");
    EXPECT_EQ(changedCount, 0);

    auto enterEvt = keyEvent(Bess::KeyCode::enter);
    EXPECT_TRUE(label.onKeyEvent(enterEvt));
    label.draw(drawCtx);

    EXPECT_FALSE(label.getEditing());
    EXPECT_EQ(label.getName(), "AlphaX");
    EXPECT_EQ(changedCount, 1);
    EXPECT_EQ(changedValue, "AlphaX");
    EXPECT_EQ(submittedCount, 1);
    EXPECT_EQ(submittedValue, "AlphaX");
}

TEST_F(UiLayoutTests, EditableLabelCanSelectTextWhenEditStarts) {
    Bess::Canvas::SceneState sceneState;
    const auto renderer = std::make_shared<LayoutTestRenderer2D>();
    auto drawCtx = uiDrawContext(sceneState, renderer);

    Bess::Canvas::UI::EditableLabelComp label;
    label.setName("Alpha");
    label.setRuntimeId(0x504);
    label.setSelectTextOnEdit(true);

    prepareEditableLabel(label, sceneState, renderer);
    beginEditableLabelEdit(label, drawCtx, sceneState, renderer);

    auto textEvt = textInputEvent(U'X');
    EXPECT_TRUE(label.onKeyEvent(textEvt));
    label.draw(drawCtx);

    auto enterEvt = keyEvent(Bess::KeyCode::enter);
    EXPECT_TRUE(label.onKeyEvent(enterEvt));
    label.draw(drawCtx);

    EXPECT_FALSE(label.getEditing());
    EXPECT_EQ(label.getName(), "X");
}

TEST_F(UiLayoutTests, EditableLabelCancelsEscapedEdit) {
    Bess::Canvas::SceneState sceneState;
    const auto renderer = std::make_shared<LayoutTestRenderer2D>();
    auto drawCtx = uiDrawContext(sceneState, renderer);

    Bess::Canvas::UI::EditableLabelComp label;
    label.setName("Alpha");
    label.setRuntimeId(0x502);

    int changedCount = 0;
    int canceledCount = 0;
    std::string canceledValue;
    label.setChangedCallback([&](const std::string &) { ++changedCount; });
    label.setCanceledCallback([&](const std::string &value) {
        ++canceledCount;
        canceledValue = value;
    });

    prepareEditableLabel(label, sceneState, renderer);
    beginEditableLabelEdit(label, drawCtx, sceneState, renderer);

    auto textEvt = textInputEvent(U'X');
    EXPECT_TRUE(label.onKeyEvent(textEvt));
    label.draw(drawCtx);

    auto escapeEvt = keyEvent(Bess::KeyCode::escape);
    EXPECT_TRUE(label.onKeyEvent(escapeEvt));
    label.draw(drawCtx);

    EXPECT_FALSE(label.getEditing());
    EXPECT_EQ(label.getName(), "Alpha");
    EXPECT_EQ(changedCount, 0);
    EXPECT_EQ(canceledCount, 1);
    EXPECT_EQ(canceledValue, "Alpha");
}

TEST_F(UiLayoutTests, EditableLabelCommitsEditOnBlur) {
    Bess::Canvas::SceneState sceneState;
    const auto renderer = std::make_shared<LayoutTestRenderer2D>();
    auto drawCtx = uiDrawContext(sceneState, renderer);

    Bess::Canvas::UI::EditableLabelComp label;
    label.setName("Alpha");
    label.setRuntimeId(0x503);

    int changedCount = 0;
    label.setChangedCallback([&](const std::string &) { ++changedCount; });

    prepareEditableLabel(label, sceneState, renderer);
    beginEditableLabelEdit(label, drawCtx, sceneState, renderer);

    auto textEvt = textInputEvent(U'Z');
    EXPECT_TRUE(label.onKeyEvent(textEvt));
    label.draw(drawCtx);

    label.onFocusLost({
        .entityUuid = label.getUuid(),
        .sceneState = &sceneState,
    });
    label.draw(drawCtx);

    EXPECT_FALSE(label.getEditing());
    EXPECT_EQ(label.getName(), "AlphaZ");
    EXPECT_EQ(changedCount, 1);
}

TEST_F(UiLayoutTests, SceneTextBoxSupportsCtrlCopyCutPasteAndSelectAll) {
    Bess::Canvas::SceneState sceneState;
    Bess::Canvas::SceneWidgets::SceneWidgetsState widgets;
    const auto renderer = std::make_shared<LayoutTestRenderer2D>();
    auto context = textBoxDrawContext(sceneState, renderer, widgets);

    const auto id = Bess::PickingId::forWidget(0x101);
    std::string value = "alpha beta";
    focusTextBox(context, widgets, id, value);

    auto evt = keyEvent(Bess::KeyCode::a, true);
    EXPECT_TRUE(Bess::Canvas::SceneWidgets::queueKey(&widgets, evt));

    evt = keyEvent(Bess::KeyCode::c, true);
    EXPECT_TRUE(Bess::Canvas::SceneWidgets::queueKey(&widgets, evt));

    evt = keyEvent(Bess::KeyCode::x, true);
    EXPECT_TRUE(Bess::Canvas::SceneWidgets::queueKey(&widgets, evt));
    auto result = drawTextBox(context, id, value);
    EXPECT_TRUE(result.changed);
    EXPECT_EQ(value, "");

    evt = keyEvent(Bess::KeyCode::v, true);
    EXPECT_TRUE(Bess::Canvas::SceneWidgets::queueKey(&widgets, evt));
    result = drawTextBox(context, id, value);
    EXPECT_TRUE(result.changed);
    EXPECT_EQ(value, "alpha beta");
}

TEST_F(UiLayoutTests, SceneTextBoxReplacesKeyboardSelection) {
    Bess::Canvas::SceneState sceneState;
    Bess::Canvas::SceneWidgets::SceneWidgetsState widgets;
    const auto renderer = std::make_shared<LayoutTestRenderer2D>();
    auto context = textBoxDrawContext(sceneState, renderer, widgets);

    const auto id = Bess::PickingId::forWidget(0x102);
    std::string value = "abcd";
    focusTextBox(context, widgets, id, value);

    auto evt = keyEvent(Bess::KeyCode::arrowLeft, false, true);
    EXPECT_TRUE(Bess::Canvas::SceneWidgets::queueKey(&widgets, evt));
    EXPECT_TRUE(Bess::Canvas::SceneWidgets::queueKey(&widgets, evt));

    auto textEvt = textInputEvent(U'X');
    EXPECT_TRUE(Bess::Canvas::SceneWidgets::queueKey(&widgets, textEvt));

    const auto result = drawTextBox(context, id, value);
    EXPECT_TRUE(result.changed);
    EXPECT_EQ(value, "abX");
}

TEST_F(UiLayoutTests, SceneTextBoxReplacesPointerSelection) {
    Bess::Canvas::SceneState sceneState;
    Bess::Canvas::SceneWidgets::SceneWidgetsState widgets;
    const auto renderer = std::make_shared<LayoutTestRenderer2D>();
    auto context = textBoxDrawContext(sceneState, renderer, widgets);

    const auto id = Bess::PickingId::forWidget(0x104);
    std::string value = "abcdef";
    focusTextBox(context, widgets, id, value);

    auto pointer = textBoxCursorPointer(renderer, value, 1);
    Bess::Canvas::SceneWidgets::queuePress(&widgets, id, pointer);
    drawTextBox(context, id, value);

    pointer = textBoxCursorPointer(renderer, value, 4);
    Bess::Canvas::SceneWidgets::queuePointerMove(&widgets, pointer);
    drawTextBox(context, id, value);

    Bess::Canvas::SceneWidgets::queueRelease(&widgets, id, pointer);
    drawTextBox(context, id, value);

    auto textEvt = textInputEvent(U'Z');
    EXPECT_TRUE(Bess::Canvas::SceneWidgets::queueKey(&widgets, textEvt));

    const auto result = drawTextBox(context, id, value);
    EXPECT_TRUE(result.changed);
    EXPECT_EQ(value, "aZef");
}

TEST_F(UiLayoutTests, SceneTextBoxSupportsCtrlWordDeletion) {
    Bess::Canvas::SceneState sceneState;
    Bess::Canvas::SceneWidgets::SceneWidgetsState widgets;
    const auto renderer = std::make_shared<LayoutTestRenderer2D>();
    auto context = textBoxDrawContext(sceneState, renderer, widgets);

    const auto id = Bess::PickingId::forWidget(0x103);
    std::string value = "alpha beta";
    focusTextBox(context, widgets, id, value);

    auto evt = keyEvent(Bess::KeyCode::backspace, true);
    EXPECT_TRUE(Bess::Canvas::SceneWidgets::queueKey(&widgets, evt));

    const auto result = drawTextBox(context, id, value);
    EXPECT_TRUE(result.changed);
    EXPECT_EQ(value, "alpha ");
}

TEST_F(UiLayoutTests, UINodeLayout) {
    Bess::Canvas::UI::UINodeRegistry registry;

    Bess::Canvas::UI::UINode parentNode;
    setPointSize(parentNode, glm::vec2(200, 100));
    parentNode.setPos(glm::vec2(0, 0));
    registry.addNode(parentNode);

    Bess::Canvas::UI::UINode *childNode1Ptr = nullptr;
    Bess::Canvas::UI::UINode *childNode2Ptr = nullptr;
    {
        Bess::Canvas::UI::UINode childNode1;
        setPointSize(childNode1, glm::vec2(50, 50));
        childNode1Ptr = registry.addNode(childNode1);
        parentNode.addChild(childNode1Ptr);
    }

    {
        Bess::Canvas::UI::UINode childNode2;
        setPointSize(childNode2, glm::vec2(30, 30));
        childNode2Ptr = registry.addNode(childNode2);
        parentNode.addChild(childNode2Ptr);
    }

    EXPECT_EQ(parentNode.getCrossAxisAlignment(),
              Bess::Canvas::UI::LayoutAlignment::start);
    EXPECT_EQ(parentNode.getMainAxisAlignment(),
              Bess::Canvas::UI::LayoutAlignment::start);
    EXPECT_EQ(parentNode.getDirection(),
              Bess::Canvas::UI::LayoutDirection::horizontal);
    EXPECT_EQ(parentNode.getChildren().size(), 2);

    parentNode.measure(registry, Bess::UUID::null);

    glm::vec2 childSize = childNode1Ptr->getCachedSize();
    expectVec2(childSize, 50, 50);

    childSize = childNode2Ptr->getCachedSize();
    expectVec2(childSize, 30, 30);

    parentNode.measure(registry, Bess::UUID::null);

    auto parentPos = parentNode.getCachedPos();

    auto child1Pos = childNode1Ptr->getCachedPos();
    auto child2Pos = childNode2Ptr->getCachedPos();

    expectVec2(child1Pos, -75, -25);

    expectVec2(child2Pos, -35, -35);

    BESS_INFO("Checked measure positions of child nodes");
}

TEST_F(UiLayoutTests, UINodeLayoutHonorsMarginAndCenterAlignment) {
    Bess::Canvas::UI::UINodeRegistry registry;

    Bess::Canvas::UI::UINode parentNode;
    setPointSize(parentNode, glm::vec2(200, 100));
    parentNode.setCrossAxisAlignment(Bess::Canvas::UI::LayoutAlignment::center);

    Bess::Canvas::UI::UINode childNode1;
    setPointSize(childNode1, glm::vec2(50, 50));
    childNode1.setMargin(Bess::Core::Style::Margin(5, 50, 5, 5));
    auto childNode1Ptr = registry.addNode(childNode1);
    parentNode.addChild(childNode1Ptr);

    Bess::Canvas::UI::UINode childNode2;
    setPointSize(childNode2, glm::vec2(30, 30));
    auto childNode2Ptr = registry.addNode(childNode2);
    parentNode.addChild(childNode2Ptr);

    parentNode.measure(registry, Bess::UUID::null);

    ASSERT_NE(childNode1Ptr, nullptr);
    ASSERT_NE(childNode2Ptr, nullptr);

    expectVec2(parentNode.getDrawSize(), 200, 100);
    expectVec2(childNode1Ptr->getCachedSize(), 105, 60);
    expectVec2(childNode1Ptr->getDrawSize(), 50, 50);
    expectVec2(childNode1Ptr->getCachedPos(), -70, 0);
    expectVec2(childNode2Ptr->getCachedPos(), 20, 0);
}

TEST_F(UiLayoutTests, UINodeLayoutHonorsMainAndCrossAxisAlignment) {
    Bess::Canvas::UI::UINodeRegistry registry;

    Bess::Canvas::UI::UINode parentNode;
    setPointSize(parentNode, glm::vec2(200, 100));
    parentNode.setMainAxisAlignment(Bess::Canvas::UI::LayoutAlignment::end);
    parentNode.setCrossAxisAlignment(Bess::Canvas::UI::LayoutAlignment::center);

    Bess::Canvas::UI::UINode childNode1;
    setPointSize(childNode1, glm::vec2(50, 50));
    auto childNode1Ptr = registry.addNode(childNode1);
    ASSERT_NE(childNode1Ptr, nullptr);
    parentNode.addChild(childNode1Ptr);

    Bess::Canvas::UI::UINode childNode2;
    setPointSize(childNode2, glm::vec2(30, 30));
    auto childNode2Ptr = registry.addNode(childNode2);
    ASSERT_NE(childNode2Ptr, nullptr);
    parentNode.addChild(childNode2Ptr);

    parentNode.measure(registry, Bess::UUID::null);

    expectVec2(childNode1Ptr->getCachedPos(), 45, 0);
    expectVec2(childNode2Ptr->getCachedPos(), 85, 0);
}

TEST_F(UiLayoutTests, UINodeLayoutHonorsDistributedMainAxisAlignment) {
    Bess::Canvas::UI::UINodeRegistry registry;

    Bess::Canvas::UI::UINode parentNode;
    setPointSize(parentNode, glm::vec2(200, 100));

    Bess::Canvas::UI::UINode childNode1;
    setPointSize(childNode1, glm::vec2(50, 50));
    auto childNode1Ptr = registry.addNode(childNode1);
    ASSERT_NE(childNode1Ptr, nullptr);
    parentNode.addChild(childNode1Ptr);

    Bess::Canvas::UI::UINode childNode2;
    setPointSize(childNode2, glm::vec2(30, 30));
    auto childNode2Ptr = registry.addNode(childNode2);
    ASSERT_NE(childNode2Ptr, nullptr);
    parentNode.addChild(childNode2Ptr);

    parentNode.setMainAxisAlignment(
        Bess::Canvas::UI::LayoutAlignment::spaceBetween);
    parentNode.measure(registry, Bess::UUID::null);
    expectVec2(childNode1Ptr->getCachedPos(), -75, -25);
    expectVec2(childNode2Ptr->getCachedPos(), 85, -35);

    parentNode.setMainAxisAlignment(
        Bess::Canvas::UI::LayoutAlignment::spaceAround);
    parentNode.measure(registry, Bess::UUID::null);
    expectVec2(childNode1Ptr->getCachedPos(), -45, -25);
    expectVec2(childNode2Ptr->getCachedPos(), 55, -35);

    parentNode.setMainAxisAlignment(
        Bess::Canvas::UI::LayoutAlignment::spaceEvenly);
    parentNode.measure(registry, Bess::UUID::null);
    expectVec2(childNode1Ptr->getCachedPos(), -35, -25);
    expectVec2(childNode2Ptr->getCachedPos(), 45, -35);
}

TEST_F(UiLayoutTests, UINodeLayoutHonorsDistributedVerticalMainAxisAlignment) {
    Bess::Canvas::UI::UINodeRegistry registry;

    Bess::Canvas::UI::UINode parentNode;
    setPointSize(parentNode, glm::vec2(100, 200));
    parentNode.setDirection(Bess::Canvas::UI::LayoutDirection::vertical);
    parentNode.setMainAxisAlignment(
        Bess::Canvas::UI::LayoutAlignment::spaceEvenly);

    Bess::Canvas::UI::UINode childNode1;
    setPointSize(childNode1, glm::vec2(50, 50));
    auto childNode1Ptr = registry.addNode(childNode1);
    ASSERT_NE(childNode1Ptr, nullptr);
    parentNode.addChild(childNode1Ptr);

    Bess::Canvas::UI::UINode childNode2;
    setPointSize(childNode2, glm::vec2(30, 30));
    auto childNode2Ptr = registry.addNode(childNode2);
    ASSERT_NE(childNode2Ptr, nullptr);
    parentNode.addChild(childNode2Ptr);

    parentNode.measure(registry, Bess::UUID::null);

    expectVec2(childNode1Ptr->getCachedPos(), -25, -35);
    expectVec2(childNode2Ptr->getCachedPos(), -35, 45);
}

TEST_F(UiLayoutTests, EqualFlexColumnsRespectSharedMinimumWidth) {
    Bess::Canvas::UI::UINodeRegistry registry;

    Bess::Canvas::UI::UINode parentNode;
    setPointSize(parentNode, glm::vec2(216, 100));

    Bess::Canvas::UI::UINode childNode1;
    childNode1.setFlex(1.f, 0.f, 0.f);
    childNode1.setMinSize(glm::vec2(100.f, -1.f));
    childNode1.setHeight(20.f);
    childNode1.setMargin(Bess::Core::Style::Margin(0.f, 16.f, 0.f, 0.f));
    auto childNode1Ptr = registry.addNode(childNode1);
    ASSERT_NE(childNode1Ptr, nullptr);
    parentNode.addChild(childNode1Ptr);

    Bess::Canvas::UI::UINode childNode2;
    childNode2.setFlex(1.f, 0.f, 0.f);
    childNode2.setMinSize(glm::vec2(100.f, -1.f));
    childNode2.setHeight(20.f);
    auto childNode2Ptr = registry.addNode(childNode2);
    ASSERT_NE(childNode2Ptr, nullptr);
    parentNode.addChild(childNode2Ptr);

    parentNode.measure(registry, Bess::UUID::null);

    expectVec2(parentNode.getDrawSize(), 216, 100);
    expectVec2(childNode1Ptr->getDrawSize(), 100, 20);
    expectVec2(childNode1Ptr->getCachedSize(), 116, 20);
    expectVec2(childNode2Ptr->getDrawSize(), 100, 20);
    expectVec2(childNode1Ptr->getCachedPos(), -58, -40);
    expectVec2(childNode2Ptr->getCachedPos(), 58, -40);
}

TEST_F(UiLayoutTests, WrapContainerDoesNotGrowFromStaleRelativeSizes) {
    Bess::Canvas::UI::UINodeRegistry registry;

    Bess::Canvas::UI::UINode rootNode;
    rootNode.setDirection(Bess::Canvas::UI::LayoutDirection::vertical);
    setFitContent(rootNode);

    Bess::Canvas::UI::UINode slotsBoxNode;
    slotsBoxNode.setDirection(Bess::Canvas::UI::LayoutDirection::horizontal);
    slotsBoxNode.setAlignSelf(Bess::Canvas::UI::LayoutSelfAlignment::stretch);
    slotsBoxNode.setWidthAuto();
    slotsBoxNode.setHeightFitContent();
    auto slotsBoxNodePtr = registry.addNode(slotsBoxNode);
    ASSERT_NE(slotsBoxNodePtr, nullptr);
    rootNode.addChild(slotsBoxNodePtr);

    Bess::Canvas::UI::UINode inputBoxNode;
    inputBoxNode.setDirection(Bess::Canvas::UI::LayoutDirection::vertical);
    inputBoxNode.setFlex(1.f, 0.f, 0.f);
    inputBoxNode.setMinSize(glm::vec2(100.f, -1.f));
    inputBoxNode.setMargin(Bess::Core::Style::Margin(0.f, 16.f, 0.f, 0.f));
    auto inputBoxNodePtr = registry.addNode(inputBoxNode);
    ASSERT_NE(inputBoxNodePtr, nullptr);
    slotsBoxNodePtr->addChild(inputBoxNodePtr);

    Bess::Canvas::UI::UINode outputBoxNode;
    outputBoxNode.setDirection(Bess::Canvas::UI::LayoutDirection::vertical);
    outputBoxNode.setFlex(1.f, 0.f, 0.f);
    outputBoxNode.setMinSize(glm::vec2(100.f, -1.f));
    auto outputBoxNodePtr = registry.addNode(outputBoxNode);
    ASSERT_NE(outputBoxNodePtr, nullptr);
    slotsBoxNodePtr->addChild(outputBoxNodePtr);

    Bess::Canvas::UI::UINode inputRowNode;
    setPointSize(inputRowNode, glm::vec2(100.f, 20.f));
    auto inputRowNodePtr = registry.addNode(inputRowNode);
    ASSERT_NE(inputRowNodePtr, nullptr);
    inputBoxNodePtr->addChild(inputRowNodePtr);

    Bess::Canvas::UI::UINode outputRowNode;
    setPointSize(outputRowNode, glm::vec2(100.f, 20.f));
    auto outputRowNodePtr = registry.addNode(outputRowNode);
    ASSERT_NE(outputRowNodePtr, nullptr);
    outputBoxNodePtr->addChild(outputRowNodePtr);

    rootNode.measure(registry, Bess::UUID::null);
    expectVec2(rootNode.getDrawSize(), 216.f, 20.f);
    expectVec2(slotsBoxNodePtr->getDrawSize(), 216.f, 20.f);
    expectVec2(inputBoxNodePtr->getCachedSize(), 116.f, 20.f);
    expectVec2(outputBoxNodePtr->getCachedSize(), 100.f, 20.f);

    rootNode.measure(registry, Bess::UUID::null);
    expectVec2(rootNode.getDrawSize(), 216.f, 20.f);
    expectVec2(slotsBoxNodePtr->getDrawSize(), 216.f, 20.f);
    expectVec2(inputBoxNodePtr->getCachedSize(), 116.f, 20.f);
    expectVec2(outputBoxNodePtr->getCachedSize(), 100.f, 20.f);
}

TEST_F(UiLayoutTests, OutputColumnCrossAxisEndAlignsRowsToRightEdge) {
    Bess::Canvas::UI::UINodeRegistry registry;

    Bess::Canvas::UI::UINode rootNode;
    rootNode.setDirection(Bess::Canvas::UI::LayoutDirection::vertical);
    setFitContent(rootNode);

    Bess::Canvas::UI::UINode headerNode;
    setPointSize(headerNode, glm::vec2(200.f, 20.f));
    auto headerNodePtr = registry.addNode(headerNode);
    ASSERT_NE(headerNodePtr, nullptr);
    rootNode.addChild(headerNodePtr);

    Bess::Canvas::UI::UINode slotsBoxNode;
    slotsBoxNode.setDirection(Bess::Canvas::UI::LayoutDirection::horizontal);
    slotsBoxNode.setAlignSelf(Bess::Canvas::UI::LayoutSelfAlignment::stretch);
    slotsBoxNode.setWidthAuto();
    slotsBoxNode.setHeightFitContent();
    auto slotsBoxNodePtr = registry.addNode(slotsBoxNode);
    ASSERT_NE(slotsBoxNodePtr, nullptr);
    rootNode.addChild(slotsBoxNodePtr);

    Bess::Canvas::UI::UINode inputBoxNode;
    inputBoxNode.setDirection(Bess::Canvas::UI::LayoutDirection::vertical);
    inputBoxNode.setFlex(1.f, 0.f, 0.f);
    inputBoxNode.setMinSize(glm::vec2(100.f, -1.f));
    inputBoxNode.setMargin(Bess::Core::Style::Margin(0.f, 16.f, 0.f, 0.f));
    auto inputBoxNodePtr = registry.addNode(inputBoxNode);
    ASSERT_NE(inputBoxNodePtr, nullptr);
    slotsBoxNodePtr->addChild(inputBoxNodePtr);

    Bess::Canvas::UI::UINode outputBoxNode;
    outputBoxNode.setDirection(Bess::Canvas::UI::LayoutDirection::vertical);
    outputBoxNode.setCrossAxisAlignment(Bess::Canvas::UI::LayoutAlignment::end);
    outputBoxNode.setFlex(1.f, 0.f, 0.f);
    outputBoxNode.setMinSize(glm::vec2(100.f, -1.f));
    auto outputBoxNodePtr = registry.addNode(outputBoxNode);
    ASSERT_NE(outputBoxNodePtr, nullptr);
    slotsBoxNodePtr->addChild(outputBoxNodePtr);

    Bess::Canvas::UI::UINode inputRowNode;
    setPointSize(inputRowNode, glm::vec2(100.f, 20.f));
    auto inputRowNodePtr = registry.addNode(inputRowNode);
    ASSERT_NE(inputRowNodePtr, nullptr);
    inputBoxNodePtr->addChild(inputRowNodePtr);

    Bess::Canvas::UI::UINode outputRowNode;
    setPointSize(outputRowNode, glm::vec2(30.f, 20.f));
    auto outputRowNodePtr = registry.addNode(outputRowNode);
    ASSERT_NE(outputRowNodePtr, nullptr);
    outputBoxNodePtr->addChild(outputRowNodePtr);

    rootNode.measure(registry, Bess::UUID::null);

    expectVec2(rootNode.getDrawSize(), 216.f, 40.f);
    expectVec2(slotsBoxNodePtr->getDrawSize(), 216.f, 20.f);
    expectVec2(outputBoxNodePtr->getDrawSize(), 100.f, 20.f);
    expectVec2(inputRowNodePtr->getCachedPos(), -58.f, 10.f);
    expectVec2(outputRowNodePtr->getCachedPos(), 93.f, 10.f);
}

TEST_F(UiLayoutTests, UINodeLayoutHonorsEndAlignmentInVerticalFlow) {
    Bess::Canvas::UI::UINodeRegistry registry;

    Bess::Canvas::UI::UINode parentNode;
    setPointSize(parentNode, glm::vec2(100, 100));
    parentNode.setDirection(Bess::Canvas::UI::LayoutDirection::vertical);
    parentNode.setCrossAxisAlignment(Bess::Canvas::UI::LayoutAlignment::end);

    Bess::Canvas::UI::UINode childNode;
    setPointSize(childNode, glm::vec2(20, 30));
    auto childNodePtr = registry.addNode(childNode);
    ASSERT_NE(childNodePtr, nullptr);

    parentNode.addChild(childNodePtr);

    parentNode.measure(registry, Bess::UUID::null);

    expectVec2(childNodePtr->getCachedPos(), 40, -35);
}

TEST_F(UiLayoutTests, UINodeRelativeSizeUsesParentContentBox) {
    Bess::Canvas::UI::UINodeRegistry registry;

    Bess::Canvas::UI::UINode parentNode;
    setPointSize(parentNode, glm::vec2(200, 100));
    parentNode.setPadding(Bess::Core::Style::Margin(10, 10, 10, 10));

    Bess::Canvas::UI::UINode childNode;
    childNode.setWidthPercent(50.f);
    childNode.setHeightPercent(25.f);
    auto childNodePtr = registry.addNode(childNode);
    ASSERT_NE(childNodePtr, nullptr);
    parentNode.addChild(childNodePtr);

    parentNode.measure(registry, Bess::UUID::null);

    expectVec2(childNodePtr->getDrawSize(), 90, 20);
    expectVec2(childNodePtr->getCachedPos(), -45, -30);
}

TEST_F(UiLayoutTests, UINodeLayoutRefreshesWhenChildSizeChanges) {
    Bess::Canvas::UI::UINodeRegistry registry;

    auto parentNode = registry.addNode(Bess::UUID());
    ASSERT_NE(parentNode, nullptr);
    setPointSize(*parentNode, glm::vec2(200, 100));

    Bess::Canvas::UI::UINode childNode1;
    setPointSize(childNode1, glm::vec2(50, 50));
    auto childNode1Ptr = registry.addNode(childNode1);
    ASSERT_NE(childNode1Ptr, nullptr);
    parentNode->addChild(childNode1Ptr);

    Bess::Canvas::UI::UINode childNode2;
    setPointSize(childNode2, glm::vec2(30, 30));
    auto childNode2Ptr = registry.addNode(childNode2);
    ASSERT_NE(childNode2Ptr, nullptr);
    parentNode->addChild(childNode2Ptr);

    parentNode->measure(registry, Bess::UUID::null);

    expectVec2(childNode2Ptr->getCachedPos(), -35, -35);

    setPointSize(*childNode1Ptr, glm::vec2(100, 50));
    EXPECT_TRUE(childNode1Ptr->getSizeDirty());
    EXPECT_TRUE(parentNode->getSizeDirty());
    EXPECT_FALSE(childNode2Ptr->getSizeDirty());

    parentNode->measure(registry, Bess::UUID::null);

    expectVec2(childNode1Ptr->getCachedPos(), -50, -25);
    expectVec2(childNode2Ptr->getCachedPos(), 15, -35);
    EXPECT_FALSE(parentNode->getSizeDirty());
    EXPECT_FALSE(parentNode->getPosDirty());
    EXPECT_FALSE(childNode1Ptr->getSizeDirty());
    EXPECT_FALSE(childNode1Ptr->getPosDirty());
    EXPECT_FALSE(childNode2Ptr->getSizeDirty());
    EXPECT_FALSE(childNode2Ptr->getPosDirty());
}

TEST_F(UiLayoutTests, UINodeDirtySizePropagatesThroughAncestors) {
    Bess::Canvas::UI::UINodeRegistry registry;

    auto rootNode = registry.addNode(Bess::UUID());
    auto rowNode = registry.addNode(Bess::UUID());
    auto childNode1 = registry.addNode(Bess::UUID());
    auto childNode2 = registry.addNode(Bess::UUID());
    ASSERT_NE(rootNode, nullptr);
    ASSERT_NE(rowNode, nullptr);
    ASSERT_NE(childNode1, nullptr);
    ASSERT_NE(childNode2, nullptr);

    setPointSize(*rootNode, glm::vec2(200, 100));
    rootNode->addChild(rowNode);

    setFitContent(*rowNode);
    rowNode->addChild(childNode1);
    rowNode->addChild(childNode2);

    setPointSize(*childNode1, glm::vec2(50, 20));
    setPointSize(*childNode2, glm::vec2(30, 20));

    rootNode->measure(registry, Bess::UUID::null);
    const auto child2CachedSize = childNode2->getCachedSize();
    const auto child2CachedPos = childNode2->getCachedPos();

    EXPECT_FALSE(rootNode->getSizeDirty());
    EXPECT_FALSE(rowNode->getSizeDirty());
    EXPECT_FALSE(childNode1->getSizeDirty());
    EXPECT_FALSE(childNode2->getSizeDirty());

    setPointSize(*childNode1, glm::vec2(80, 20));

    EXPECT_TRUE(childNode1->getSizeDirty());
    EXPECT_TRUE(rowNode->getSizeDirty());
    EXPECT_TRUE(rootNode->getSizeDirty());
    EXPECT_FALSE(childNode2->getSizeDirty());

    rootNode->measure(registry, Bess::UUID::null);

    expectVec2(rowNode->getDrawSize(), 110, 20);
    expectVec2(childNode1->getDrawSize(), 80, 20);
    expectVec2(child2CachedSize, 30, 20);
    expectVec2(
        childNode2->getCachedSize(), child2CachedSize.x, child2CachedSize.y);
    EXPECT_NE(childNode2->getCachedPos().x, child2CachedPos.x);

    EXPECT_FALSE(rootNode->getSizeDirty());
    EXPECT_FALSE(rootNode->getPosDirty());
    EXPECT_FALSE(rowNode->getSizeDirty());
    EXPECT_FALSE(rowNode->getPosDirty());
    EXPECT_FALSE(childNode1->getSizeDirty());
    EXPECT_FALSE(childNode1->getPosDirty());
    EXPECT_FALSE(childNode2->getSizeDirty());
    EXPECT_FALSE(childNode2->getPosDirty());
}

TEST_F(UiLayoutTests, UINodeDirtyPositionPropagatesWithoutSizeInvalidation) {
    Bess::Canvas::UI::UINodeRegistry registry;

    auto rootNode = registry.addNode(Bess::UUID());
    auto rowNode = registry.addNode(Bess::UUID());
    auto childNode = registry.addNode(Bess::UUID());
    ASSERT_NE(rootNode, nullptr);
    ASSERT_NE(rowNode, nullptr);
    ASSERT_NE(childNode, nullptr);

    setPointSize(*rootNode, glm::vec2(200, 100));
    rootNode->addChild(rowNode);

    setFitContent(*rowNode);
    rowNode->addChild(childNode);

    setPointSize(*childNode, glm::vec2(50, 20));

    rootNode->measure(registry, Bess::UUID::null);
    const auto previousCachedSize = rootNode->getCachedSize();

    childNode->setPos(glm::vec2(4, 0));

    EXPECT_TRUE(childNode->getPosDirty());
    EXPECT_TRUE(rowNode->getPosDirty());
    EXPECT_TRUE(rootNode->getPosDirty());
    EXPECT_FALSE(childNode->getSizeDirty());
    EXPECT_FALSE(rowNode->getSizeDirty());
    EXPECT_FALSE(rootNode->getSizeDirty());

    rootNode->measure(registry, Bess::UUID::null);

    expectVec2(
        rootNode->getCachedSize(), previousCachedSize.x, previousCachedSize.y);
    expectVec2(childNode->getCachedPos(), -71, -40);
    EXPECT_FALSE(rootNode->getPosDirty());
    EXPECT_FALSE(rowNode->getPosDirty());
    EXPECT_FALSE(childNode->getPosDirty());
}

TEST_F(UiLayoutTests, UINodeSameValueSettersDoNotInvalidateCleanTree) {
    Bess::Canvas::UI::UINodeRegistry registry;

    auto rootNode = registry.addNode(Bess::UUID());
    ASSERT_NE(rootNode, nullptr);

    setPointSize(*rootNode, glm::vec2(100, 60));
    rootNode->setPos(glm::vec2(2, 4));
    rootNode->setPadding(Bess::Core::Style::Padding(1, 2, 3, 4));
    rootNode->setMargin(Bess::Core::Style::Margin(5, 6, 7, 8));
    rootNode->measure(registry, Bess::UUID::null);

    ASSERT_FALSE(rootNode->getSizeDirty());
    ASSERT_FALSE(rootNode->getPosDirty());

    setPointSize(*rootNode, glm::vec2(100, 60));
    rootNode->setPos(glm::vec2(2, 4));
    rootNode->setPadding(Bess::Core::Style::Padding(1, 2, 3, 4));
    rootNode->setMargin(Bess::Core::Style::Margin(5, 6, 7, 8));
    rootNode->setDirection(Bess::Canvas::UI::LayoutDirection::horizontal);
    rootNode->setMainAxisAlignment(Bess::Canvas::UI::LayoutAlignment::start);
    rootNode->setCrossAxisAlignment(Bess::Canvas::UI::LayoutAlignment::start);
    rootNode->setZVal(0.f);

    EXPECT_FALSE(rootNode->getSizeDirty());
    EXPECT_FALSE(rootNode->getPosDirty());
}
