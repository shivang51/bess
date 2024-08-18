#pragma once
#include "component.h"
#include "slot.h"
#include "json.hpp"

namespace Bess::Simulator::Components {
class InputProbe : public Component {
  public:
      InputProbe();

    InputProbe(const uuids::uuid &uid, int renderId, glm::vec3 position,
               const uuids::uuid &outputSlot);
    ~InputProbe() = default;

    void render() override;

    void deleteComponent() override;

    void generate(const glm::vec3& pos = { 0.f, 0.f, 0.f }) override;

    nlohmann::json toJson();

    static void fromJson(const nlohmann::json& data);

    void simulate() const;
    void refresh() const;

  private:
    uuids::uuid m_outputSlot;

    void onLeftClick(const glm::vec2& pos);
};
} // namespace Bess::Simulator::Components
