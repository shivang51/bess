#include "test_scene_graph_fixture.h"

using namespace Bess::Tests;

class CopyPasteTest : public SceneGraphTestBase {};

TEST_F(CopyPasteTest, CopyPasteWithEmptySelectionProducesNoChanges) {
    auto targetScene = std::make_shared<Bess::Canvas::Scene>();

    copyPaste->copy(scene);
    const auto clonedIds = copyPaste->paste(targetScene, false);

    EXPECT_TRUE(clonedIds.empty());
    EXPECT_TRUE(targetScene->getState().getAllComponents().empty());
}

TEST_F(CopyPasteTest, CopyPasteClonesSelectedGraphAndRemapsConnectionEndpoints) {
    const auto source = addSimComponentDirect(scene, inputDef);
    const auto sink = addSimComponentDirect(scene, outputDef);
    auto connection = connectSlots(source.firstOutput(), sink.firstInput());

    auto &sourceState = scene->getState();
    sourceState.addSelectedComponent(source.comp->getUuid());
    sourceState.addSelectedComponent(sink.comp->getUuid());
    sourceState.addSelectedComponent(connection->getUuid());

    auto targetScene = std::make_shared<Bess::Canvas::Scene>();
    copyPaste->copy(scene);
    const auto clonedIds = copyPaste->paste(targetScene, false);

    ASSERT_TRUE(clonedIds.contains(source.comp->getUuid()));
    ASSERT_TRUE(clonedIds.contains(sink.comp->getUuid()));
    ASSERT_TRUE(clonedIds.contains(source.firstOutput()->getUuid()));
    ASSERT_TRUE(clonedIds.contains(sink.firstInput()->getUuid()));
    ASSERT_TRUE(clonedIds.contains(connection->getUuid()));

    const auto clonedConnection = targetScene->getState().getComponentByUuid<Bess::Canvas::ConnectionSceneComponent>(
        clonedIds.at(connection->getUuid()));
    ASSERT_NE(clonedConnection, nullptr);
    EXPECT_EQ(clonedConnection->getStartSlot(), clonedIds.at(source.firstOutput()->getUuid()));
    EXPECT_EQ(clonedConnection->getEndSlot(), clonedIds.at(sink.firstInput()->getUuid()));
    EXPECT_EQ(targetScene->getState().getRootComponents().size(), 3u);
    EXPECT_EQ(targetScene->getState().getAllComponents().size(), scene->getState().getAllComponents().size());
}

TEST_F(CopyPasteTest, CopyPasteSkipsUnselectedConnectionAndOtherEndpoint) {
    const auto source = addSimComponentDirect(scene, inputDef);
    const auto sink = addSimComponentDirect(scene, outputDef);
    auto connection = connectSlots(source.firstOutput(), sink.firstInput());

    scene->getState().addSelectedComponent(source.comp->getUuid());

    auto targetScene = std::make_shared<Bess::Canvas::Scene>();
    copyPaste->copy(scene);
    const auto clonedIds = copyPaste->paste(targetScene, false);

    EXPECT_TRUE(clonedIds.contains(source.comp->getUuid()));
    EXPECT_FALSE(clonedIds.contains(sink.comp->getUuid()));
    EXPECT_FALSE(clonedIds.contains(connection->getUuid()));
    EXPECT_EQ(targetScene->getState().getRootComponents().size(), 1u);
}

TEST_F(CopyPasteTest, CopyOnlyConnectionWithoutEndpointsPastesNothing) {
    const auto source = addSimComponentDirect(scene, inputDef);
    const auto sink = addSimComponentDirect(scene, outputDef);
    auto connection = connectSlots(source.firstOutput(), sink.firstInput());

    scene->getState().addSelectedComponent(connection->getUuid());

    auto targetScene = std::make_shared<Bess::Canvas::Scene>();
    copyPaste->copy(scene);
    const auto clonedIds = copyPaste->paste(targetScene, false);

    EXPECT_TRUE(clonedIds.empty());
    EXPECT_TRUE(targetScene->getState().getAllComponents().empty());
}

TEST_F(CopyPasteTest, RepeatedPasteCreatesDistinctClones) {
    const auto source = addSimComponentDirect(scene, inputDef);
    scene->getState().addSelectedComponent(source.comp->getUuid());

    auto targetScene = std::make_shared<Bess::Canvas::Scene>();
    copyPaste->copy(scene);

    const auto firstPaste = copyPaste->paste(targetScene, false);
    const auto secondPaste = copyPaste->paste(targetScene, false);

    ASSERT_TRUE(firstPaste.contains(source.comp->getUuid()));
    ASSERT_TRUE(secondPaste.contains(source.comp->getUuid()));
    EXPECT_NE(firstPaste.at(source.comp->getUuid()), secondPaste.at(source.comp->getUuid()));
    EXPECT_EQ(targetScene->getState().getRootComponents().size(), 2u);
}
