#include "macro_command.h"
#include "pages/main_page/cmds/delete_comp_cmd.h"
#include "test_scene_graph_fixture.h"
#include <array>

using namespace Bess::Tests;

class CommandSystemTest : public SceneGraphTestBase {};

TEST_F(CommandSystemTest, UndoRedoWithoutCommandsIsSafe) {
    EXPECT_FALSE(cmdSystem.canUndo());
    EXPECT_FALSE(cmdSystem.canRedo());

    cmdSystem.undo();
    cmdSystem.redo();

    EXPECT_FALSE(cmdSystem.canUndo());
    EXPECT_FALSE(cmdSystem.canRedo());
    EXPECT_TRUE(scene->getState().getAllComponents().empty());
}

TEST_F(CommandSystemTest, UndoRedoRestoresAddedSimulationComponentWithChildren) {
    const auto fixture = executeAddSimComponent(inputDef);

    EXPECT_TRUE(cmdSystem.canUndo());
    EXPECT_EQ(scene->getState().getRootComponents().size(), 1u);
    EXPECT_EQ(scene->getState().getAllComponents().size(), 3u);

    cmdSystem.undo();
    EXPECT_TRUE(scene->getState().getAllComponents().empty());
    EXPECT_TRUE(cmdSystem.canRedo());

    cmdSystem.redo();
    EXPECT_NE(scene->getState().getComponentByUuid(fixture.comp->getUuid()), nullptr);
    EXPECT_NE(scene->getState().getComponentByUuid(fixture.firstOutput()->getUuid()), nullptr);
    EXPECT_EQ(scene->getState().getAllComponents().size(), 3u);
}

TEST_F(CommandSystemTest, ExecutingNewCommandClearsRedoStack) {
    const auto first = executeAddSimComponent(inputDef);

    cmdSystem.undo();
    ASSERT_TRUE(cmdSystem.canRedo());

    const auto second = executeAddSimComponent(outputDef);

    EXPECT_FALSE(cmdSystem.canRedo());
    EXPECT_NE(scene->getState().getComponentByUuid(second.comp->getUuid()), nullptr);
    EXPECT_EQ(scene->getState().getComponentByUuid(first.comp->getUuid()), nullptr);
}

TEST_F(CommandSystemTest, DeleteCommandUndoRedoRestoresStandaloneComponentTree) {
    const auto fixture = executeAddSimComponent(inputDef);

    cmdSystem.execute(std::make_unique<Bess::Cmd::DeleteCompCmd>(
        std::vector<Bess::UUID>{fixture.comp->getUuid()}));

    EXPECT_TRUE(scene->getState().getAllComponents().empty());

    cmdSystem.undo();
    EXPECT_NE(scene->getState().getComponentByUuid(fixture.comp->getUuid()), nullptr);
    EXPECT_NE(scene->getState().getComponentByUuid(fixture.firstOutput()->getUuid()), nullptr);
    EXPECT_EQ(scene->getState().getAllComponents().size(), 3u);

    cmdSystem.redo();
    EXPECT_TRUE(scene->getState().getAllComponents().empty());
}

TEST_F(CommandSystemTest, DeleteCommandWithUnknownIdDoesNotCreateUndoHistory) {
    cmdSystem.execute(std::make_unique<Bess::Cmd::DeleteCompCmd>(
        std::vector<Bess::UUID>{Bess::UUID{123456789}}));

    EXPECT_FALSE(cmdSystem.canUndo());
    EXPECT_FALSE(cmdSystem.canRedo());
    EXPECT_TRUE(scene->getState().getAllComponents().empty());
}

TEST_F(CommandSystemTest, MacroCommandUndoRedoPreservesMultiStepSceneChanges) {
    auto createdA = Bess::Canvas::SimulationSceneComponent::createNewAndRegister(inputDef);
    auto createdB = Bess::Canvas::SimulationSceneComponent::createNewAndRegister(outputDef);

    auto compA = std::dynamic_pointer_cast<Bess::Canvas::SimulationSceneComponent>(createdA.front());
    auto compB = std::dynamic_pointer_cast<Bess::Canvas::SimulationSceneComponent>(createdB.front());

    std::vector<std::shared_ptr<Bess::Canvas::SceneComponent>> childrenA(createdA.begin() + 1, createdA.end());
    std::vector<std::shared_ptr<Bess::Canvas::SceneComponent>> childrenB(createdB.begin() + 1, createdB.end());

    auto macro = std::make_unique<Bess::Cmd::MacroCommand>();
    macro->addCommand(std::make_unique<Bess::Cmd::AddCompCmd<Bess::Canvas::SimulationSceneComponent>>(compA, childrenA));
    macro->addCommand(std::make_unique<Bess::Cmd::AddCompCmd<Bess::Canvas::SimulationSceneComponent>>(compB, childrenB));

    cmdSystem.execute(std::move(macro));

    EXPECT_EQ(scene->getState().getRootComponents().size(), 2u);
    EXPECT_TRUE(cmdSystem.canUndo());

    cmdSystem.undo();
    EXPECT_TRUE(scene->getState().getAllComponents().empty());

    cmdSystem.redo();
    EXPECT_NE(scene->getState().getComponentByUuid(compA->getUuid()), nullptr);
    EXPECT_NE(scene->getState().getComponentByUuid(compB->getUuid()), nullptr);
}

TEST_F(CommandSystemTest, MacroCommandWithNoOpsStillProducesUndoableEntryWithoutSceneMutation) {
    cmdSystem.execute(std::make_unique<Bess::Cmd::MacroCommand>());

    EXPECT_TRUE(cmdSystem.canUndo());
    EXPECT_TRUE(scene->getState().getAllComponents().empty());

    cmdSystem.undo();
    EXPECT_TRUE(cmdSystem.canRedo());
    EXPECT_TRUE(scene->getState().getAllComponents().empty());

    cmdSystem.redo();
    EXPECT_TRUE(scene->getState().getAllComponents().empty());
}

TEST_F(CommandSystemTest, RepeatedUndoRedoCyclesKeepMacroAddedSceneStable) {
    auto createdA = Bess::Canvas::SimulationSceneComponent::createNewAndRegister(inputDef);
    auto createdB = Bess::Canvas::SimulationSceneComponent::createNewAndRegister(andDef);
    auto createdC = Bess::Canvas::SimulationSceneComponent::createNewAndRegister(outputDef);

    auto compA = std::dynamic_pointer_cast<Bess::Canvas::SimulationSceneComponent>(createdA.front());
    auto compB = std::dynamic_pointer_cast<Bess::Canvas::SimulationSceneComponent>(createdB.front());
    auto compC = std::dynamic_pointer_cast<Bess::Canvas::SimulationSceneComponent>(createdC.front());

    std::vector<std::shared_ptr<Bess::Canvas::SceneComponent>> childrenA(createdA.begin() + 1, createdA.end());
    std::vector<std::shared_ptr<Bess::Canvas::SceneComponent>> childrenB(createdB.begin() + 1, createdB.end());
    std::vector<std::shared_ptr<Bess::Canvas::SceneComponent>> childrenC(createdC.begin() + 1, createdC.end());

    auto macro = std::make_unique<Bess::Cmd::MacroCommand>();
    macro->addCommand(std::make_unique<Bess::Cmd::AddCompCmd<Bess::Canvas::SimulationSceneComponent>>(compA, childrenA));
    macro->addCommand(std::make_unique<Bess::Cmd::AddCompCmd<Bess::Canvas::SimulationSceneComponent>>(compB, childrenB));
    macro->addCommand(std::make_unique<Bess::Cmd::AddCompCmd<Bess::Canvas::SimulationSceneComponent>>(compC, childrenC));

    cmdSystem.execute(std::move(macro));

    for (int i = 0; i < 4; ++i) {
        EXPECT_EQ(scene->getState().getRootComponents().size(), 3u);
        EXPECT_EQ(scene->getState().getAllComponents().size(), 11u);

        cmdSystem.undo();
        EXPECT_TRUE(scene->getState().getAllComponents().empty());
        EXPECT_TRUE(cmdSystem.canRedo());

        cmdSystem.redo();
        EXPECT_NE(scene->getState().getComponentByUuid(compA->getUuid()), nullptr);
        EXPECT_NE(scene->getState().getComponentByUuid(compB->getUuid()), nullptr);
        EXPECT_NE(scene->getState().getComponentByUuid(compC->getUuid()), nullptr);
    }
}

TEST_F(CommandSystemTest, LongLinearHistoryCanBeFullyUndoneAndReplayed) {
    std::vector<Bess::UUID> componentIds;
    std::vector<size_t> componentSizes;
    const std::array<std::shared_ptr<Bess::SimEngine::ComponentDefinition>, 3> defs = {
        inputDef,
        andDef,
        outputDef,
    };

    size_t expectedTotalComponents = 0;
    for (int i = 0; i < 9; ++i) {
        const auto fixture = executeAddSimComponent(defs[i % defs.size()]);
        componentIds.push_back(fixture.comp->getUuid());
        componentSizes.push_back(1u + fixture.inputs.size() + fixture.outputs.size());
        expectedTotalComponents += componentSizes.back();
    }

    EXPECT_EQ(scene->getState().getRootComponents().size(), componentIds.size());
    EXPECT_EQ(scene->getState().getAllComponents().size(), expectedTotalComponents);

    for (size_t i = componentIds.size(); i > 0; --i) {
        ASSERT_TRUE(cmdSystem.canUndo());
        cmdSystem.undo();
    }

    EXPECT_TRUE(scene->getState().getAllComponents().empty());
    EXPECT_TRUE(cmdSystem.canRedo());

    for (size_t i = 0; i < componentIds.size(); ++i) {
        ASSERT_TRUE(cmdSystem.canRedo());
        cmdSystem.redo();
    }

    EXPECT_EQ(scene->getState().getRootComponents().size(), componentIds.size());
    EXPECT_EQ(scene->getState().getAllComponents().size(), expectedTotalComponents);
    for (const auto &componentId : componentIds) {
        EXPECT_NE(scene->getState().getComponentByUuid(componentId), nullptr);
    }
}

TEST_F(CommandSystemTest, InterleavedUndoRedoFollowsExpectedTimelineAcrossCommands) {
    const auto first = executeAddSimComponent(inputDef);
    const auto second = executeAddSimComponent(outputDef);
    const auto third = executeAddSimComponent(andDef);

    EXPECT_EQ(scene->getState().getRootComponents().size(), 3u);

    cmdSystem.undo();
    EXPECT_EQ(scene->getState().getComponentByUuid(third.comp->getUuid()), nullptr);
    EXPECT_NE(scene->getState().getComponentByUuid(second.comp->getUuid()), nullptr);

    cmdSystem.undo();
    EXPECT_EQ(scene->getState().getComponentByUuid(second.comp->getUuid()), nullptr);
    EXPECT_NE(scene->getState().getComponentByUuid(first.comp->getUuid()), nullptr);

    cmdSystem.redo();
    EXPECT_NE(scene->getState().getComponentByUuid(second.comp->getUuid()), nullptr);
    EXPECT_EQ(scene->getState().getComponentByUuid(third.comp->getUuid()), nullptr);

    const auto replacement = executeAddSimComponent(notDef);

    EXPECT_FALSE(cmdSystem.canRedo());
    EXPECT_NE(scene->getState().getComponentByUuid(first.comp->getUuid()), nullptr);
    EXPECT_NE(scene->getState().getComponentByUuid(second.comp->getUuid()), nullptr);
    EXPECT_NE(scene->getState().getComponentByUuid(replacement.comp->getUuid()), nullptr);
    EXPECT_EQ(scene->getState().getComponentByUuid(third.comp->getUuid()), nullptr);
    EXPECT_EQ(scene->getState().getRootComponents().size(), 3u);
}

TEST_F(CommandSystemTest, UndoRedoRestoresNonSimulationAnnotationComponent) {
    auto text = std::make_shared<Bess::Canvas::TextComponent>();
    text->setName("Checklist");
    text->setData("Checklist");
    text->setSize(22);
    text->setForegroundColor({0.8f, 0.9f, 0.4f, 1.f});

    cmdSystem.execute(std::make_unique<Bess::Cmd::AddCompCmd<Bess::Canvas::NonSimSceneComponent>>(text));

    ASSERT_NE(scene->getState().getComponentByUuid(text->getUuid()), nullptr);
    EXPECT_EQ(scene->getState().getRootComponents().size(), 1u);

    cmdSystem.undo();
    EXPECT_TRUE(scene->getState().getAllComponents().empty());

    cmdSystem.redo();
    const auto restored = scene->getState().getComponentByUuid<Bess::Canvas::TextComponent>(text->getUuid());
    ASSERT_NE(restored, nullptr);
    EXPECT_EQ(restored->getData(), text->getData());
    EXPECT_EQ(restored->getSize(), text->getSize());
    EXPECT_EQ(restored->getForegroundColor(), text->getForegroundColor());
}
