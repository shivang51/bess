#include "pages/main_page/scene_components/group_scene_component.h"
#include "scene/scene_serializer.h"
#include "test_scene_graph_fixture.h"
#include <unordered_set>

using namespace Bess::Tests;

class SceneSerializationTest : public SceneGraphTestBase {};

TEST_F(SceneSerializationTest, RoundTripPreservesMixedSceneGraphMetadataAndHierarchy) {
    const auto inputA = addSimComponentDirect(scene, inputDef);
    const auto inputB = addSimComponentDirect(scene, inputDef);
    const auto andGate = addSimComponentDirect(scene, andDef);
    const auto output = addSimComponentDirect(scene, outputDef);

    auto connA = connectSlots(inputA.firstOutput(), andGate.firstInput());
    auto connB = connectSlots(inputB.firstOutput(), andGate.inputs.at(1));
    auto connC = connectSlots(andGate.firstOutput(), output.firstInput());

    auto group = Bess::Canvas::GroupSceneComponent::create("Annotations");
    scene->getState().addComponent(group);

    auto label = addTextComponentDirect(scene, "Status label", {32.f, -18.f, 0.25f});
    label->setData("Status label");
    label->setSize(18);
    label->setForegroundColor({0.25f, 0.75f, 0.5f, 1.f});
    scene->getState().attachChild(group->getUuid(), label->getUuid(), false);

    auto &sourceState = scene->getState();
    sourceState.setIsRootScene(false);
    sourceState.setModuleId(Bess::UUID{});
    sourceState.addSelectedComponent(inputA.comp->getUuid());
    sourceState.addSelectedComponent(connA->getUuid());

    Bess::SceneSerializer serializer;
    Json::Value json;
    serializer.serialize(json, scene);

    auto roundTrippedScene = std::make_shared<Bess::Canvas::Scene>();
    serializer.deserialize(json, roundTrippedScene);

    auto &loadedState = roundTrippedScene->getState();
    EXPECT_EQ(loadedState.getAllComponents().size(), sourceState.getAllComponents().size());
    EXPECT_EQ(loadedState.getRootComponents(), sourceState.getRootComponents());
    EXPECT_EQ(loadedState.getSceneId(), sourceState.getSceneId());
    EXPECT_EQ(loadedState.getModuleId(), sourceState.getModuleId());
    EXPECT_EQ(loadedState.getIsRootScene(), sourceState.getIsRootScene());
    EXPECT_TRUE(loadedState.getSelectedComponents().empty());

    const auto loadedConnA = loadedState.getComponentByUuid<Bess::Canvas::ConnectionSceneComponent>(connA->getUuid());
    const auto loadedConnB = loadedState.getComponentByUuid<Bess::Canvas::ConnectionSceneComponent>(connB->getUuid());
    const auto loadedConnC = loadedState.getComponentByUuid<Bess::Canvas::ConnectionSceneComponent>(connC->getUuid());
    const auto loadedText = loadedState.getComponentByUuid<Bess::Canvas::TextComponent>(label->getUuid());
    const auto loadedGroup = loadedState.getComponentByUuid<Bess::Canvas::GroupSceneComponent>(group->getUuid());

    ASSERT_NE(loadedConnA, nullptr);
    ASSERT_NE(loadedConnB, nullptr);
    ASSERT_NE(loadedConnC, nullptr);
    ASSERT_NE(loadedText, nullptr);
    ASSERT_NE(loadedGroup, nullptr);

    EXPECT_EQ(loadedConnA->getStartSlot(), inputA.firstOutput()->getUuid());
    EXPECT_EQ(loadedConnA->getEndSlot(), andGate.firstInput()->getUuid());
    EXPECT_EQ(loadedConnB->getStartSlot(), inputB.firstOutput()->getUuid());
    EXPECT_EQ(loadedConnB->getEndSlot(), andGate.inputs.at(1)->getUuid());
    EXPECT_EQ(loadedConnC->getStartSlot(), andGate.firstOutput()->getUuid());
    EXPECT_EQ(loadedConnC->getEndSlot(), output.firstInput()->getUuid());

    EXPECT_EQ(loadedText->getData(), label->getData());
    EXPECT_EQ(loadedText->getSize(), label->getSize());
    EXPECT_EQ(loadedText->getForegroundColor(), label->getForegroundColor());
    EXPECT_EQ(loadedText->getParentComponent(), loadedGroup->getUuid());
    EXPECT_TRUE(loadedGroup->getChildComponents().contains(loadedText->getUuid()));

    std::unordered_set<uint32_t> runtimeIds;
    for (const auto &[uuid, component] : loadedState.getAllComponents()) {
        EXPECT_NE(component->getRuntimeId(), Bess::Canvas::PickingId::invalidRuntimeId);
        runtimeIds.insert(component->getRuntimeId());
        EXPECT_EQ(loadedState.getComponentByPickingId(Bess::Canvas::PickingId{component->getRuntimeId(), 0}), component);
    }
    EXPECT_EQ(runtimeIds.size(), loadedState.getAllComponents().size());

    Json::Value secondJson;
    serializer.serialize(secondJson, roundTrippedScene);
    auto secondRoundTrip = std::make_shared<Bess::Canvas::Scene>();
    serializer.deserialize(secondJson, secondRoundTrip);

    EXPECT_EQ(secondRoundTrip->getState().getAllComponents().size(), loadedState.getAllComponents().size());
    EXPECT_EQ(secondRoundTrip->getState().getRootComponents(), loadedState.getRootComponents());
    EXPECT_NE(secondRoundTrip->getState().getComponentByUuid<Bess::Canvas::TextComponent>(label->getUuid()), nullptr);
    EXPECT_NE(secondRoundTrip->getState().getComponentByUuid<Bess::Canvas::GroupSceneComponent>(group->getUuid()), nullptr);
}

TEST_F(SceneSerializationTest, DeserializeSkipsMalformedUnknownAndDuplicateEntries) {
    const auto source = addSimComponentDirect(scene, inputDef);
    auto text = addTextComponentDirect(scene, "Legend", {-12.f, 22.f, 0.1f});
    text->setData("Legend");
    text->setSize(14);

    Bess::SceneSerializer serializer;
    Json::Value json;
    serializer.serialize(json, scene);

    const auto originalCount = scene->getState().getAllComponents().size();

    auto duplicate = json["scene_state"]["components"][0];
    duplicate["name"] = "Duplicate Should Be Ignored";
    json["scene_state"]["components"].append(duplicate);

    json["scene_state"]["components"].append(Json::objectValue);

    Json::Value unknown = Json::objectValue;
    unknown["typeName"] = "UnknownSceneComponent";
    unknown["uuid"] = static_cast<Json::UInt64>(Bess::UUID{});
    json["scene_state"]["components"].append(unknown);

    auto loadedScene = std::make_shared<Bess::Canvas::Scene>();
    serializer.deserialize(json, loadedScene);

    const auto &loadedState = loadedScene->getState();
    EXPECT_EQ(loadedState.getAllComponents().size(), originalCount);
    EXPECT_NE(loadedState.getComponentByUuid(source.comp->getUuid()), nullptr);

    const auto loadedText = loadedState.getComponentByUuid<Bess::Canvas::TextComponent>(text->getUuid());
    ASSERT_NE(loadedText, nullptr);
    EXPECT_EQ(loadedText->getName(), text->getName());
    EXPECT_EQ(loadedText->getData(), text->getData());
}
