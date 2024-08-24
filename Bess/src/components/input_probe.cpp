#include "components/input_probe.h"
#include "components/connection.h"
#include "application_state.h"
#include "settings/viewport_theme.h"
#include "renderer/renderer.h"
#include "common/bind_helpers.h"
#include "common/helpers.h"
#include "simulator/simulator_engine.h"

namespace Bess::Simulator::Components {

glm::vec2 inputProbeSize = {65.f, 25.f};

InputProbe::InputProbe(): Component()
{
}

InputProbe::InputProbe(const uuids::uuid &uid, int renderId,
                       glm::vec3 position, const uuids::uuid &outputSlot)
    : Component(uid, renderId, position, ComponentType::inputProbe) {
    m_name = "Input Probe";
    m_outputSlot = outputSlot;
    m_events[ComponentEventType::leftClick] = (OnLeftClickCB)BIND_FN_1(InputProbe::onLeftClick);
}

void InputProbe::render() {

    bool selected = ApplicationState::getSelectedId() == m_uid;
    float thickness = 1.f;

    Slot* slot =
        (Slot*)Simulator::ComponentsManager::components[m_outputSlot].get();

    float r = 16.f;

    auto bgColor = ViewportTheme::componentBGColor;
    auto borderColor = ViewportTheme::componentBorderColor;

    std::string label = "Off";

    if (slot->getState() == DigitalState::high) {
        borderColor = ViewportTheme::stateHighColor;
        label = "On";
    }

    Renderer2D::Renderer::quad(m_position, inputProbeSize,  bgColor, m_renderId, glm::vec4(r), borderColor, thickness);

    slot->update(m_position + glm::vec3({(inputProbeSize.x / 2) - 12.f, 0.f, 0.f}), {-12.f, 0.f}, label);
    slot->render();
    auto leftCornerPos = Common::Helpers::GetLeftCornerPos(m_position, inputProbeSize);
    glm::vec3 v1 = { leftCornerPos.x + 16.0, leftCornerPos.y - 4.f, m_position.z };
    float l = 8.0f;
    Renderer2D::Renderer::triangle({ {v1.x - l, v1.y - l, v1.z}, v1, {v1.x + l, v1.y - l, v1.z} }, ViewportTheme::compHeaderColor, m_renderId);
    v1.y -= (l * 2.f) + 1.f;
    Renderer2D::Renderer::triangle({ {v1.x - l, v1.y + l, v1.z}, v1, {v1.x + l, v1.y + l, v1.z} }, ViewportTheme::compHeaderColor, m_renderId);
}

void InputProbe::deleteComponent()
{
    ComponentsManager::deleteComponent(m_outputSlot);
}

void InputProbe::generate(const glm::vec3& pos)
{
    auto uid = Common::Helpers::uuidGenerator.getUUID();

    auto slotId = Common::Helpers::uuidGenerator.getUUID();
    auto renderId = ComponentsManager::getNextRenderId();
    ComponentsManager::components[slotId] = std::make_shared<Components::Slot>(
        slotId, uid, renderId, ComponentType::outputSlot);
    ComponentsManager::addRenderIdToCId(renderId, slotId);
    ComponentsManager::addCompIdToRId(renderId, slotId);

    auto pos_ = pos;
    pos_.z = ComponentsManager::getNextZPos();

    renderId = ComponentsManager::getNextRenderId();
    ComponentsManager::components[uid] = std::make_shared<Components::InputProbe>(uid, renderId, pos_, slotId);
    ComponentsManager::addRenderIdToCId(renderId, uid);
    ComponentsManager::addCompIdToRId(renderId, uid);
    ComponentsManager::renderComponenets.emplace_back(uid);
}

nlohmann::json InputProbe::toJson() {
    nlohmann::json data;
    data["uid"] = Common::Helpers::uuidToStr(m_uid);
    data["type"] = (int)m_type;
    data["pos"] = Common::Helpers::EncodeVec3(m_position);
    auto slot = (Slot*)ComponentsManager::components[m_outputSlot].get();
    data["slot"] = slot->toJson();
    return data;
}

void InputProbe::fromJson(const nlohmann::json& data)
{
    uuids::uuid uid = Common::Helpers::strToUUID(static_cast<std::string>(data["uid"]));

    uuids::uuid slotId = Slot::fromJson(data["slot"], uid);

    auto pos = Common::Helpers::DecodeVec3(data["pos"]);
    pos.z = ComponentsManager::getNextZPos();

    auto renderId = ComponentsManager::getNextRenderId();

    ComponentsManager::components[uid] = std::make_shared<Components::InputProbe>(uid, renderId, pos, slotId);
    ComponentsManager::addRenderIdToCId(renderId, uid);
    ComponentsManager::addCompIdToRId(renderId, uid);
    ComponentsManager::renderComponenets.emplace_back(uid);
}

void InputProbe::simulate() const
{
    auto slot = (Slot*)ComponentsManager::components[m_outputSlot].get();
    slot->simulate(m_uid, slot->getState());
}

void InputProbe::refresh() const
{
    auto slot = (Slot*)ComponentsManager::components[m_outputSlot].get();
    slot->refresh(m_uid, slot->getState());
}

void InputProbe::onLeftClick(const glm::vec2& pos)
{
    ApplicationState::setSelectedId(m_uid);
    auto slot = (Slot*)ComponentsManager::components[m_outputSlot].get();
    slot->flipState();
}

} // namespace Bess::Simulator::Components
