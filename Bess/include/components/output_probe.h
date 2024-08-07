#pragma once
#include "component.h"
#include "slot.h"

namespace Bess::Simulator::Components {
class OutputProbe : public Component {
  public:
      OutputProbe();
    OutputProbe(const UUIDv4::UUID &uid, int renderId, glm::vec3 position,
                const UUIDv4::UUID &outputSlot);
    ~OutputProbe() = default;

    void render() override;

    void generate(const glm::vec3& pos = { 0.f, 0.f, 0.f }) override;

  private:
    UUIDv4::UUID m_outputSlot;
};
} // namespace Bess::Simulator::Components
