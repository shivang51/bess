#include "event_dispatcher.h"
#include "gtest/gtest.h"
#include "pages/main_page/scene_components/scene_comp_types.h"
#include "scene/scene_events.h"
#include "scene/scene_state/scene_state.h"
#include "scene/scene_state/components/scene_component.h"
#include <algorithm>
#include <unordered_set>

using namespace Bess::Canvas;
using Bess::UUID;

namespace {
    UUID genUUID() {
        static uint64_t counter = 1000;
        return UUID(counter++);
    }

    class TestSceneComponent : public SceneComponent {
      public:
        explicit TestSceneComponent(UUID id = UUID::null) {
            m_uuid = id == UUID::null ? genUUID() : id;
            m_name = "TestSceneComponent";
        }

        SceneComponentType getType() const override {
            return SceneComponentType::_base;
        }
    };

    bool containsUuid(const std::vector<UUID> &uuids, UUID target) {
        return std::ranges::find(uuids, target) != uuids.end();
    }
} // namespace

class SceneStateTest : public testing::Test {
  protected:
    void SetUp() override {
        state.clear();
        Bess::EventSystem::EventDispatcher::instance().clear();
    }

    void TearDown() override {
        Bess::EventSystem::EventDispatcher::instance().dispatchAll();
        Bess::EventSystem::EventDispatcher::instance().clear();
        state.clear();
    }

    std::shared_ptr<TestSceneComponent> createComponent(UUID id = UUID::null) {
        return std::make_shared<TestSceneComponent>(id);
    }

    SceneState state;
};

TEST_F(SceneStateTest, AddComponentRegistersRootAndDispatchesAddedEvent) {
    const auto comp = createComponent();

    int addedEvents = 0;
    auto sink = Bess::EventSystem::EventDispatcher::instance().sink<Events::ComponentAddedEvent>();
    sink.connect([&](const Events::ComponentAddedEvent &event) {
        EXPECT_EQ(event.uuid, comp->getUuid());
        EXPECT_EQ(event.type, SceneComponentType::_base);
        EXPECT_EQ(event.state, &state);
        addedEvents++;
    });

    state.addComponent(comp);
    Bess::EventSystem::EventDispatcher::instance().dispatchAll();

    ASSERT_EQ(state.getComponentByUuid(comp->getUuid()), comp);
    EXPECT_TRUE(state.getRootComponents().contains(comp->getUuid()));
    EXPECT_EQ(addedEvents, 1);
}

TEST_F(SceneStateTest, AddDuplicateComponentIsIgnored) {
    const auto comp = createComponent();
    state.addComponent(comp);

    state.addComponent(comp);
    Bess::EventSystem::EventDispatcher::instance().dispatchAll();

    EXPECT_EQ(state.getAllComponents().size(), 1u);
    EXPECT_TRUE(state.getRootComponents().contains(comp->getUuid()));
}

TEST_F(SceneStateTest, RemoveComponentClearsSelectionRootAndDispatchesRemovedEvent) {
    const auto comp = createComponent();
    state.addComponent(comp);
    state.addSelectedComponent(comp->getUuid());

    int removedEvents = 0;
    auto sink = Bess::EventSystem::EventDispatcher::instance().sink<Events::ComponentRemovedEvent>();
    sink.connect([&](const Events::ComponentRemovedEvent &event) {
        EXPECT_EQ(event.uuid, comp->getUuid());
        EXPECT_EQ(event.type, SceneComponentType::_base);
        EXPECT_EQ(event.state, &state);
        removedEvents++;
    });

    const auto removed = state.removeComponent(comp->getUuid());
    Bess::EventSystem::EventDispatcher::instance().dispatchAll();

    ASSERT_EQ(removed.size(), 1u);
    EXPECT_EQ(removed.front(), comp->getUuid());
    EXPECT_EQ(state.getComponentByUuid(comp->getUuid()), nullptr);
    EXPECT_FALSE(state.getRootComponents().contains(comp->getUuid()));
    EXPECT_FALSE(state.isComponentSelected(comp->getUuid()));
    EXPECT_EQ(removedEvents, 1);
}

TEST_F(SceneStateTest, RemoveComponentRecursivelyRemovesChildren) {
    const auto parent = createComponent();
    const auto child = createComponent();
    const auto grandChild = createComponent();

    state.addComponent(parent);
    state.addComponent(child);
    state.addComponent(grandChild);

    state.attachChild(parent->getUuid(), child->getUuid(), false);
    state.attachChild(child->getUuid(), grandChild->getUuid(), false);

    const auto removed = state.removeComponent(parent->getUuid());

    EXPECT_EQ(removed.size(), 3u);
    EXPECT_TRUE(containsUuid(removed, parent->getUuid()));
    EXPECT_TRUE(containsUuid(removed, child->getUuid()));
    EXPECT_TRUE(containsUuid(removed, grandChild->getUuid()));
    EXPECT_EQ(state.getComponentByUuid(parent->getUuid()), nullptr);
    EXPECT_EQ(state.getComponentByUuid(child->getUuid()), nullptr);
    EXPECT_EQ(state.getComponentByUuid(grandChild->getUuid()), nullptr);
}

TEST_F(SceneStateTest, ChildRemovalIsBlockedUnlessCallerIsParentOrMaster) {
    const auto parent = createComponent();
    const auto child = createComponent();

    state.addComponent(parent);
    state.addComponent(child);
    state.attachChild(parent->getUuid(), child->getUuid(), false);

    auto removed = state.removeComponent(child->getUuid());
    EXPECT_TRUE(removed.empty());
    EXPECT_NE(state.getComponentByUuid(child->getUuid()), nullptr);

    removed = state.removeComponent(child->getUuid(), parent->getUuid());
    ASSERT_EQ(removed.size(), 1u);
    EXPECT_EQ(removed.front(), child->getUuid());
    EXPECT_EQ(state.getComponentByUuid(child->getUuid()), nullptr);
}

TEST_F(SceneStateTest, AttachChildReparentsAndDispatchesEntityReparentedEvent) {
    const auto parentA = createComponent();
    const auto parentB = createComponent();
    const auto child = createComponent();

    state.addComponent(parentA);
    state.addComponent(parentB);
    state.addComponent(child);
    state.attachChild(parentA->getUuid(), child->getUuid(), false);

    int reparentEvents = 0;
    auto sink = Bess::EventSystem::EventDispatcher::instance().sink<Events::EntityReparentedEvent>();
    sink.connect([&](const Events::EntityReparentedEvent &event) {
        EXPECT_EQ(event.entityUuid, child->getUuid());
        EXPECT_EQ(event.newParentUuid, parentB->getUuid());
        EXPECT_EQ(event.prevParent, parentA->getUuid());
        EXPECT_EQ(event.state, &state);
        reparentEvents++;
    });

    state.attachChild(parentB->getUuid(), child->getUuid());
    Bess::EventSystem::EventDispatcher::instance().dispatchAll();

    EXPECT_EQ(child->getParentComponent(), parentB->getUuid());
    EXPECT_FALSE(parentA->getChildComponents().contains(child->getUuid()));
    EXPECT_TRUE(parentB->getChildComponents().contains(child->getUuid()));
    EXPECT_FALSE(state.getRootComponents().contains(child->getUuid()));
    EXPECT_EQ(reparentEvents, 1);
}

TEST_F(SceneStateTest, DetachChildMakesItRootAndDispatchesEntityReparentedEvent) {
    const auto parent = createComponent();
    const auto child = createComponent();

    state.addComponent(parent);
    state.addComponent(child);
    state.attachChild(parent->getUuid(), child->getUuid(), false);

    int reparentEvents = 0;
    auto sink = Bess::EventSystem::EventDispatcher::instance().sink<Events::EntityReparentedEvent>();
    sink.connect([&](const Events::EntityReparentedEvent &event) {
        EXPECT_EQ(event.entityUuid, child->getUuid());
        EXPECT_EQ(event.newParentUuid, UUID::null);
        EXPECT_EQ(event.prevParent, parent->getUuid());
        EXPECT_EQ(event.state, &state);
        reparentEvents++;
    });

    state.detachChild(child->getUuid());
    Bess::EventSystem::EventDispatcher::instance().dispatchAll();

    EXPECT_EQ(child->getParentComponent(), UUID::null);
    EXPECT_FALSE(parent->getChildComponents().contains(child->getUuid()));
    EXPECT_TRUE(state.getRootComponents().contains(child->getUuid()));
    EXPECT_EQ(reparentEvents, 1);
}

TEST_F(SceneStateTest, OrphanComponentClearsParentAndKeepsChildInRootSet) {
    const auto parent = createComponent();
    const auto child = createComponent();

    state.addComponent(parent);
    state.addComponent(child);
    state.attachChild(parent->getUuid(), child->getUuid(), false);

    int reparentEvents = 0;
    auto sink = Bess::EventSystem::EventDispatcher::instance().sink<Events::EntityReparentedEvent>();
    sink.connect([&](const Events::EntityReparentedEvent &event) {
        EXPECT_EQ(event.entityUuid, child->getUuid());
        EXPECT_EQ(event.newParentUuid, UUID::null);
        EXPECT_EQ(event.prevParent, parent->getUuid());
        reparentEvents++;
    });

    state.orphanComponent(child->getUuid());
    Bess::EventSystem::EventDispatcher::instance().dispatchAll();

    EXPECT_EQ(child->getParentComponent(), UUID::null);
    EXPECT_TRUE(state.getRootComponents().contains(child->getUuid()));
    EXPECT_TRUE(parent->getChildComponents().contains(child->getUuid()));
    EXPECT_EQ(reparentEvents, 1);
}

TEST_F(SceneStateTest, SelectionHelpersTrackSelectedComponents) {
    const auto compA = createComponent();
    const auto compB = createComponent();

    state.addComponent(compA);
    state.addComponent(compB);

    state.addSelectedComponent(compA->getUuid());
    state.addSelectedComponent(compB->getUuid());

    EXPECT_TRUE(state.isComponentSelected(compA->getUuid()));
    EXPECT_TRUE(state.isComponentSelected(compB->getUuid()));
    EXPECT_EQ(state.getSelectedComponents().size(), 2u);

    state.removeSelectedComponent(compA->getUuid());
    EXPECT_FALSE(state.isComponentSelected(compA->getUuid()));
    EXPECT_TRUE(state.isComponentSelected(compB->getUuid()));

    state.clearSelectedComponents();
    EXPECT_TRUE(state.getSelectedComponents().empty());
    EXPECT_FALSE(state.isComponentSelected(compB->getUuid()));
}

TEST_F(SceneStateTest, ClearRemovesComponentsRootsAndSelection) {
    const auto comp = createComponent();
    state.addComponent(comp);
    state.addSelectedComponent(comp->getUuid());

    state.clear();

    EXPECT_TRUE(state.getAllComponents().empty());
    EXPECT_TRUE(state.getRootComponents().empty());
    EXPECT_TRUE(state.getSelectedComponents().empty());
}

TEST_F(SceneStateTest, RemoveComponentWithMasterDetachesItFromParentChildSet) {
    const auto parent = createComponent();
    const auto child = createComponent();

    state.addComponent(parent);
    state.addComponent(child);
    state.attachChild(parent->getUuid(), child->getUuid(), false);

    const auto removed = state.removeComponent(child->getUuid(), Bess::UUID::master);

    ASSERT_EQ(removed.size(), 1u);
    EXPECT_EQ(removed.front(), child->getUuid());
    EXPECT_FALSE(parent->getChildComponents().contains(child->getUuid()));
    EXPECT_EQ(state.getComponentByUuid(child->getUuid()), nullptr);
}

TEST_F(SceneStateTest, AssignRuntimeIdReusesFreedIdsAfterRemoval) {
    const auto first = createComponent();
    const auto second = createComponent();

    state.addComponent(first);
    const auto firstRuntimeId = first->getRuntimeId();
    ASSERT_NE(firstRuntimeId, PickingId::invalidRuntimeId);

    state.removeComponent(first->getUuid(), Bess::UUID::master);
    state.addComponent(second);

    EXPECT_EQ(second->getRuntimeId(), firstRuntimeId);
}
