#include "components/slot.h"
#include "renderer/renderer.h"

#include "application_state.h"

#include "ui.h"
#include "common/theme.h"

namespace Bess::Simulator::Components {
	

    #define BIND_EVENT_FN_1(fn) \
    std::bind(&Slot::fn, this, std::placeholders::_1)

    #define BIND_EVENT_FN(fn) \
    std::bind(&Slot::fn, this)

	glm::vec3 connectedBg = { 0.42f, 0.82f, 0.42f };

	Slot::Slot(const UUIDv4::UUID& uid, int id, ComponentType type) : Component(uid, id, { 0.f, 0.f }, type) {
		m_events[ComponentEventType::leftClick] = (OnLeftClickCB)BIND_EVENT_FN_1(onLeftClick);
		m_events[ComponentEventType::mouseHover] = (VoidCB)BIND_EVENT_FN(onMouseHover);
	}

	void Slot::update(const glm::vec2& pos) {
		m_position = pos;
	}

	void Slot::render() {


		Renderer2D::Renderer::circle(m_position, 0.019f,  m_highlightBorder ? Theme::selectedWireColor : Theme::componentBorderColor, m_renderId);
		Renderer2D::Renderer::circle(m_position, 0.015f, (connections.size() == 0) ? Theme::backgroundColor : connectedBg, m_renderId);

//		if (m_type == ComponentType::inputSlot) return;
//
//		for (auto& sid : connections) {
//			auto& sPos = ComponentsManager::components[sid]->getPosition();
//			Renderer2D::Renderer::curve(m_position, sPos, { 0.5f, 0.8f, 0.5f }, -1);
//		}
	}

	void Slot::onLeftClick(const glm::vec2& pos) {
		if (ApplicationState::drawMode == DrawMode::none) {
			ApplicationState::connStartId = m_uid;
			ApplicationState::points.emplace_back(m_position);
			ApplicationState::drawMode = DrawMode::connection;
			return;
		}

        auto slot = ComponentsManager::components[ApplicationState::connStartId];

        if(ApplicationState::connStartId == m_uid ||
            slot->getType() == m_type) return;

		ComponentsManager::generateConnection(ApplicationState::connStartId, m_uid);

		ApplicationState::drawMode = DrawMode::none;
		ApplicationState::connStartId = ComponentsManager::emptyId;
		ApplicationState::points.pop_back();
	}

	void Slot::onMouseHover() {
		UI::setCursorPointer();
	}

	void Slot::addConnection(const UUIDv4::UUID& uId) {
		connections.emplace_back(uId);
	}

    void Slot::highlightBorder(bool highlight){
        m_highlightBorder = highlight;
    }
}
