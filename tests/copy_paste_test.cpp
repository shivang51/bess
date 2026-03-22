#include "test_scene_graph_fixture.h"
#include <unordered_set>

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

TEST_F(CopyPasteTest, CopyPastePreservesSelectedMultiStageGraphTopology) {
    const auto inputA = addSimComponentDirect(scene, inputDef);
    const auto inputB = addSimComponentDirect(scene, inputDef);
    const auto andGate = addSimComponentDirect(scene, andDef);
    const auto notGate = addSimComponentDirect(scene, notDef);
    const auto output = addSimComponentDirect(scene, outputDef);

    auto connA = connectSlots(inputA.firstOutput(), andGate.firstInput());
    auto connB = connectSlots(inputB.firstOutput(), andGate.inputs.at(1));
    auto connC = connectSlots(andGate.firstOutput(), notGate.firstInput());
    auto connD = connectSlots(notGate.firstOutput(), output.firstInput());

    auto &state = scene->getState();
    for (const auto &id : {
             inputA.comp->getUuid(),
             inputB.comp->getUuid(),
             andGate.comp->getUuid(),
             notGate.comp->getUuid(),
             output.comp->getUuid(),
             connA->getUuid(),
             connB->getUuid(),
             connC->getUuid(),
             connD->getUuid(),
         }) {
        state.addSelectedComponent(id);
    }

    auto targetScene = std::make_shared<Bess::Canvas::Scene>();
    copyPaste->copy(scene);
    const auto clonedIds = copyPaste->paste(targetScene, false);

    EXPECT_EQ(targetScene->getState().getRootComponents().size(), 9u);
    EXPECT_EQ(targetScene->getState().getAllComponents().size(), scene->getState().getAllComponents().size());
    EXPECT_EQ(clonedIds.size(), scene->getState().getAllComponents().size());

    const auto clonedConnA = targetScene->getState().getComponentByUuid<Bess::Canvas::ConnectionSceneComponent>(
        clonedIds.at(connA->getUuid()));
    const auto clonedConnB = targetScene->getState().getComponentByUuid<Bess::Canvas::ConnectionSceneComponent>(
        clonedIds.at(connB->getUuid()));
    const auto clonedConnC = targetScene->getState().getComponentByUuid<Bess::Canvas::ConnectionSceneComponent>(
        clonedIds.at(connC->getUuid()));
    const auto clonedConnD = targetScene->getState().getComponentByUuid<Bess::Canvas::ConnectionSceneComponent>(
        clonedIds.at(connD->getUuid()));

    ASSERT_NE(clonedConnA, nullptr);
    ASSERT_NE(clonedConnB, nullptr);
    ASSERT_NE(clonedConnC, nullptr);
    ASSERT_NE(clonedConnD, nullptr);

    EXPECT_EQ(clonedConnA->getStartSlot(), clonedIds.at(inputA.firstOutput()->getUuid()));
    EXPECT_EQ(clonedConnA->getEndSlot(), clonedIds.at(andGate.firstInput()->getUuid()));
    EXPECT_EQ(clonedConnB->getStartSlot(), clonedIds.at(inputB.firstOutput()->getUuid()));
    EXPECT_EQ(clonedConnB->getEndSlot(), clonedIds.at(andGate.inputs.at(1)->getUuid()));
    EXPECT_EQ(clonedConnC->getStartSlot(), clonedIds.at(andGate.firstOutput()->getUuid()));
    EXPECT_EQ(clonedConnC->getEndSlot(), clonedIds.at(notGate.firstInput()->getUuid()));
    EXPECT_EQ(clonedConnD->getStartSlot(), clonedIds.at(notGate.firstOutput()->getUuid()));
    EXPECT_EQ(clonedConnD->getEndSlot(), clonedIds.at(output.firstInput()->getUuid()));
}

TEST_F(CopyPasteTest, CopyPastePartialSelectionKeepsOnlyInternalConnections) {
    const auto inputA = addSimComponentDirect(scene, inputDef);
    const auto inputB = addSimComponentDirect(scene, inputDef);
    const auto orGate = addSimComponentDirect(scene, orDef);
    const auto output = addSimComponentDirect(scene, outputDef);

    auto selectedConn = connectSlots(inputA.firstOutput(), orGate.firstInput());
    auto skippedInputConn = connectSlots(inputB.firstOutput(), orGate.inputs.at(1));
    auto skippedOutputConn = connectSlots(orGate.firstOutput(), output.firstInput());

    auto &state = scene->getState();
    state.addSelectedComponent(inputA.comp->getUuid());
    state.addSelectedComponent(orGate.comp->getUuid());
    state.addSelectedComponent(selectedConn->getUuid());

    auto targetScene = std::make_shared<Bess::Canvas::Scene>();
    copyPaste->copy(scene);
    const auto clonedIds = copyPaste->paste(targetScene, false);

    ASSERT_TRUE(clonedIds.contains(inputA.comp->getUuid()));
    ASSERT_TRUE(clonedIds.contains(orGate.comp->getUuid()));
    ASSERT_TRUE(clonedIds.contains(selectedConn->getUuid()));
    EXPECT_FALSE(clonedIds.contains(inputB.comp->getUuid()));
    EXPECT_FALSE(clonedIds.contains(output.comp->getUuid()));
    EXPECT_FALSE(clonedIds.contains(skippedInputConn->getUuid()));
    EXPECT_FALSE(clonedIds.contains(skippedOutputConn->getUuid()));

    const auto clonedConnection = targetScene->getState().getComponentByUuid<Bess::Canvas::ConnectionSceneComponent>(
        clonedIds.at(selectedConn->getUuid()));
    ASSERT_NE(clonedConnection, nullptr);
    EXPECT_EQ(clonedConnection->getStartSlot(), clonedIds.at(inputA.firstOutput()->getUuid()));
    EXPECT_EQ(clonedConnection->getEndSlot(), clonedIds.at(orGate.firstInput()->getUuid()));
    EXPECT_EQ(targetScene->getState().getRootComponents().size(), 3u);
}

TEST_F(CopyPasteTest, RepeatedPasteOfConnectedGraphScalesLinearlyWithoutReusingIds) {
    const auto input = addSimComponentDirect(scene, inputDef);
    const auto gate = addSimComponentDirect(scene, notDef);
    const auto output = addSimComponentDirect(scene, outputDef);

    auto connA = connectSlots(input.firstOutput(), gate.firstInput());
    auto connB = connectSlots(gate.firstOutput(), output.firstInput());

    auto &state = scene->getState();
    for (const auto &id : {
             input.comp->getUuid(),
             gate.comp->getUuid(),
             output.comp->getUuid(),
             connA->getUuid(),
             connB->getUuid(),
         }) {
        state.addSelectedComponent(id);
    }

    auto targetScene = std::make_shared<Bess::Canvas::Scene>();
    copyPaste->copy(scene);

    std::unordered_set<Bess::UUID> pastedInputIds;
    std::unordered_set<Bess::UUID> pastedGateIds;
    std::unordered_set<Bess::UUID> pastedOutputIds;
    for (int i = 0; i < 5; ++i) {
        const auto clonedIds = copyPaste->paste(targetScene, false);
        ASSERT_EQ(clonedIds.size(), scene->getState().getAllComponents().size());
        pastedInputIds.insert(clonedIds.at(input.comp->getUuid()));
        pastedGateIds.insert(clonedIds.at(gate.comp->getUuid()));
        pastedOutputIds.insert(clonedIds.at(output.comp->getUuid()));
    }

    EXPECT_EQ(pastedInputIds.size(), 5u);
    EXPECT_EQ(pastedGateIds.size(), 5u);
    EXPECT_EQ(pastedOutputIds.size(), 5u);
    EXPECT_EQ(targetScene->getState().getRootComponents().size(), 25u);
    EXPECT_EQ(targetScene->getState().getAllComponents().size(), scene->getState().getAllComponents().size() * 5u);
}

TEST_F(CopyPasteTest, CopyPasteNonSimulationTextPreservesContentAndStyle) {
    auto text = addTextComponentDirect(scene, "Build Notes", {48.f, -26.f, 0.4f});
    text->setData("Build Notes");
    text->setSize(24);
    text->setForegroundColor({0.3f, 0.8f, 0.4f, 1.f});
    auto style = text->getStyle();
    style.color = {0.15f, 0.15f, 0.15f, 0.9f};
    style.borderColor = {0.7f, 0.7f, 0.7f, 1.f};
    style.borderRadius = {6.f, 6.f, 6.f, 6.f};
    style.borderSize = {1.f, 1.f, 1.f, 1.f};
    text->setStyle(style);

    scene->getState().addSelectedComponent(text->getUuid());

    auto targetScene = std::make_shared<Bess::Canvas::Scene>();
    copyPaste->copy(scene);
    const auto clonedIds = copyPaste->paste(targetScene, false);

    ASSERT_TRUE(clonedIds.contains(text->getUuid()));
    const auto clonedText = targetScene->getState().getComponentByUuid<Bess::Canvas::TextComponent>(
        clonedIds.at(text->getUuid()));
    ASSERT_NE(clonedText, nullptr);
    EXPECT_EQ(clonedText->getData(), text->getData());
    EXPECT_EQ(clonedText->getName(), text->getName());
    EXPECT_EQ(clonedText->getSize(), text->getSize());
    EXPECT_EQ(clonedText->getForegroundColor(), text->getForegroundColor());
    EXPECT_EQ(clonedText->getStyle().color, text->getStyle().color);
    EXPECT_EQ(clonedText->getStyle().borderColor, text->getStyle().borderColor);
    EXPECT_EQ(targetScene->getState().getRootComponents().size(), 1u);
}
