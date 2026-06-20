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
    childNode.setSize(glm::vec2(20, 10));
    registry.addNode(childNode);
    node.addChild(childNode.getId());

    measuredSize = node.measure(registry, Bess::UUID::null);
    worldSize = node.getCachedSize();

    // Child layout footprints are placed inside the padded content box.
    expectVec2(measuredSize, worldSize.x, worldSize.y);

    expectVec2(node.getDrawSize(), paddingSize.x + 20, paddingSize.y + 10);
    expectVec2(worldSize,
               paddingSize.x + 20 + marginSize.x,
               paddingSize.y + 10 + marginSize.y);

    auto childNodePtr = registry.getNode(childNode.getId());
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
    parentNode.setSize(glm::vec2(200, 100));
    parentNode.setPos(glm::vec2(0, 0));
    registry.addNode(parentNode);

    Bess::Canvas::UI::UINode *childNode1Ptr = nullptr;
    Bess::Canvas::UI::UINode *childNode2Ptr = nullptr;
    {
        Bess::Canvas::UI::UINode childNode1;
        childNode1.setSize(glm::vec2(50, 50));
        registry.addNode(childNode1);
        parentNode.addChild(childNode1.getId());
        childNode1Ptr = registry.getNode(childNode1.getId());
    }

    {
        Bess::Canvas::UI::UINode childNode2;
        childNode2.setSize(glm::vec2(30, 30));
        registry.addNode(childNode2);
        parentNode.addChild(childNode2.getId());
        childNode2Ptr = registry.getNode(childNode2.getId());
    }

    EXPECT_EQ(parentNode.getAlignment(),
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
    parentNode.setSize(glm::vec2(200, 100));
    parentNode.setAlignment(Bess::Canvas::UI::LayoutAlignment::center);

    Bess::Canvas::UI::UINode childNode1;
    childNode1.setSize(glm::vec2(50, 50));
    childNode1.setMargin(glm::vec4(5, 50, 5, 5));
    registry.addNode(childNode1);
    parentNode.addChild(childNode1.getId());

    Bess::Canvas::UI::UINode childNode2;
    childNode2.setSize(glm::vec2(30, 30));
    registry.addNode(childNode2);
    parentNode.addChild(childNode2.getId());

    parentNode.layout(registry, Bess::UUID::null);

    const auto *childNode1Ptr = registry.getNode(childNode1.getId());
    const auto *childNode2Ptr = registry.getNode(childNode2.getId());
    ASSERT_NE(childNode1Ptr, nullptr);
    ASSERT_NE(childNode2Ptr, nullptr);

    expectVec2(parentNode.getDrawSize(), 200, 100);
    expectVec2(childNode1Ptr->getCachedSize(), 105, 60);
    expectVec2(childNode1Ptr->getDrawSize(), 50, 50);
    expectVec2(childNode1Ptr->getCachedPos(), -70, 0);
    expectVec2(childNode2Ptr->getCachedPos(), 20, 0);
}

TEST_F(UiLayoutTests, UINodeLayoutHonorsEndAlignmentInVerticalFlow) {
    Bess::Canvas::UI::UINodeRegistry registry;

    Bess::Canvas::UI::UINode parentNode;
    parentNode.setSize(glm::vec2(100, 100));
    parentNode.setDirection(Bess::Canvas::UI::LayoutDirection::vertical);
    parentNode.setAlignment(Bess::Canvas::UI::LayoutAlignment::end);

    Bess::Canvas::UI::UINode childNode;
    childNode.setSize(glm::vec2(20, 30));
    registry.addNode(childNode);
    parentNode.addChild(childNode.getId());

    parentNode.layout(registry, Bess::UUID::null);

    const auto *childNodePtr = registry.getNode(childNode.getId());
    ASSERT_NE(childNodePtr, nullptr);
    expectVec2(childNodePtr->getCachedPos(), 40, -35);
}

TEST_F(UiLayoutTests, UINodeRelativeSizeUsesParentContentBox) {
    Bess::Canvas::UI::UINodeRegistry registry;

    Bess::Canvas::UI::UINode parentNode;
    parentNode.setSize(glm::vec2(200, 100));
    parentNode.setPadding(glm::vec4(10, 10, 10, 10));

    Bess::Canvas::UI::UINode childNode;
    childNode.setSizeUnit(Bess::Canvas::UI::Unit::relative);
    childNode.setSize(glm::vec2(0.5f, 0.25f));
    registry.addNode(childNode);
    parentNode.addChild(childNode.getId());

    parentNode.layout(registry, Bess::UUID::null);

    const auto *childNodePtr = registry.getNode(childNode.getId());
    ASSERT_NE(childNodePtr, nullptr);
    expectVec2(childNodePtr->getDrawSize(), 90, 20);
    expectVec2(childNodePtr->getCachedPos(), -45, -30);
}

TEST_F(UiLayoutTests, UINodeLayoutRefreshesWhenChildSizeChanges) {
    Bess::Canvas::UI::UINodeRegistry registry;

    Bess::Canvas::UI::UINode parentNode;
    parentNode.setSize(glm::vec2(200, 100));

    Bess::Canvas::UI::UINode childNode1;
    childNode1.setSize(glm::vec2(50, 50));
    registry.addNode(childNode1);
    parentNode.addChild(childNode1.getId());

    Bess::Canvas::UI::UINode childNode2;
    childNode2.setSize(glm::vec2(30, 30));
    registry.addNode(childNode2);
    parentNode.addChild(childNode2.getId());

    parentNode.layout(registry, Bess::UUID::null);

    auto *childNode1Ptr = registry.getNode(childNode1.getId());
    const auto *childNode2Ptr = registry.getNode(childNode2.getId());
    ASSERT_NE(childNode1Ptr, nullptr);
    ASSERT_NE(childNode2Ptr, nullptr);
    expectVec2(childNode2Ptr->getCachedPos(), -35, -35);

    childNode1Ptr->setSize(glm::vec2(100, 50));
    parentNode.layout(registry, Bess::UUID::null);

    expectVec2(childNode1Ptr->getCachedPos(), -50, -25);
    expectVec2(childNode2Ptr->getCachedPos(), 15, -35);
}
