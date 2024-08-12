#include "components/slot.h"
#include "renderer/renderer.h"
#include "components/jcomponent.h"
#include "application_state.h"

#include "common/theme.h"
#include "ui/ui.h"
#include <common/bind_helpers.h>
#include "simulator/simulator_engine.h"

namespace Bess::Simulator::Components {
float fontSize = 10.f;
glm::vec3 connectedBg = {0.42f, 0.82f, 0.42f};

Slot::Slot(const UUIDv4::UUID &uid, const UUIDv4::UUID& parentUid, int id, ComponentType type)
    : Component(uid, id, { 0.f, 0.f, 0.f }, type), m_parentUid{ parentUid } {
    m_events[ComponentEventType::leftClick] =
        (OnLeftClickCB)BIND_FN_1(Slot::onLeftClick);
    m_events[ComponentEventType::mouseHover] =
        (VoidCB)BIND_FN(Slot::onMouseHover);
    m_state = DigitalState::low;
}

void Slot::update(const glm::vec3& pos, const std::string& label) {
    m_position = pos; 
    setLabel(label);
}

void Slot::update(const glm::vec3& pos, const glm::vec2& labelOffset) {
    m_position = pos;
    m_labelOffset = labelOffset;
}

void Slot::update(const glm::vec3& pos, const glm::vec2& labelOffset, const std::string& label) {
    m_position = pos;
    m_labelOffset = labelOffset;
    setLabel(label);
}

void Slot::update(const glm::vec3 &pos) { m_position = pos; }

void Slot::render() {
    float r = 4.0f;
    Renderer2D::Renderer::circle(m_position, m_highlightBorder ? r + 2.0f: r + 1.f,
                                 m_highlightBorder
                                     ? Theme::selectedWireColor
                                     : Theme::componentBorderColor,
                                 m_renderId);

    Renderer2D::Renderer::circle(
        m_position, r,
        (m_connections.size() == 0) ? Theme::backgroundColor : connectedBg,
        m_renderId);
    
    if (m_label == "") return;
	auto charSize = Renderer2D::Renderer::getCharRenderSize('Z', fontSize);
    glm::vec3 offset = glm::vec3(m_labelOffset, ComponentsManager::zIncrement);
    if (offset.x < 0.f) {
        offset.x -= m_labelWidth;
    }
    offset.y -= charSize.y / 2.f;
    Renderer2D::Renderer::text(m_label, m_position + offset, fontSize, { 1.f, 1.f, 1.f }, ComponentsManager::compIdToRid(m_parentUid));
}

void Slot::onLeftClick(const glm::vec2 &pos) {
    if (ApplicationState::drawMode == DrawMode::none) {
        ApplicationState::connStartId = m_uid;
        ApplicationState::points.emplace_back(m_position);
        ApplicationState::drawMode = DrawMode::connection;
        return;
    }

    auto slot = ComponentsManager::components[ApplicationState::connStartId];

    if (ApplicationState::connStartId == m_uid || slot->getType() == m_type)
        return;

    ComponentsManager::addConnection(ApplicationState::connStartId, m_uid);

    ApplicationState::drawMode = DrawMode::none;
    ApplicationState::connStartId = ComponentsManager::emptyId;
    ApplicationState::points.pop_back();
}

void Slot::onMouseHover() { UI::UIMain::setCursorPointer(); }

void Slot::onChange()
{
    if (m_type == ComponentType::outputSlot) {
        for (auto& slot : m_connections) {
            Simulator::Engine::addToSimQueue(slot, m_uid, m_state);
        }
    }
    else {
        Simulator::Engine::addToSimQueue(m_parentUid, m_uid, m_state);
    }
}

void Slot::calculateLabelWidth(float fontSize) {
    m_labelWidth = 0.f;
    for (auto& ch_ : m_label) {
        auto& ch = Renderer2D::Renderer::getCharRenderSize(ch_, fontSize);
        m_labelWidth += ch.x;
    }
}

void Slot::addConnection(const UUIDv4::UUID &uid, bool simulate) {
    if (isConnectedTo(uid)) return;
    m_connections.emplace_back(uid);
    if (!simulate) return;
    Simulator::Engine::addToSimQueue(uid, m_uid, m_state);
}

bool Slot::isConnectedTo(const UUIDv4::UUID& uId) {
    for (auto& conn : m_connections) {
        if (conn == uId) return true;
    }
    return false;
}

void Slot::highlightBorder(bool highlight) { m_highlightBorder = highlight; }

Simulator::DigitalState Slot::getState() const
{
    return m_state;
}

void Slot::setState(const UUIDv4::UUID& uid, Simulator::DigitalState state, bool forceUpdate)
{
    if (m_type == ComponentType::inputSlot) {
        stateChangeHistory[uid] = state == DigitalState::high; 
        if (state == DigitalState::low) {
            for (auto& ent : stateChangeHistory) {
                if (!ent.second) continue;
                state = DigitalState::high;
                break;
            }
        }
    }

    if (m_state == state && !forceUpdate) return;
    m_state = state;
    onChange();
}

DigitalState Slot::flipState() {
    if (DigitalState::high == m_state) {
        m_state = DigitalState::low;
    }
    else {
        m_state = DigitalState::high;
    }
    onChange();
    return m_state;
}

const UUIDv4::UUID& Slot::getParentId()
{
    return m_parentUid;
}

void Slot::generate(const glm::vec3& pos)
{
}
const std::string& Slot::getLabel() {
    return m_label;
}
void Slot::setLabel(const std::string& label) {
    m_label = label;
    calculateLabelWidth(fontSize);
}
const glm::vec2& Slot::getLabelOffset()
{
    return m_labelOffset;
}

void Slot::setLabelOffset(const glm::vec2& offset)
{
    m_labelOffset = offset;
}

const std::vector<UUIDv4::UUID>& Slot::getConnections()
{
    return m_connections;
}

void Slot::simulate(const UUIDv4::UUID& uid, DigitalState state)
{
    setState(uid, state);
}

void Slot::refresh(const UUIDv4::UUID& uid, DigitalState state)
{
    setState(uid, state, true);
}
} // namespace Bess::Simulator::Components
