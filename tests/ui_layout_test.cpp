#include "bess_core/scene/scene_ui/layout.h"
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
