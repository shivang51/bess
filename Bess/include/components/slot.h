#pragma once

#include "component.h"

namespace Bess::Simulator::Components {
	class Slot : public Component {
	public:
		Slot(const UUIDv4::UUID& uid, int renderId, ComponentType type);
		~Slot() = default;

		void update(const glm::vec2& pos);

		void render() override ;

		void addConnection(const UUIDv4::UUID& uId);
        void highlightBorder(bool highlight = true);

	private:
		// contains one way connection from starting slot to other
		std::vector<UUIDv4::UUID> connections;
        bool m_highlightBorder = false;
		void onLeftClick(const glm::vec2& pos);
		void onMouseHover();

    };
}
