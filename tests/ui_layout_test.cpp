#include "bess_core/scene/scene_ui/layout.h"
#include "common/bess_uuid.h"
#include "common/logger.h"
#include <gtest/gtest.h>
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
    constexpr glm::vec2 wrapContentSize =
        glm::vec2(padding.x + padding.z, padding.y + padding.w) +
        glm::vec2(margin.x + margin.z, margin.y + margin.w);

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

    EXPECT_EQ(measuredSize.x, worldSize.x);
    EXPECT_EQ(measuredSize.y, worldSize.y);

    // checking for SizeContraint::fixed size
    EXPECT_EQ(worldSize.x, size.x);
    EXPECT_EQ(worldSize.y, size.y);

    BESS_INFO("Measured with fixed constraint");

    node.setSizeConstraint(Bess::Canvas::UI::SizeContraint::wrap_content);
    measuredSize = node.measure(registry, Bess::UUID::null);
    worldSize = node.getCachedSize();

    // checking for SizeContraint::wrap_content size
    EXPECT_EQ(measuredSize.x, worldSize.x);
    EXPECT_EQ(measuredSize.y, worldSize.y);

    EXPECT_EQ(worldSize.x, wrapContentSize.x);
    EXPECT_EQ(worldSize.y, wrapContentSize.y);

    BESS_INFO("Measured with wrap_content constraint");

    Bess::Canvas::UI::UINode childNode;
    childNode.setSize(glm::vec2(20, 10));
    registry.addNode(childNode);
    node.addChild(childNode.getId());

    measuredSize = node.measure(registry, Bess::UUID::null);
    worldSize = node.getCachedSize();

    // checking for SizeContraint::wrap_content size with child
    EXPECT_EQ(measuredSize.x, worldSize.x);
    EXPECT_EQ(measuredSize.y, worldSize.y);

    EXPECT_EQ(worldSize.x, wrapContentSize.x + 20);
    EXPECT_EQ(worldSize.y, wrapContentSize.y + 10);

    auto childNodePtr = registry.getNode(childNode.getId());
    ASSERT_NE(childNodePtr, nullptr);
    EXPECT_EQ(childNodePtr->getCachedSize().x, 20);
    EXPECT_EQ(childNodePtr->getCachedSize().y, 10);

    BESS_INFO("Measured with wrap_content constraint and child node");

    node.setMinSize(glm::vec2(150, 100));
    measuredSize = node.measure(registry, Bess::UUID::null);
    worldSize = node.getCachedSize();

    // checking for SizeContraint::wrap_content size with child and min size
    EXPECT_EQ(measuredSize.x, worldSize.x);
    EXPECT_EQ(measuredSize.y, worldSize.y);

    EXPECT_EQ(worldSize.x, 150);
    EXPECT_EQ(worldSize.y, 100);

    BESS_INFO("Measured with wrap_content constraint, child node and min size");

    node.setMaxSize(glm::vec2(120, 80));
    measuredSize = node.measure(registry, Bess::UUID::null);
    worldSize = node.getCachedSize();

    // checking for SizeContraint::wrap_content size with child, min size and
    // max size
    EXPECT_EQ(measuredSize.x, worldSize.x);
    EXPECT_EQ(measuredSize.y, worldSize.y);

    // Minimum width overrides maximum width
    EXPECT_EQ(worldSize.x, 150);
    EXPECT_EQ(worldSize.y, 100);

    node.setMinSize(glm::vec2(50, 40));
    node.setSize(glm::vec2(200, 150));

    node.setSizeConstraint(Bess::Canvas::UI::SizeContraint::fixed);
    measuredSize = node.measure(registry, Bess::UUID::null);
    worldSize = node.getCachedSize();

    EXPECT_EQ(measuredSize.x, worldSize.x);
    EXPECT_EQ(measuredSize.y, worldSize.y);

    EXPECT_EQ(worldSize.x, 120);
    EXPECT_EQ(worldSize.y, 80);

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
    EXPECT_EQ(childSize.x, 50);
    EXPECT_EQ(childSize.y, 50);

    childSize = childNode2Ptr->getCachedSize();
    EXPECT_EQ(childSize.x, 30);
    EXPECT_EQ(childSize.y, 30);

    parentNode.layout(registry, Bess::UUID::null);

    auto parentPos = parentNode.getCachedPos();

    auto child1Pos = childNode1Ptr->getCachedPos();
    auto child2Pos = childNode2Ptr->getCachedPos();

    EXPECT_EQ(child1Pos.x, -75);
    EXPECT_EQ(child1Pos.y, -25);

    EXPECT_EQ(child2Pos.x, -35);
    EXPECT_EQ(child2Pos.y, -35);

    BESS_INFO("Checked layout positions of child nodes");
}
