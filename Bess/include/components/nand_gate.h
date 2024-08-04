#pragma once
#include "component.h"
#include "components/slot.h"
#include "ext/vector_float2.hpp"
#include <vector>

#include "uuid_v4.h"

namespace Bess::Simulator::Components {
class NandGate : public Component {
  public:
      NandGate();
    NandGate(const UUIDv4::UUID &uid, int renderId, glm::vec3 position,
             std::vector<UUIDv4::UUID> inputSlots,
             std::vector<UUIDv4::UUID> outputSlots);
    ~NandGate() = default;

    void render() override;

    void simulate();

    void generate(const glm::vec3& pos = { 0.f, 0.f, 0.f }) override;

  private:
    std::vector<UUIDv4::UUID> m_inputSlots;
    std::vector<UUIDv4::UUID> m_outputSlots;

    void onLeftClick(const glm::vec2 &pos);
    void onRightClick(const glm::vec2 &pos);
};
} // namespace Bess::Simulator::Components
