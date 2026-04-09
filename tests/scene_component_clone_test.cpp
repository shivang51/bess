#include "pages/main_page/scene_components/group_scene_component.h"
#include "test_scene_graph_fixture.h"
#include <unordered_set>

using namespace Bess::Tests;

class SceneComponentCloneTest : public SceneGraphTestBase {};

TEST_F(SceneComponentCloneTest, SimulationCloneProducesFreshDetachedComponentTree) {
    const auto leftInput = addSimComponentDirect(scene, inputDef);
    const auto rightInput = addSimComponentDirect(scene, inputDef);
    const auto gate = addSimComponentDirect(scene, andDef);

    ASSERT_NE(connectSlots(leftInput.firstOutput(), gate.firstInput()), nullptr);
    ASSERT_NE(connectSlots(rightInput.firstOutput(), gate.inputs.at(1)), nullptr);

    const auto clones = gate.comp->clone(scene->getState());
    ASSERT_EQ(clones.size(), 1u + gate.inputs.size() + gate.outputs.size());

    const auto clonedGate = std::dynamic_pointer_cast<Bess::Canvas::SimulationSceneComponent>(clones.front());
    ASSERT_NE(clonedGate, nullptr);
    EXPECT_NE(clonedGate->getUuid(), gate.comp->getUuid());
    EXPECT_EQ(clonedGate->getSimEngineId(), Bess::UUID::null);
    EXPECT_EQ(clonedGate->getNetId(), Bess::UUID::null);
    EXPECT_NE(clonedGate->getCompDef(), nullptr);
    EXPECT_NE(clonedGate->getCompDef(), gate.comp->getCompDef());
    EXPECT_EQ(clonedGate->getCompDef()->getName(), gate.comp->getCompDef()->getName());
    EXPECT_TRUE(clonedGate->getChildComponents().empty());
    EXPECT_FALSE(clonedGate->getIsSelected());

    std::unordered_set<Bess::UUID> originalSlotIds;
    for (const auto &slot : gate.inputs) {
        originalSlotIds.insert(slot->getUuid());
    }
    for (const auto &slot : gate.outputs) {
        originalSlotIds.insert(slot->getUuid());
    }

    std::unordered_set<Bess::UUID> clonedSlotIds;
    for (size_t i = 1; i < clones.size(); ++i) {
        const auto clonedSlot = std::dynamic_pointer_cast<Bess::Canvas::SlotSceneComponent>(clones[i]);
        ASSERT_NE(clonedSlot, nullptr);
        EXPECT_FALSE(originalSlotIds.contains(clonedSlot->getUuid()));
        EXPECT_EQ(clonedSlot->getParentComponent(), Bess::UUID::null);
        EXPECT_TRUE(clonedSlot->getConnectedConnections().empty());
        EXPECT_EQ(clonedSlot->getRuntimeId(), Bess::Canvas::PickingId::invalidRuntimeId);
        clonedSlotIds.insert(clonedSlot->getUuid());
    }

    for (const auto &slotId : clonedGate->getInputSlots()) {
        EXPECT_TRUE(clonedSlotIds.contains(slotId));
    }
    for (const auto &slotId : clonedGate->getOutputSlots()) {
        EXPECT_TRUE(clonedSlotIds.contains(slotId));
    }
}

TEST_F(SceneComponentCloneTest, GroupClonePreservesChildContentWithNewHierarchyIds) {
    auto group = Bess::Canvas::GroupSceneComponent::create("Documentation");
    scene->getState().addComponent(group);

    auto firstLabel = addTextComponentDirect(scene, "Input Notes", {-20.f, 12.f, 0.2f});
    firstLabel->setData("Input Notes");
    firstLabel->setSize(16);

    auto secondLabel = addTextComponentDirect(scene, "Output Notes", {28.f, -8.f, 0.2f});
    secondLabel->setData("Output Notes");
    secondLabel->setSize(20);

    scene->getState().attachChild(group->getUuid(), firstLabel->getUuid(), false);
    scene->getState().attachChild(group->getUuid(), secondLabel->getUuid(), false);

    const auto clones = group->clone(scene->getState());
    ASSERT_EQ(clones.size(), 3u);

    const auto clonedGroup = std::dynamic_pointer_cast<Bess::Canvas::GroupSceneComponent>(clones.front());
    ASSERT_NE(clonedGroup, nullptr);
    EXPECT_NE(clonedGroup->getUuid(), group->getUuid());
    EXPECT_EQ(clonedGroup->getName(), group->getName());
    EXPECT_EQ(clonedGroup->getChildComponents().size(), 2u);

    std::unordered_set<Bess::UUID> clonedChildIds;
    for (size_t i = 1; i < clones.size(); ++i) {
        const auto clonedText = std::dynamic_pointer_cast<Bess::Canvas::TextComponent>(clones[i]);
        ASSERT_NE(clonedText, nullptr);
        EXPECT_EQ(clonedText->getParentComponent(), clonedGroup->getUuid());
        EXPECT_NE(clonedText->getUuid(), firstLabel->getUuid());
        EXPECT_NE(clonedText->getUuid(), secondLabel->getUuid());
        clonedChildIds.insert(clonedText->getUuid());
        EXPECT_TRUE(clonedText->getData() == firstLabel->getData() ||
                    clonedText->getData() == secondLabel->getData());
    }

    EXPECT_EQ(clonedGroup->getChildComponents(), clonedChildIds);
}
