#pragma once
#include "component.h"
#include "slot.h"
#include "json.hpp"


namespace Bess::Simulator::Components {
class OutputProbe : public Component {
  public:
      OutputProbe();
    OutputProbe(const uuids::uuid &uid, int renderId, glm::vec3 position,
                const uuids::uuid &outputSlot);
    ~OutputProbe() = default;

    void render() override;

    void deleteComponent() override;

    void generate(const glm::vec3& pos = { 0.f, 0.f, 0.f }) override;

    nlohmann::json toJson();

    static void fromJson(const nlohmann::json& data);

  private:
    uuids::uuid m_inputSlot;
};
} // namespace Bess::Simulator::Components
