#pragma once
#include <string>
#include <unordered_map>
#include <vector>

#include "components/component.h"
#include "uuid_v4.h"

#include <memory>

namespace Bess::Simulator {
    typedef std::shared_ptr<Components::Component> ComponentPtr;
    typedef std::unordered_map<UUIDv4::UUID, ComponentPtr> TComponents;

class ComponentsManager {
  public:
    static void init();


    // contains all the components that can be interacted with by user.
    static TComponents components;

    // contains all the components whose render function needs to be called from
    // scene.
    static std::vector<UUIDv4::UUID> renderComponenets;

    static void generateComponent(ComponentType type, const std::any& data = NULL, const glm::vec3& pos = { 0.f, 0.f, 0.f });

    static void addConnection(const UUIDv4::UUID &start, const UUIDv4::UUID &end);

    static const UUIDv4::UUID &renderIdToCid(int rId);
    static int compIdToRid(const UUIDv4::UUID &cid);

    static UUIDv4::UUID emptyId;

    static const float zIncrement;

    static void addRenderIdToCId(int rid, const UUIDv4::UUID& cid);
    static void addCompIdToRId(int rid, const UUIDv4::UUID& cid);

    static float getNextZPos();

    static int getNextRenderId();

  private:

    // mapping from render id to components id.
    static std::unordered_map<int, UUIDv4::UUID> renderIdToCId;

    // mapping from component id to render id.
    static std::unordered_map<UUIDv4::UUID, int> compIdToRId;

    static int renderIdCounter;

    static float zPos;

};
} // namespace Bess::Simulator
