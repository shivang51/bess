#include "components/input_probe.h"
#include "application_state.h"
#include "common/theme.h"
#include "renderer/renderer.h"
#include "common/bind_helpers.h"

namespace Bess::Simulator::Components {

glm::vec2 inputProbeSize = {50.f, 30.f};

InputProbe::InputProbe(const UUIDv4::UUID &uid, int renderId,
                       glm::vec3 position, const UUIDv4::UUID &outputSlot)
    : Component(uid, renderId, position, ComponentType::inputProbe) {
    m_outputSlot = outputSlot;
    m_events[ComponentEventType::leftClick] = (OnLeftClickCB)BIND_FN_1(InputProbe::onLeftClick);
}

void InputProbe::render() {

    bool selected = ApplicationState::getSelectedId() == m_uid;
    float thickness = 2.f;

    Slot* slot =
        (Slot*)Simulator::ComponentsManager::components[m_outputSlot].get();

    float r = 1.0, r1 = r;
    thickness /= inputProbeSize.y;

    auto bgColor = Theme::componentBGColor;

    if (slot->getState() == DigitalState::high) {
        bgColor = glm::vec3(50.f / 255.f, 100.f / 255.f, 50.f/ 255.f);
    }

    Renderer2D::Renderer::quad(
        m_position, inputProbeSize,  bgColor, m_renderId,
        {r1, r1, r1, r1}, Theme::componentBorderColor, thickness);

    slot->update(m_position +
                 glm::vec3({(inputProbeSize.x / 2) - 16.f, 0.f, 0.f}));
    slot->render();
}

void InputProbe::onLeftClick(const glm::vec2& pos)
{
    ApplicationState::setSelectedId(m_uid);
    auto slot = (Slot*)ComponentsManager::components[m_outputSlot].get();
    slot->flipState();
}

} // namespace Bess::Simulator::Components
