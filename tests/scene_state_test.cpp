#include "application/events/application_event.h"
#include "event_dispatcher.h"
#include "pages/main_page/scene_components/sim_scene_component.h"
#include "scene/scene_events.h"
#include "scene/scene_state/scene_state.h"
#include "scene_state/components/scene_component.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"

using namespace Bess::Canvas;
using Bess::UUID;

// Helper to generate unique UUIDs for tests
static UUID genUUID() {
    static uint64_t counter = 1000; // Start high to avoid collision with potential defaults
    return UUID(counter++);
}

// Concrete implementation for testing
class TestSceneComponent : public SceneComponent {
  public:
    TestSceneComponent(UUID id) {
        m_uuid = id;
    }

    SceneComponentType getType() const override { return SceneComponentType::_base; }
};

class SceneStateTest : public testing::Test {
  protected:
    SceneState state;

    void SetUp() override {
        state.clear();
        Bess::EventSystem::EventDispatcher::instance().clear();
    }

    void TearDown() override {
        state.clear();
        Bess::EventSystem::EventDispatcher::instance().clear();
    }

    std::shared_ptr<TestSceneComponent> createComponent(UUID id = UUID::null) {
        if (id == UUID::null)
            id = genUUID();
        return std::make_shared<TestSceneComponent>(id);
    }
};

TEST_F(SceneStateTest, AddComponent) {
    UUID id = genUUID();
    auto comp = createComponent(id);

    bool eventReceived = false;
    // Listen for ComponentAddedEvent
    // Event type dispatching is handled by EventDispatcher
    auto sink = Bess::EventSystem::EventDispatcher::instance().sink<Events::ComponentAddedEvent>();
    sink.connect([&](const Events::ComponentAddedEvent &e) {
        if (e.uuid == id)
            eventReceived = true;
    });

    state.addComponent<TestSceneComponent>(comp);
    Bess::EventSystem::EventDispatcher::instance().dispatchAll();

    EXPECT_TRUE(state.getComponentByUuid(id) != nullptr);
    EXPECT_EQ(state.getComponentByUuid(id), comp);
    EXPECT_TRUE(eventReceived);
    EXPECT_TRUE(state.getRootComponents().contains(id));
}

TEST_F(SceneStateTest, RemoveComponent) {
    UUID id = genUUID();
    auto comp = createComponent(id);
    state.addComponent<TestSceneComponent>(comp);

    auto removed = state.removeComponent(id);

    EXPECT_EQ(removed.size(), 1);
    EXPECT_EQ(removed[0], id);
    EXPECT_TRUE(state.getComponentByUuid(id) == nullptr);
    EXPECT_FALSE(state.getRootComponents().contains(id));
}

TEST_F(SceneStateTest, RemoveComponentRecursive) {
    // Parent -> Child structure setup manually for test since SceneState manages components
    // SceneState logic for parent/child depends on components having parent/child links set

    auto parent = createComponent();
    auto child = createComponent();

    parent->addChildComponent(child->getUuid());
    child->setParentComponent(parent->getUuid());

    state.addComponent<TestSceneComponent>(parent);
    state.addComponent<TestSceneComponent>(child);

    // Child should ideally NOT be in root components if it has a parent
    // But addComponent checks if parent is null to add to root.
    // Since child has parent set, it shouldn't be in root.
    EXPECT_TRUE(state.getRootComponents().contains(parent->getUuid()));
    EXPECT_FALSE(state.getRootComponents().contains(child->getUuid()));

    auto removed = state.removeComponent(parent->getUuid());

    // Should remove both
    EXPECT_EQ(removed.size(), 2);
    EXPECT_TRUE(state.getComponentByUuid(parent->getUuid()) == nullptr);
    EXPECT_TRUE(state.getComponentByUuid(child->getUuid()) == nullptr);
}

TEST_F(SceneStateTest, SelectionParams) {
    UUID id1 = genUUID();
    UUID id2 = genUUID();

    state.addComponent<TestSceneComponent>(createComponent(id1));
    state.addComponent<TestSceneComponent>(createComponent(id2));

    state.addSelectedComponent(id1);
    EXPECT_TRUE(state.isComponentSelected(id1));
    EXPECT_FALSE(state.isComponentSelected(id2));

    state.addSelectedComponent(id2);
    EXPECT_TRUE(state.isComponentSelected(id1));
    EXPECT_TRUE(state.isComponentSelected(id2));
    EXPECT_EQ(state.getSelectedComponents().size(), 2);

    state.removeSelectedComponent(id1);
    EXPECT_FALSE(state.isComponentSelected(id1));
    EXPECT_TRUE(state.isComponentSelected(id2));

    state.clearSelectedComponents();
    EXPECT_FALSE(state.isComponentSelected(id2));
    EXPECT_EQ(state.getSelectedComponents().size(), 0);
}

TEST_F(SceneStateTest, EdgeCases_AddDuplicate) {
    auto comp = createComponent();
    state.addComponent<TestSceneComponent>(comp);

    // Add same component again
    state.addComponent<TestSceneComponent>(comp);

    // Should not duplicate in map
    EXPECT_EQ(state.getAllComponents().size(), 1);
}

TEST_F(SceneStateTest, EdgeCases_ChildRemovalProtection) {
    auto parent = createComponent();
    auto child = createComponent();

    parent->addChildComponent(child->getUuid());
    child->setParentComponent(parent->getUuid());

    state.addComponent<TestSceneComponent>(parent);
    state.addComponent<TestSceneComponent>(child);

    // Try to remove child directly (no callerId, i.e., UUID::null)
    auto removed = state.removeComponent(child->getUuid());

    // Should be prevented
    EXPECT_TRUE(removed.empty());
    EXPECT_TRUE(state.getComponentByUuid(child->getUuid()) != nullptr);

    // Try to remove with UUID::master
    removed = state.removeComponent(child->getUuid(), Bess::UUID::master);
    EXPECT_EQ(removed.size(), 1);
    EXPECT_TRUE(state.getComponentByUuid(child->getUuid()) == nullptr);
}

TEST_F(SceneStateTest, Getters) {
    auto comp = createComponent();
    state.addComponent<TestSceneComponent>(comp);

    auto retrieved = state.getComponentByUuid<TestSceneComponent>(comp->getUuid());
    EXPECT_EQ(retrieved, comp);

    auto retrievedBase = state.getComponentByUuid(comp->getUuid());
    EXPECT_EQ(retrievedBase, comp);

    auto nonExistent = state.getComponentByUuid(genUUID());
    EXPECT_EQ(nonExistent, nullptr);
}

TEST_F(SceneStateTest, Clear) {
    auto comp = createComponent();
    state.addComponent<TestSceneComponent>(comp);
    state.addSelectedComponent(comp->getUuid());

    state.clear();

    EXPECT_EQ(state.getAllComponents().size(), 0);
    EXPECT_EQ(state.getSelectedComponents().size(), 0);
    EXPECT_EQ(state.getRootComponents().size(), 0);
}
