#include "bess_core/scene/scene_ui/layout.h"
#include "bess_core/style/bess_theme.h"
#include "common/bess_uuid.h"
#include "common/logger.h"
#include <gtest/gtest.h>

namespace {
    void expectVec2(const glm::vec2 &actual, float expectedX, float expectedY) {
        EXPECT_FLOAT_EQ(actual.x, expectedX);
        EXPECT_FLOAT_EQ(actual.y, expectedY);
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
} // namespace

class UiLayoutTests : public testing::Test {};

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
