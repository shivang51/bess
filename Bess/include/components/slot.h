#pragma once

#include "component.h"
#include "common/digital_state.h"

namespace Bess::Simulator::Components {
class Slot : public Component {
  public:
    Slot(const UUIDv4::UUID &uid, const UUIDv4::UUID& parentUid,  int renderId, ComponentType type);
    ~Slot() = default;

    void update(const glm::vec3 &pos);

    void render() override;

    void addConnection(const UUIDv4::UUID &uId);
    bool isConnectedTo(const UUIDv4::UUID& uId);

    void highlightBorder(bool highlight = true);

    Simulator::DigitalState getState() const;
    DigitalState flipState();
    
    void setState(const UUIDv4::UUID& uid, Simulator::DigitalState state);

    const UUIDv4::UUID& getParentId();

    void generate(const glm::vec3& pos = { 0.f, 0.f, 0.f }) override;

  private:
    // contains one way connection from starting slot to other
    std::vector<UUIDv4::UUID> connections;
    bool m_highlightBorder = false;
    void onLeftClick(const glm::vec2 &pos);
    void onMouseHover();

    // slot specific
    const UUIDv4::UUID m_parentUid;
    Simulator::DigitalState m_state;

    void onChange();

    std::unordered_map<UUIDv4::UUID, bool> stateChangeHistory = {};
};
} // namespace Bess::Simulator::Components
