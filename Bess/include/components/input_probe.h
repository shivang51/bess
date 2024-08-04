#pragma once
#include "component.h"
#include "slot.h"

namespace Bess::Simulator::Components {
class InputProbe : public Component {
  public:
    InputProbe(const UUIDv4::UUID &uid, int renderId, glm::vec3 position,
               const UUIDv4::UUID &outputSlot);
    ~InputProbe() = default;

    void render() override;

  private:
    UUIDv4::UUID m_outputSlot;

    void onLeftClick(const glm::vec2& pos);
};
} // namespace Bess::Simulator::Components
