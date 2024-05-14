#include "components/output_probe.h"
#include "renderer/renderer.h"
#include "common/theme.h"
#include "application_state.h"
namespace Bess::Simulator::Components
{

    glm::vec2 outputProbeSize = {50.f, 30.f};
    OutputProbe::OutputProbe(const UUIDv4::UUID &uid, int renderId, glm::vec2 position, const UUIDv4::UUID &outputSlot) : Component(uid, renderId, position, ComponentType::outputProbe)
    {
        m_outputSlot = outputSlot;
        m_events[ComponentEventType::leftClick] = (OnLeftClickCB)[&](const glm::vec2 &pos)
        {
            ApplicationState::setSelectedId(m_uid);
        };
    }

    void OutputProbe::render()
    {
        bool selected = ApplicationState::getSelectedId() == m_uid;
        float thickness = 1.f;

        float r = 0.2, r1 = r;

        Renderer2D::Renderer::quad(m_position, outputProbeSize + (thickness * 2), selected ? Theme::selectedCompColor : Theme::componentBorderColor, m_renderId, {r1, r1, r1, r1});
        Renderer2D::Renderer::quad(m_position, outputProbeSize, Theme::componentBGColor, m_renderId, {r1, r1, r1, r1});

        Slot *slot = (Slot *)Simulator::ComponentsManager::components[m_outputSlot].get();
        slot->update(m_position + glm::vec2({-(outputProbeSize.x / 2) + 16.f, 0.f}));
        slot->render();
    }
}