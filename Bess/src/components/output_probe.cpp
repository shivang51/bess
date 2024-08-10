#include "components/output_probe.h"
#include "application_state.h"
#include "common/theme.h"
#include "renderer/renderer.h"
#include "common/helpers.h"


namespace Bess::Simulator::Components {

glm::vec2 outputProbeSize = {50.f, 25.f};
OutputProbe::OutputProbe() : Component()
{
}

OutputProbe::OutputProbe(const UUIDv4::UUID &uid, int renderId,
                         glm::vec3 position, const UUIDv4::UUID &outputSlot)
    : Component(uid, renderId, position, ComponentType::outputProbe) {
    m_name = "Output Probe";
    m_outputSlot = outputSlot;
    m_events[ComponentEventType::leftClick] =
        (OnLeftClickCB)[&](const glm::vec2 &pos) {
        ApplicationState::setSelectedId(m_uid);
    };
}

void OutputProbe::render() {
    bool selected = ApplicationState::getSelectedId() == m_uid;
    float thickness = 1.f;

    Slot* slot = (Slot*)Simulator::ComponentsManager::components[m_outputSlot].get();

    float r = 12.f;

    auto bgColor = Theme::componentBGColor;
    auto borderColor = Theme::componentBorderColor;

    std::string label = "L";
    if (slot->getState() == DigitalState::high) {
        borderColor = Theme::stateHighColor;
        label = "H";
    }

    Renderer2D::Renderer::quad(m_position, outputProbeSize, bgColor, m_renderId, glm::vec4(r), borderColor, thickness);

    slot->update(m_position + glm::vec3({ -(outputProbeSize.x / 2) + 10.f, 0.f, 0.f }), {12.f, 0.f}, label);
    slot->render();
}

void OutputProbe::generate(const glm::vec3& pos)
{
    auto uid = Common::Helpers::uuidGenerator.getUUID();

    auto slotId = Common::Helpers::uuidGenerator.getUUID();
    auto renderId = ComponentsManager::getNextRenderId();
    
    ComponentsManager::components[slotId] = std::make_shared<Components::Slot>(
        slotId, uid, renderId, ComponentType::inputSlot);

    ComponentsManager::addRenderIdToCId(renderId, slotId);
    ComponentsManager::addCompIdToRId(renderId, slotId);

    auto pos_ = pos;
    pos_.z = ComponentsManager::getNextZPos();

    renderId = ComponentsManager::getNextRenderId();
    ComponentsManager::components[uid] =
        std::make_shared<Components::OutputProbe>(uid, renderId, pos_, slotId);
    ComponentsManager::addRenderIdToCId(renderId, uid);
    ComponentsManager::addCompIdToRId(renderId, uid);
    ComponentsManager::renderComponenets.emplace_back(uid);
}

nlohmann::json OutputProbe::toJson() {
    nlohmann::json data;
    data["uid"] = m_uid.str();
    data["slotUId"] = m_outputSlot.str();
    data["type"] = (int)m_type;
    data["pos"] = Common::Helpers::EncodeVec3(m_position);
    return data;
}

void OutputProbe::fromJson(const nlohmann::json& data)
{
    UUIDv4::UUID uid;
    uid.fromStr((static_cast<std::string>(data["uid"])).c_str());

    int renderId = Simulator::ComponentsManager::getNextRenderId();

    UUIDv4::UUID slotId;
    slotId.fromStr((static_cast<std::string>(data["slotUId"])).c_str());

    ComponentsManager::components[slotId] = std::make_shared<Components::Slot>(
        slotId, uid, renderId, ComponentType::inputSlot);
    ComponentsManager::addRenderIdToCId(renderId, slotId);
    ComponentsManager::addCompIdToRId(renderId, slotId);


    auto pos = Common::Helpers::DecodeVec3(data["pos"]);
    renderId = ComponentsManager::getNextRenderId();

    ComponentsManager::components[uid] =
        std::make_shared<Components::OutputProbe>(uid, renderId, pos, slotId);
    ComponentsManager::addRenderIdToCId(renderId, uid);
    ComponentsManager::addCompIdToRId(renderId, uid);
    ComponentsManager::renderComponenets.emplace_back(uid);
}
} // namespace Bess::Simulator::Components
