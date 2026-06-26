#include "bess_core/scene/scene_ui/layout.h"
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

TEST_F(UiLayoutTests, UINodeMeasure) {
    Bess::Canvas::UI::UINodeRegistry registry;

    constexpr glm::vec2 size(100, 50);
    constexpr glm::vec4 padding(10, 5, 9, 6);
    constexpr glm::vec4 margin(2, 3, 4, 5);
    const glm::vec2 paddingSize = boxEdges(padding);
    const glm::vec2 marginSize = boxEdges(margin);

    Bess::Canvas::UI::UINode node;
    node.setSizeConstraint(Bess::Canvas::UI::SizeContraint::fixed);
    node.setSize(size);
    EXPECT_EQ(node.getSizeDirty(), true);
    EXPECT_EQ(node.getSize(), size);

    node.setPadding(padding);
    EXPECT_EQ(node.getSizeDirty(), true);
    EXPECT_EQ(node.getPadding(), padding);

    node.setMargin(margin);
    EXPECT_EQ(node.getSizeDirty(), true);
    EXPECT_EQ(node.getMargin(), margin);

    auto measuredSize = node.measure(registry, Bess::UUID::null);
    auto worldSize = node.getCachedSize();

    expectVec2(measuredSize, worldSize.x, worldSize.y);

    // Fixed size describes the drawn box. Margin contributes to layout size.
    expectVec2(node.getDrawSize(), size.x, size.y);
    expectVec2(worldSize, size.x + marginSize.x, size.y + marginSize.y);

    BESS_INFO("Measured with fixed constraint");

    node.setSizeConstraint(Bess::Canvas::UI::SizeContraint::wrap_content);
    measuredSize = node.measure(registry, Bess::UUID::null);
    worldSize = node.getCachedSize();

    // Wrap content uses padding as the drawn box when there are no children.
    expectVec2(measuredSize, worldSize.x, worldSize.y);

    expectVec2(node.getDrawSize(), paddingSize.x, paddingSize.y);
    expectVec2(
        worldSize, paddingSize.x + marginSize.x, paddingSize.y + marginSize.y);

    BESS_INFO("Measured with wrap_content constraint");

    Bess::Canvas::UI::UINode childNode;
    childNode.setSizeConstraint(Bess::Canvas::UI::SizeContraint::fixed);
    childNode.setSize(glm::vec2(20, 10));
    auto childNodePtr = registry.addNode(childNode);
    node.addChild(childNodePtr);

    measuredSize = node.measure(registry, Bess::UUID::null);
    worldSize = node.getCachedSize();

    // Child layout footprints are placed inside the padded content box.
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

    // checking for SizeContraint::wrap_content size with child and min size
    expectVec2(measuredSize, worldSize.x, worldSize.y);

    expectVec2(node.getDrawSize(), 150, 100);
    expectVec2(worldSize, 150 + marginSize.x, 100 + marginSize.y);

    BESS_INFO("Measured with wrap_content constraint, child node and min size");

    node.setMaxSize(glm::vec2(120, 80));
    measuredSize = node.measure(registry, Bess::UUID::null);
    worldSize = node.getCachedSize();

    // checking for SizeContraint::wrap_content size with child, min size and
    // max size
    expectVec2(measuredSize, worldSize.x, worldSize.y);

    // Minimum width overrides maximum width
    expectVec2(node.getDrawSize(), 150, 100);
    expectVec2(worldSize, 150 + marginSize.x, 100 + marginSize.y);

    node.setMinSize(glm::vec2(50, 40));
    node.setSize(glm::vec2(200, 150));

    node.setSizeConstraint(Bess::Canvas::UI::SizeContraint::fixed);
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
    parentNode.setSizeConstraint(Bess::Canvas::UI::SizeContraint::fixed);
    parentNode.setSize(glm::vec2(200, 100));
    parentNode.setPos(glm::vec2(0, 0));
    registry.addNode(parentNode);

    Bess::Canvas::UI::UINode *childNode1Ptr = nullptr;
    Bess::Canvas::UI::UINode *childNode2Ptr = nullptr;
    {
        Bess::Canvas::UI::UINode childNode1;
        childNode1.setSizeConstraint(Bess::Canvas::UI::SizeContraint::fixed);
        childNode1.setSize(glm::vec2(50, 50));
        childNode1Ptr = registry.addNode(childNode1);
        parentNode.addChild(childNode1Ptr);
    }

    {
        Bess::Canvas::UI::UINode childNode2;
        childNode2.setSizeConstraint(Bess::Canvas::UI::SizeContraint::fixed);
        childNode2.setSize(glm::vec2(30, 30));
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

    parentNode.layout(registry, Bess::UUID::null);

    auto parentPos = parentNode.getCachedPos();

    auto child1Pos = childNode1Ptr->getCachedPos();
    auto child2Pos = childNode2Ptr->getCachedPos();

    expectVec2(child1Pos, -75, -25);

    expectVec2(child2Pos, -35, -35);

    BESS_INFO("Checked layout positions of child nodes");
}

TEST_F(UiLayoutTests, UINodeLayoutHonorsMarginAndCenterAlignment) {
    Bess::Canvas::UI::UINodeRegistry registry;

    Bess::Canvas::UI::UINode parentNode;
    parentNode.setSizeConstraint(Bess::Canvas::UI::SizeContraint::fixed);
    parentNode.setSize(glm::vec2(200, 100));
    parentNode.setCrossAxisAlignment(Bess::Canvas::UI::LayoutAlignment::center);

    Bess::Canvas::UI::UINode childNode1;
    childNode1.setSizeConstraint(Bess::Canvas::UI::SizeContraint::fixed);
    childNode1.setSize(glm::vec2(50, 50));
    childNode1.setMargin(glm::vec4(5, 50, 5, 5));
    auto childNode1Ptr = registry.addNode(childNode1);
    parentNode.addChild(childNode1Ptr);

    Bess::Canvas::UI::UINode childNode2;
    childNode2.setSizeConstraint(Bess::Canvas::UI::SizeContraint::fixed);
    childNode2.setSize(glm::vec2(30, 30));
    auto childNode2Ptr = registry.addNode(childNode2);
    parentNode.addChild(childNode2Ptr);

    parentNode.layout(registry, Bess::UUID::null);

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
    parentNode.setSizeConstraint(Bess::Canvas::UI::SizeContraint::fixed);
    parentNode.setSize(glm::vec2(200, 100));
    parentNode.setMainAxisAlignment(Bess::Canvas::UI::LayoutAlignment::end);
    parentNode.setCrossAxisAlignment(Bess::Canvas::UI::LayoutAlignment::center);

    Bess::Canvas::UI::UINode childNode1;
    childNode1.setSizeConstraint(Bess::Canvas::UI::SizeContraint::fixed);
    childNode1.setSize(glm::vec2(50, 50));
    auto childNode1Ptr = registry.addNode(childNode1);
    ASSERT_NE(childNode1Ptr, nullptr);
    parentNode.addChild(childNode1Ptr);

    Bess::Canvas::UI::UINode childNode2;
    childNode2.setSizeConstraint(Bess::Canvas::UI::SizeContraint::fixed);
    childNode2.setSize(glm::vec2(30, 30));
    auto childNode2Ptr = registry.addNode(childNode2);
    ASSERT_NE(childNode2Ptr, nullptr);
    parentNode.addChild(childNode2Ptr);

    parentNode.layout(registry, Bess::UUID::null);

    expectVec2(childNode1Ptr->getCachedPos(), 45, 0);
    expectVec2(childNode2Ptr->getCachedPos(), 85, 0);
}

TEST_F(UiLayoutTests, FixedContainerGrowsToFitRelativeChildrenMargins) {
    Bess::Canvas::UI::UINodeRegistry registry;

    Bess::Canvas::UI::UINode parentNode;
    parentNode.setSizeConstraint(Bess::Canvas::UI::SizeContraint::fixed);
    parentNode.setSize(glm::vec2(200, 100));

    Bess::Canvas::UI::UINode childNode1;
    childNode1.setSizeConstraint(Bess::Canvas::UI::SizeContraint::fixed);
    childNode1.setSizeUnit(Bess::Canvas::UI::Unit::relative);
    childNode1.setSize(glm::vec2(0.5f, 0.2f));
    childNode1.setMargin(glm::vec4(0.f, 16.f, 0.f, 0.f));
    auto childNode1Ptr = registry.addNode(childNode1);
    ASSERT_NE(childNode1Ptr, nullptr);
    parentNode.addChild(childNode1Ptr);

    Bess::Canvas::UI::UINode childNode2;
    childNode2.setSizeConstraint(Bess::Canvas::UI::SizeContraint::fixed);
    childNode2.setSizeUnit(Bess::Canvas::UI::Unit::relative);
    childNode2.setSize(glm::vec2(0.5f, 0.2f));
    auto childNode2Ptr = registry.addNode(childNode2);
    ASSERT_NE(childNode2Ptr, nullptr);
    parentNode.addChild(childNode2Ptr);

    parentNode.layout(registry, Bess::UUID::null);

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
    rootNode.setSizeConstraint(Bess::Canvas::UI::SizeContraint::wrap_content);

    Bess::Canvas::UI::UINode slotsBoxNode;
    slotsBoxNode.setDirection(Bess::Canvas::UI::LayoutDirection::horizontal);
    slotsBoxNode.setSizeConstraint(Bess::Canvas::UI::SizeContraint::fixed);
    slotsBoxNode.setSizeUnit(Bess::Canvas::UI::Unit::relative);
    slotsBoxNode.setSize(glm::vec2(1.f, -1.f));
    auto slotsBoxNodePtr = registry.addNode(slotsBoxNode);
    ASSERT_NE(slotsBoxNodePtr, nullptr);
    rootNode.addChild(slotsBoxNodePtr);

    Bess::Canvas::UI::UINode inputBoxNode;
    inputBoxNode.setDirection(Bess::Canvas::UI::LayoutDirection::vertical);
    inputBoxNode.setSizeConstraint(Bess::Canvas::UI::SizeContraint::fixed);
    inputBoxNode.setSizeUnit(Bess::Canvas::UI::Unit::relative);
    inputBoxNode.setSize(glm::vec2(0.5f, -1.f));
    inputBoxNode.setMargin(glm::vec4(0.f, 16.f, 0.f, 0.f));
    auto inputBoxNodePtr = registry.addNode(inputBoxNode);
    ASSERT_NE(inputBoxNodePtr, nullptr);
    slotsBoxNodePtr->addChild(inputBoxNodePtr);

    Bess::Canvas::UI::UINode outputBoxNode;
    outputBoxNode.setDirection(Bess::Canvas::UI::LayoutDirection::vertical);
    outputBoxNode.setSizeConstraint(Bess::Canvas::UI::SizeContraint::fixed);
    outputBoxNode.setSizeUnit(Bess::Canvas::UI::Unit::relative);
    outputBoxNode.setSize(glm::vec2(0.5f, -1.f));
    auto outputBoxNodePtr = registry.addNode(outputBoxNode);
    ASSERT_NE(outputBoxNodePtr, nullptr);
    slotsBoxNodePtr->addChild(outputBoxNodePtr);

    Bess::Canvas::UI::UINode inputRowNode;
    inputRowNode.setSizeConstraint(Bess::Canvas::UI::SizeContraint::fixed);
    inputRowNode.setSize(glm::vec2(100.f, 20.f));
    auto inputRowNodePtr = registry.addNode(inputRowNode);
    ASSERT_NE(inputRowNodePtr, nullptr);
    inputBoxNodePtr->addChild(inputRowNodePtr);

    Bess::Canvas::UI::UINode outputRowNode;
    outputRowNode.setSizeConstraint(Bess::Canvas::UI::SizeContraint::fixed);
    outputRowNode.setSize(glm::vec2(100.f, 20.f));
    auto outputRowNodePtr = registry.addNode(outputRowNode);
    ASSERT_NE(outputRowNodePtr, nullptr);
    outputBoxNodePtr->addChild(outputRowNodePtr);

    rootNode.layout(registry, Bess::UUID::null);
    expectVec2(rootNode.getDrawSize(), 216.f, 20.f);
    expectVec2(slotsBoxNodePtr->getDrawSize(), 216.f, 20.f);
    expectVec2(inputBoxNodePtr->getCachedSize(), 116.f, 20.f);
    expectVec2(outputBoxNodePtr->getCachedSize(), 100.f, 20.f);

    rootNode.layout(registry, Bess::UUID::null);
    expectVec2(rootNode.getDrawSize(), 216.f, 20.f);
    expectVec2(slotsBoxNodePtr->getDrawSize(), 216.f, 20.f);
    expectVec2(inputBoxNodePtr->getCachedSize(), 116.f, 20.f);
    expectVec2(outputBoxNodePtr->getCachedSize(), 100.f, 20.f);
}

TEST_F(UiLayoutTests, OutputColumnCrossAxisEndAlignsRowsToRightEdge) {
    Bess::Canvas::UI::UINodeRegistry registry;

    Bess::Canvas::UI::UINode rootNode;
    rootNode.setDirection(Bess::Canvas::UI::LayoutDirection::vertical);
    rootNode.setSizeConstraint(Bess::Canvas::UI::SizeContraint::wrap_content);

    Bess::Canvas::UI::UINode headerNode;
    headerNode.setSizeConstraint(Bess::Canvas::UI::SizeContraint::fixed);
    headerNode.setSize(glm::vec2(200.f, 20.f));
    auto headerNodePtr = registry.addNode(headerNode);
    ASSERT_NE(headerNodePtr, nullptr);
    rootNode.addChild(headerNodePtr);

    Bess::Canvas::UI::UINode slotsBoxNode;
    slotsBoxNode.setDirection(Bess::Canvas::UI::LayoutDirection::horizontal);
    slotsBoxNode.setSizeConstraint(Bess::Canvas::UI::SizeContraint::fixed);
    slotsBoxNode.setSizeUnit(Bess::Canvas::UI::Unit::relative);
    slotsBoxNode.setSize(glm::vec2(1.f, -1.f));
    auto slotsBoxNodePtr = registry.addNode(slotsBoxNode);
    ASSERT_NE(slotsBoxNodePtr, nullptr);
    rootNode.addChild(slotsBoxNodePtr);

    Bess::Canvas::UI::UINode inputBoxNode;
    inputBoxNode.setDirection(Bess::Canvas::UI::LayoutDirection::vertical);
    inputBoxNode.setSizeConstraint(Bess::Canvas::UI::SizeContraint::fixed);
    inputBoxNode.setSizeUnit(Bess::Canvas::UI::Unit::relative);
    inputBoxNode.setSize(glm::vec2(0.5f, -1.f));
    inputBoxNode.setMargin(glm::vec4(0.f, 16.f, 0.f, 0.f));
    auto inputBoxNodePtr = registry.addNode(inputBoxNode);
    ASSERT_NE(inputBoxNodePtr, nullptr);
    slotsBoxNodePtr->addChild(inputBoxNodePtr);

    Bess::Canvas::UI::UINode outputBoxNode;
    outputBoxNode.setDirection(Bess::Canvas::UI::LayoutDirection::vertical);
    outputBoxNode.setCrossAxisAlignment(Bess::Canvas::UI::LayoutAlignment::end);
    outputBoxNode.setSizeConstraint(Bess::Canvas::UI::SizeContraint::fixed);
    outputBoxNode.setSizeUnit(Bess::Canvas::UI::Unit::relative);
    outputBoxNode.setSize(glm::vec2(0.5f, -1.f));
    auto outputBoxNodePtr = registry.addNode(outputBoxNode);
    ASSERT_NE(outputBoxNodePtr, nullptr);
    slotsBoxNodePtr->addChild(outputBoxNodePtr);

    Bess::Canvas::UI::UINode inputRowNode;
    inputRowNode.setSizeConstraint(Bess::Canvas::UI::SizeContraint::fixed);
    inputRowNode.setSize(glm::vec2(100.f, 20.f));
    auto inputRowNodePtr = registry.addNode(inputRowNode);
    ASSERT_NE(inputRowNodePtr, nullptr);
    inputBoxNodePtr->addChild(inputRowNodePtr);

    Bess::Canvas::UI::UINode outputRowNode;
    outputRowNode.setSizeConstraint(Bess::Canvas::UI::SizeContraint::fixed);
    outputRowNode.setSize(glm::vec2(30.f, 20.f));
    auto outputRowNodePtr = registry.addNode(outputRowNode);
    ASSERT_NE(outputRowNodePtr, nullptr);
    outputBoxNodePtr->addChild(outputRowNodePtr);

    rootNode.layout(registry, Bess::UUID::null);

    expectVec2(rootNode.getDrawSize(), 216.f, 40.f);
    expectVec2(slotsBoxNodePtr->getDrawSize(), 216.f, 20.f);
    expectVec2(outputBoxNodePtr->getDrawSize(), 100.f, 20.f);
    expectVec2(inputRowNodePtr->getCachedPos(), -58.f, 10.f);
    expectVec2(outputRowNodePtr->getCachedPos(), 93.f, 10.f);
}

TEST_F(UiLayoutTests, UINodeLayoutHonorsEndAlignmentInVerticalFlow) {
    Bess::Canvas::UI::UINodeRegistry registry;

    Bess::Canvas::UI::UINode parentNode;
    parentNode.setSizeConstraint(Bess::Canvas::UI::SizeContraint::fixed);
    parentNode.setSize(glm::vec2(100, 100));
    parentNode.setDirection(Bess::Canvas::UI::LayoutDirection::vertical);
    parentNode.setCrossAxisAlignment(Bess::Canvas::UI::LayoutAlignment::end);

    Bess::Canvas::UI::UINode childNode;
    childNode.setSizeConstraint(Bess::Canvas::UI::SizeContraint::fixed);
    childNode.setSize(glm::vec2(20, 30));
    auto childNodePtr = registry.addNode(childNode);
    ASSERT_NE(childNodePtr, nullptr);

    parentNode.addChild(childNodePtr);

    parentNode.layout(registry, Bess::UUID::null);

    expectVec2(childNodePtr->getCachedPos(), 40, -35);
}

TEST_F(UiLayoutTests, UINodeRelativeSizeUsesParentContentBox) {
    Bess::Canvas::UI::UINodeRegistry registry;

    Bess::Canvas::UI::UINode parentNode;
    parentNode.setSizeConstraint(Bess::Canvas::UI::SizeContraint::fixed);
    parentNode.setSize(glm::vec2(200, 100));
    parentNode.setPadding(glm::vec4(10, 10, 10, 10));

    Bess::Canvas::UI::UINode childNode;
    childNode.setSizeConstraint(Bess::Canvas::UI::SizeContraint::fixed);
    childNode.setSizeUnit(Bess::Canvas::UI::Unit::relative);
    childNode.setSize(glm::vec2(0.5f, 0.25f));
    auto childNodePtr = registry.addNode(childNode);
    ASSERT_NE(childNodePtr, nullptr);
    parentNode.addChild(childNodePtr);

    parentNode.layout(registry, Bess::UUID::null);

    expectVec2(childNodePtr->getDrawSize(), 90, 20);
    expectVec2(childNodePtr->getCachedPos(), -45, -30);
}

TEST_F(UiLayoutTests, UINodeLayoutRefreshesWhenChildSizeChanges) {
    Bess::Canvas::UI::UINodeRegistry registry;

    Bess::Canvas::UI::UINode parentNode;
    parentNode.setSizeConstraint(Bess::Canvas::UI::SizeContraint::fixed);
    parentNode.setSize(glm::vec2(200, 100));

    Bess::Canvas::UI::UINode childNode1;
    childNode1.setSizeConstraint(Bess::Canvas::UI::SizeContraint::fixed);
    childNode1.setSize(glm::vec2(50, 50));
    auto childNode1Ptr = registry.addNode(childNode1);
    ASSERT_NE(childNode1Ptr, nullptr);
    parentNode.addChild(childNode1Ptr);

    Bess::Canvas::UI::UINode childNode2;
    childNode2.setSizeConstraint(Bess::Canvas::UI::SizeContraint::fixed);
    childNode2.setSize(glm::vec2(30, 30));
    auto childNode2Ptr = registry.addNode(childNode2);
    ASSERT_NE(childNode2Ptr, nullptr);
    parentNode.addChild(childNode2Ptr);

    parentNode.layout(registry, Bess::UUID::null);

    expectVec2(childNode2Ptr->getCachedPos(), -35, -35);

    childNode1Ptr->setSize(glm::vec2(100, 50));
    parentNode.layout(registry, Bess::UUID::null);

    expectVec2(childNode1Ptr->getCachedPos(), -50, -25);
    expectVec2(childNode2Ptr->getCachedPos(), 15, -35);
}
