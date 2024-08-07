#pragma once
#include "component.h"
#include "components_manager/components_manager.h"
#include "renderer/renderer.h"

namespace Bess::Simulator::Components {
class Connection : public Component {

  public:
    Connection(const UUIDv4::UUID &uid, int renderId, const UUIDv4::UUID &slot1,
               const UUIDv4::UUID &slot2);
    ~Connection() = default;
    Connection();

    void render() override;
    void generate(const UUIDv4::UUID& slot1, const UUIDv4::UUID& slot2, const glm::vec3& pos = { 0.f, 0.f, 0.f });
    void generate(const glm::vec3& pos = { 0.f, 0.f, 0.f }) override;

  private:
    UUIDv4::UUID m_slot1;
    UUIDv4::UUID m_slot2;

    void onLeftClick(const glm::vec2 &pos);
    void onFocusLost();
    void onFocus();
    void onMouseHover();
};
} // namespace Bess::Simulator::Components
