

#include "common/bess_uuid.h"
#include "ui_core.h"
#include <gtest/gtest.h>

class DockManagerTests : public testing::Test {
  protected:
    void SetUp() override {
    }
};

TEST_F(DockManagerTests, Init) {
    Bess::UI::DockManager dockManager;
    dockManager.init();
    ASSERT_TRUE(dockManager.getRootNode() != Bess::UUID::null);
}

TEST_F(DockManagerTests, CreatingNodes) {
    Bess::UI::DockManager dockManager;
    dockManager.init();

    auto leaf = dockManager.createNode<Bess::UI::DockLeaf>();
    auto tab = dockManager.createNode<Bess::UI::DockTab>();
    auto split = dockManager.createNode<Bess::UI::DockSplitter>();

    ASSERT_TRUE(leaf->isLeaf());
    ASSERT_TRUE(tab->isTab());
    ASSERT_TRUE(split->isSplitter());

    auto splitWithArgs = dockManager.createNode<Bess::UI::DockSplitter>(
        Bess::UI::SplitDirection::horizontal, 0.7f);

    ASSERT_TRUE(splitWithArgs->isSplitter());
    ASSERT_EQ(splitWithArgs->getSplitDir(),
              Bess::UI::SplitDirection::horizontal);
    ASSERT_FLOAT_EQ(splitWithArgs->getSplitRatio(), 0.7f);
}

TEST_F(DockManagerTests, DockingToLeafMainZone) {
    Bess::UI::DockManager dockManager;
    dockManager.init();

    auto leaf1 = dockManager.createNode<Bess::UI::DockLeaf>();
    auto leaf2 = dockManager.createNode<Bess::UI::DockLeaf>();

    auto leaf1Id = leaf1->getId(); // This will become tab node

    // Docking leaf2 to leaf1 in the main zone should create a tab node
    auto res =
        dockManager.dockNode(leaf2->getId(), leaf1Id, Bess::UI::DockZone::main);
    ASSERT_TRUE(res);

    auto node1 = dockManager.getNode(leaf1Id);

    ASSERT_NE(leaf1Id, leaf1->getId()); // The id should have changed since
                                        // it is now a tab node

    ASSERT_TRUE(node1->isTab());
    ASSERT_TRUE(leaf1->isLeaf());
    ASSERT_TRUE(leaf2->isLeaf());

    auto tabNode = std::dynamic_pointer_cast<Bess::UI::DockTab>(node1);
    ASSERT_TRUE(tabNode != nullptr);
    const auto &dockedNodes = tabNode->getDockedNodes();
    ASSERT_EQ(dockedNodes.size(), 2);

    ASSERT_EQ(dockedNodes[0], leaf1->getId());
    ASSERT_EQ(dockedNodes[1], leaf2->getId());
}

TEST_F(DockManagerTests, DockingToLeafLeftZone) {
    Bess::UI::DockManager dockManager;
    dockManager.init();

    auto leaf1 = dockManager.createNode<Bess::UI::DockLeaf>();
    auto leaf2 = dockManager.createNode<Bess::UI::DockLeaf>();

    auto leaf1Id = leaf1->getId(); // This will become a splitter node

    // Docking leaf2 to leaf1 in the left zone should create a splitter node
    auto res =
        dockManager.dockNode(leaf2->getId(), leaf1Id, Bess::UI::DockZone::left);
    ASSERT_TRUE(res);

    auto node1 = dockManager.getNode(leaf1Id);

    ASSERT_NE(leaf1Id, leaf1->getId()); // The id should have changed since
                                        // it is now a splitter node

    ASSERT_TRUE(node1->isSplitter());
    ASSERT_TRUE(leaf1->isLeaf());
    ASSERT_TRUE(leaf2->isLeaf());

    auto splitNode = std::dynamic_pointer_cast<Bess::UI::DockSplitter>(node1);
    ASSERT_TRUE(splitNode != nullptr);
    const auto &splitNodes = splitNode->getSplitNodes();

    ASSERT_EQ(splitNodes.first, leaf2->getId());
    ASSERT_EQ(splitNodes.second, leaf1->getId());
}

TEST_F(DockManagerTests, DockingToLeafRightZone) {
    Bess::UI::DockManager dockManager;
    dockManager.init();

    auto leaf1 = dockManager.createNode<Bess::UI::DockLeaf>();
    auto leaf2 = dockManager.createNode<Bess::UI::DockLeaf>();

    auto leaf1Id = leaf1->getId(); // This will become a splitter node

    // Docking leaf2 to leaf1 in the right zone should create a splitter node
    auto res = dockManager.dockNode(
        leaf2->getId(), leaf1Id, Bess::UI::DockZone::right);
    ASSERT_TRUE(res);

    auto node1 = dockManager.getNode(leaf1Id);

    ASSERT_NE(leaf1Id, leaf1->getId()); // The id should have changed since
                                        // it is now a splitter node

    ASSERT_TRUE(node1->isSplitter());
    ASSERT_TRUE(leaf1->isLeaf());
    ASSERT_TRUE(leaf2->isLeaf());

    auto splitNode = std::dynamic_pointer_cast<Bess::UI::DockSplitter>(node1);
    ASSERT_TRUE(splitNode != nullptr);
    const auto &splitNodes = splitNode->getSplitNodes();

    ASSERT_EQ(splitNodes.first, leaf1->getId());
    ASSERT_EQ(splitNodes.second, leaf2->getId());
}

TEST_F(DockManagerTests, DockingToLeafTopZone) {
    Bess::UI::DockManager dockManager;
    dockManager.init();

    auto leaf1 = dockManager.createNode<Bess::UI::DockLeaf>();
    auto leaf2 = dockManager.createNode<Bess::UI::DockLeaf>();

    auto leaf1Id = leaf1->getId(); // This will become a splitter node

    // Docking leaf2 to leaf1 in the top zone should create a splitter node
    auto res =
        dockManager.dockNode(leaf2->getId(), leaf1Id, Bess::UI::DockZone::top);
    ASSERT_TRUE(res);

    auto node1 = dockManager.getNode(leaf1Id);

    ASSERT_NE(leaf1Id, leaf1->getId()); // The id should have changed since
                                        // it is now a splitter node

    ASSERT_TRUE(node1->isSplitter());
    ASSERT_TRUE(leaf1->isLeaf());
    ASSERT_TRUE(leaf2->isLeaf());

    auto splitNode = std::dynamic_pointer_cast<Bess::UI::DockSplitter>(node1);
    ASSERT_TRUE(splitNode != nullptr);
    const auto &splitNodes = splitNode->getSplitNodes();

    ASSERT_EQ(splitNodes.first, leaf2->getId());
    ASSERT_EQ(splitNodes.second, leaf1->getId());
}

TEST_F(DockManagerTests, DockingToLeafBottomZone) {
    Bess::UI::DockManager dockManager;
    dockManager.init();

    auto leaf1 = dockManager.createNode<Bess::UI::DockLeaf>();
    auto leaf2 = dockManager.createNode<Bess::UI::DockLeaf>();

    auto leaf1Id = leaf1->getId(); // This will become a splitter node

    // Docking leaf2 to leaf1 in the bottom zone should create a splitter node
    auto res = dockManager.dockNode(
        leaf2->getId(), leaf1Id, Bess::UI::DockZone::bottom);
    ASSERT_TRUE(res);

    auto node1 = dockManager.getNode(leaf1Id);

    ASSERT_NE(leaf1Id, leaf1->getId()); // The id should have changed since
                                        // it is now a splitter node

    ASSERT_TRUE(node1->isSplitter());
    ASSERT_TRUE(leaf1->isLeaf());
    ASSERT_TRUE(leaf2->isLeaf());

    auto splitNode = std::dynamic_pointer_cast<Bess::UI::DockSplitter>(node1);
    ASSERT_TRUE(splitNode != nullptr);
    const auto &splitNodes = splitNode->getSplitNodes();

    ASSERT_EQ(splitNodes.first, leaf1->getId());
    ASSERT_EQ(splitNodes.second, leaf2->getId());
}

TEST_F(DockManagerTests, DockingToTabMainZone) {
    Bess::UI::DockManager dockManager;
    dockManager.init();

    auto tabNode = dockManager.createNode<Bess::UI::DockTab>();
    auto leaf1 = dockManager.createNode<Bess::UI::DockLeaf>();
    auto leaf2 = dockManager.createNode<Bess::UI::DockLeaf>();

    // Dock leaf1 to the tab node in the main zone
    auto res = dockManager.dockNode(
        leaf1->getId(), tabNode->getId(), Bess::UI::DockZone::main);

    ASSERT_TRUE(res);

    ASSERT_TRUE(tabNode->isTab());

    res = dockManager.dockNode(
        leaf2->getId(), tabNode->getId(), Bess::UI::DockZone::main);
    ASSERT_TRUE(res);

    const auto &dockedNodes = tabNode->getDockedNodes();
    ASSERT_EQ(dockedNodes.size(), 2);
    ASSERT_EQ(dockedNodes[0], leaf1->getId());
    ASSERT_EQ(dockedNodes[1], leaf2->getId());
}

TEST_F(DockManagerTests, DockingToTabLeftZone) {
    Bess::UI::DockManager dockManager;
    dockManager.init();

    auto tabNode = dockManager.createNode<Bess::UI::DockTab>();
    auto leaf1 = dockManager.createNode<Bess::UI::DockLeaf>();

    const auto tabNodeId = tabNode->getId(); // Store the original tab node ID

    // Dock leaf1 to the tab node in the left zone
    auto res = dockManager.dockNode(
        leaf1->getId(), tabNode->getId(), Bess::UI::DockZone::left);
    ASSERT_TRUE(res);

    ASSERT_TRUE(
        tabNode->isTab()); // Ensure the original tab node is still a tab

    auto newNode = dockManager.getNode(tabNodeId);
    ASSERT_TRUE(newNode->isSplitter());

    auto splitNode = std::dynamic_pointer_cast<Bess::UI::DockSplitter>(newNode);
    ASSERT_TRUE(splitNode != nullptr);

    const auto &splitNodes = splitNode->getSplitNodes();
    ASSERT_EQ(splitNodes.first, leaf1->getId());
    ASSERT_EQ(splitNodes.second, tabNode->getId());
}

TEST_F(DockManagerTests, DockingToTabRightZone) {
    Bess::UI::DockManager dockManager;
    dockManager.init();

    auto tabNode = dockManager.createNode<Bess::UI::DockTab>();
    auto leaf1 = dockManager.createNode<Bess::UI::DockLeaf>();

    const auto tabNodeId = tabNode->getId(); // Store the original tab node ID
    // Dock leaf1 to the tab node in the right zone
    auto res = dockManager.dockNode(
        leaf1->getId(), tabNode->getId(), Bess::UI::DockZone::right);
    ASSERT_TRUE(res);

    ASSERT_TRUE(
        tabNode->isTab()); // Ensure the original tab node is still a tab

    auto newNode = dockManager.getNode(tabNodeId);
    ASSERT_TRUE(newNode->isSplitter());

    auto splitNode = std::dynamic_pointer_cast<Bess::UI::DockSplitter>(newNode);
    ASSERT_TRUE(splitNode != nullptr);

    const auto &splitNodes = splitNode->getSplitNodes();
    ASSERT_EQ(splitNodes.first, tabNode->getId());
    ASSERT_EQ(splitNodes.second, leaf1->getId());
}

TEST_F(DockManagerTests, DockingToTabTopZone) {
    Bess::UI::DockManager dockManager;
    dockManager.init();

    auto tabNode = dockManager.createNode<Bess::UI::DockTab>();
    auto leaf1 = dockManager.createNode<Bess::UI::DockLeaf>();

    const auto tabNodeId = tabNode->getId(); // Store the original tab node ID

    // Dock leaf1 to the tab node in the top zone
    auto res = dockManager.dockNode(
        leaf1->getId(), tabNode->getId(), Bess::UI::DockZone::top);
    ASSERT_TRUE(res);

    auto newNode = dockManager.getNode(tabNodeId);
    ASSERT_TRUE(newNode->isSplitter());
    ASSERT_TRUE(
        tabNode->isTab()); // Ensure the original tab node is still a tab

    auto splitNode = std::dynamic_pointer_cast<Bess::UI::DockSplitter>(newNode);
    ASSERT_TRUE(splitNode != nullptr);

    const auto &splitNodes = splitNode->getSplitNodes();
    ASSERT_EQ(splitNodes.first, leaf1->getId());
    ASSERT_EQ(splitNodes.second, tabNode->getId());
}

TEST_F(DockManagerTests, DockingToTabBottomZone) {
    Bess::UI::DockManager dockManager;
    dockManager.init();

    auto tabNode = dockManager.createNode<Bess::UI::DockTab>();
    auto leaf1 = dockManager.createNode<Bess::UI::DockLeaf>();

    const auto tabNodeId = tabNode->getId(); // Store the original tab node ID

    // Dock leaf1 to the tab node in the bottom zone
    auto res = dockManager.dockNode(
        leaf1->getId(), tabNode->getId(), Bess::UI::DockZone::bottom);
    ASSERT_TRUE(res);

    ASSERT_TRUE(
        tabNode->isTab()); // Ensure the original tab node is still a tab

    auto newNode = dockManager.getNode(tabNodeId);
    ASSERT_TRUE(newNode->isSplitter());

    auto splitNode = std::dynamic_pointer_cast<Bess::UI::DockSplitter>(newNode);
    ASSERT_TRUE(splitNode != nullptr);

    const auto &splitNodes = splitNode->getSplitNodes();
    ASSERT_EQ(splitNodes.first, tabNode->getId());
    ASSERT_EQ(splitNodes.second, leaf1->getId());
}

TEST_F(DockManagerTests, DockingToSplitterLeftZone) {
    Bess::UI::DockManager dockManager;
    dockManager.init();

    auto splitterNode = dockManager.createNode<Bess::UI::DockSplitter>();
    auto leaf1 = dockManager.createNode<Bess::UI::DockLeaf>();

    const auto ogId =
        splitterNode->getId(); // Store the original splitter node ID

    // Dock leaf1 to the splitter node in the left zone
    auto res = dockManager.dockNode(
        leaf1->getId(), splitterNode->getId(), Bess::UI::DockZone::left);
    ASSERT_TRUE(res);

    auto newNode = dockManager.getNode(ogId);
    ASSERT_TRUE(newNode->isSplitter());
    ASSERT_TRUE(splitterNode->isSplitter());

    auto splitNode = std::dynamic_pointer_cast<Bess::UI::DockSplitter>(newNode);
    ASSERT_TRUE(splitNode != nullptr);

    const auto &splitNodes = splitNode->getSplitNodes();
    ASSERT_EQ(splitNodes.first, leaf1->getId());
    ASSERT_EQ(splitNodes.second, splitterNode->getId());
}

TEST_F(DockManagerTests, DockingToSplitterRightZone) {
    Bess::UI::DockManager dockManager;
    dockManager.init();

    auto splitterNode = dockManager.createNode<Bess::UI::DockSplitter>();
    auto leaf1 = dockManager.createNode<Bess::UI::DockLeaf>();

    auto ogId = splitterNode->getId(); // Store the original splitter node ID

    // Dock leaf1 to the splitter node in the right zone
    auto res = dockManager.dockNode(
        leaf1->getId(), splitterNode->getId(), Bess::UI::DockZone::right);
    ASSERT_TRUE(res);

    auto newNode = dockManager.getNode(ogId);
    ASSERT_TRUE(newNode->isSplitter());
    ASSERT_TRUE(splitterNode->isSplitter());

    auto splitNode = std::dynamic_pointer_cast<Bess::UI::DockSplitter>(newNode);
    ASSERT_TRUE(splitNode != nullptr);

    const auto &splitNodes = splitNode->getSplitNodes();
    ASSERT_EQ(splitNodes.first, splitterNode->getId());
    ASSERT_EQ(splitNodes.second, leaf1->getId());
}

TEST_F(DockManagerTests, DockingToSplitterTopZone) {
    Bess::UI::DockManager dockManager;
    dockManager.init();

    auto splitterNode = dockManager.createNode<Bess::UI::DockSplitter>();
    auto leaf1 = dockManager.createNode<Bess::UI::DockLeaf>();

    auto ogId = splitterNode->getId(); // Store the original splitter node ID

    // Dock leaf1 to the splitter node in the top zone
    auto res = dockManager.dockNode(
        leaf1->getId(), splitterNode->getId(), Bess::UI::DockZone::top);
    ASSERT_TRUE(res);
    ASSERT_TRUE(splitterNode->isSplitter());

    auto newNode = dockManager.getNode(ogId);
    ASSERT_TRUE(newNode->isSplitter());

    auto splitNode = std::dynamic_pointer_cast<Bess::UI::DockSplitter>(newNode);
    ASSERT_TRUE(splitNode != nullptr);

    const auto &splitNodes = splitNode->getSplitNodes();
    ASSERT_EQ(splitNodes.first, leaf1->getId());
    ASSERT_EQ(splitNodes.second, splitterNode->getId());
}

TEST_F(DockManagerTests, DockingToSplitterBottomZone) {
    Bess::UI::DockManager dockManager;
    dockManager.init();

    auto splitterNode = dockManager.createNode<Bess::UI::DockSplitter>();
    auto leaf1 = dockManager.createNode<Bess::UI::DockLeaf>();

    auto ogId = splitterNode->getId(); // Store the original splitter node ID

    // Dock leaf1 to the splitter node in the bottom zone
    auto res = dockManager.dockNode(
        leaf1->getId(), splitterNode->getId(), Bess::UI::DockZone::bottom);

    ASSERT_TRUE(res);
    ASSERT_TRUE(splitterNode->isSplitter());

    auto newNode = dockManager.getNode(ogId);
    ASSERT_TRUE(newNode->isSplitter());

    auto splitNode = std::dynamic_pointer_cast<Bess::UI::DockSplitter>(newNode);
    ASSERT_TRUE(splitNode != nullptr);

    const auto &splitNodes = splitNode->getSplitNodes();
    ASSERT_EQ(splitNodes.first, splitterNode->getId());
    ASSERT_EQ(splitNodes.second, leaf1->getId());
}

TEST_F(DockManagerTests, UndockingFromTab) {
    Bess::UI::DockManager dockManager;
    dockManager.init();

    auto tabNode = dockManager.createNode<Bess::UI::DockTab>();
    auto leaf1 = dockManager.createNode<Bess::UI::DockLeaf>();
    auto leaf2 = dockManager.createNode<Bess::UI::DockLeaf>();
    auto leaf3 = dockManager.createNode<Bess::UI::DockLeaf>();

    // Dock leaf1 and leaf2 to the tab node in the main zone
    dockManager.dockNode(
        leaf1->getId(), tabNode->getId(), Bess::UI::DockZone::main);
    dockManager.dockNode(
        leaf2->getId(), tabNode->getId(), Bess::UI::DockZone::main);
    dockManager.dockNode(
        leaf3->getId(), tabNode->getId(), Bess::UI::DockZone::main);

    const auto &dockedNodesBefore = tabNode->getDockedNodes();
    ASSERT_EQ(dockedNodesBefore.size(), 3);

    const auto tabNodeId = tabNode->getId(); // Store the original tab node ID

    // Undock leaf1 from the tab node
    bool res = dockManager.undockNode(leaf1->getId());
    ASSERT_TRUE(res);

    const auto &dockedNodesAfter = tabNode->getDockedNodes();
    ASSERT_EQ(dockedNodesAfter.size(), 2);

    ASSERT_EQ(tabNodeId,
              tabNode->getId()); // Ensure the tab node ID remains the same

    ASSERT_EQ(dockedNodesAfter[0], leaf2->getId());
    ASSERT_EQ(dockedNodesAfter[1], leaf3->getId());
}

TEST_F(DockManagerTests, UndockingFromTabToLeaf) {

    Bess::UI::DockManager dockManager;
    dockManager.init();

    auto tabNode = dockManager.createNode<Bess::UI::DockTab>();
    auto leaf1 = dockManager.createNode<Bess::UI::DockLeaf>();
    auto leaf2 = dockManager.createNode<Bess::UI::DockLeaf>();

    // Dock leaf1 and leaf2 to the tab node in the main zone
    dockManager.dockNode(
        leaf1->getId(), tabNode->getId(), Bess::UI::DockZone::main);
    dockManager.dockNode(
        leaf2->getId(), tabNode->getId(), Bess::UI::DockZone::main);

    const auto &dockedNodesBefore = tabNode->getDockedNodes();
    ASSERT_EQ(dockedNodesBefore.size(), 2);

    const auto tabNodeId = tabNode->getId(); // Store the original tab node ID

    // Undock leaf1 from the tab node
    bool res = dockManager.undockNode(leaf1->getId());
    ASSERT_TRUE(res);

    const auto &dockedNodesAfter = tabNode->getDockedNodes();
    ASSERT_EQ(dockedNodesAfter.size(), 1);

    ASSERT_EQ(tabNodeId, leaf2->getId());

    // Check if tabnode is removed
    auto node = dockManager.getNode(tabNode->getId());
    ASSERT_TRUE(node == nullptr);
}

TEST_F(DockManagerTests, UndockingFromSplitter) {
    Bess::UI::DockManager dockManager;
    dockManager.init();

    auto splitterNode = dockManager.createNode<Bess::UI::DockSplitter>();

    auto leaf1 = dockManager.createNode<Bess::UI::DockLeaf>();
    leaf1->setDockedTo(splitterNode->getId());

    auto leaf2 = dockManager.createNode<Bess::UI::DockLeaf>();
    leaf2->setDockedTo(splitterNode->getId());

    auto &splitNodes = splitterNode->getSplitNodes();
    splitNodes.first = leaf1->getId();
    splitNodes.second = leaf2->getId();

    const auto splitterNodeId =
        splitterNode->getId(); // Store the original splitter node ID

    // Undock leaf1 from the splitter node
    bool res = dockManager.undockNode(leaf1->getId());
    ASSERT_TRUE(res);

    // The splitter node should now only have leaf2 as its child
    auto newNode = dockManager.getNode(splitterNodeId);
    ASSERT_TRUE(newNode->isLeaf()); // Checking if the splitter node has been
                                    // replaced by leaf2

    // Check if the splitter node is removed
    auto node = dockManager.getNode(splitterNode->getId());
    ASSERT_TRUE(node == nullptr);
}

TEST_F(DockManagerTests, UndockingFloatingNode) {
    Bess::UI::DockManager dockManager;
    dockManager.init();

    auto leaf1 = dockManager.createNode<Bess::UI::DockLeaf>();

    // Undock a floating node (which is already floating)
    bool res = dockManager.undockNode(leaf1->getId());

    ASSERT_FALSE(res);
}
