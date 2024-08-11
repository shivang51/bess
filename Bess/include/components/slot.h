#pragma once

#include "component.h"
#include "common/digital_state.h"

namespace Bess::Simulator::Components {
class Slot : public Component {
  public:
    Slot(const UUIDv4::UUID &uid, const UUIDv4::UUID& parentUid,  int renderId, ComponentType type);
    ~Slot() = default;

    void update(const glm::vec3 &pos, const std::string& label);
    void update(const glm::vec3 &pos, const glm::vec2& labelOffset);
    void update(const glm::vec3 &pos, const glm::vec2& labelOffset, const std::string& label);
    void update(const glm::vec3 &pos);

    void render() override;

    void addConnection(const UUIDv4::UUID &uId, bool simulate = true);
    bool isConnectedTo(const UUIDv4::UUID& uId);

    void highlightBorder(bool highlight = true);

    Simulator::DigitalState getState() const;
    DigitalState flipState();
    
    void setState(const UUIDv4::UUID& uid, Simulator::DigitalState state);

    const UUIDv4::UUID& getParentId();

    void generate(const glm::vec3& pos = { 0.f, 0.f, 0.f }) override;

    const std::string& getLabel();
    void setLabel(const std::string& label);

    const glm::vec2& getLabelOffset();
    void setLabelOffset(const glm::vec2& label);

    const std::vector<UUIDv4::UUID>& getConnections();

    void simulate();

  private:
    // contains one way connection from starting slot to other
    std::vector<UUIDv4::UUID> m_connections;
    bool m_highlightBorder = false;
    void onLeftClick(const glm::vec2 &pos);
    void onMouseHover();

    // slot specific
    const UUIDv4::UUID m_parentUid;
    Simulator::DigitalState m_state;

    void onChange();

    std::unordered_map<UUIDv4::UUID, bool> stateChangeHistory = {};

    std::string m_label = "";
    glm::vec2 m_labelOffset = { 0.f, 0.f };
    float m_labelWidth = 0.f;

    void calculateLabelWidth(float fontSize);
};
} // namespace Bess::Simulator::Components
