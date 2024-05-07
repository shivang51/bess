#include "components/input_probe.h"
#include "renderer/renderer.h"
#include "common/theme.h"
#include "application_state.h"
namespace Bess::Simulator::Components {

    glm::vec2 inputProbeSize = { 0.1f, 0.05f };

    InputProbe::InputProbe(const UUIDv4::UUID& uid, int renderId, glm::vec2 position,const UUIDv4::UUID& outputSlot) : Component(uid, renderId, position, ComponentType::inputProbe) {
        m_outputSlot = outputSlot;
        m_events[ComponentEventType::leftClick] = (OnLeftClickCB)[&](const glm::vec2& pos) {
            ApplicationState::setSelectedId(m_uid);
            };
    }

    void InputProbe::render() {
        
        bool selected = ApplicationState::getSelectedId() == m_uid;
        float thickness = selected ? 0.005f : 0.0035f;

        float r = 0.2, r1 = r - (thickness * 2) ;

        Renderer2D::Renderer::quad(
            { m_position.x - thickness, m_position.y + thickness },
            { inputProbeSize.x + (thickness * 2.f), inputProbeSize.y + (thickness * 2.f)},
            selected ? Theme::componentSelectionColor : Theme::componentBorderColor, m_renderId, { r, r, r, r});

        Renderer2D::Renderer::quad(m_position, inputProbeSize, Theme::componentBGColor, m_renderId, { r1, r1, r1, r1});
        
		Slot* slot = (Slot*)Simulator::ComponentsManager::components[m_outputSlot].get();
		slot->update(m_position +  glm::vec2({ inputProbeSize.x - 0.025, -inputProbeSize.y / 2.f }));
		slot->render();
    }
}