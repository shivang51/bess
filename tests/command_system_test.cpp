#include "macro_command.h"
#include "pages/main_page/cmds/delete_comp_cmd.h"
#include "test_scene_graph_fixture.h"

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
