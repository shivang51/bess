#pragma once

#include "dock.h"
#include "ext/vector_float2.hpp"
#include "widget.h"

#include <cstdint>
#include <string>

namespace Bess::UI {

    enum class FrameState : uint8_t {
        expanded = 1,
        collapsed = 1 << 1,
        hidden = 1 << 2,
        docked = 1 << 3,
    };

    // Right now idea is to keep it the most high level (Like ImGui Window).
    // Every thing else will be rendered inside it.
    // Each frame will be responsible for maintaing all ui widgets inside
    // it. It will be: Docakable, Draggable, Resizable, Collapsable,
    // Closable I am just winging it right now, will see how it goes.
    //
    // I am thinking each frame will have a texture attached to them to
    // which it will write. Or should we have just one master texture, but
    // then draggable windows will be a problem, if i plane to draw them
    // outside the main window.
    class Frame {
      public:
        bool isExpanded() const {
            return stateInt() & (uint8_t)FrameState::expanded;
        }

        bool isCollapsed() const {
            return stateInt() & (uint8_t)FrameState::collapsed;
        }

        bool isHidden() const {
            return stateInt() & (uint8_t)FrameState::hidden;
        }

        bool isDocked() const {
            return stateInt() & (uint8_t)FrameState::docked;
        };

        void draw(const UIDrawContext &ctx) {
            if (isHidden()) {
                return;
            }
        }

        bool dock(const std::shared_ptr<IDockNode> &node, DockZone zone) {
            if (!m_dockNode) {
                BESS_WARN("Docking is not allowed for this node. {}",
                          (uint64_t)m_id);
                return false;
            }

            return m_dockNode->dock(node, zone);
        }

      public:
        MAKE_GETTER_SETTER(std::string, Title, m_title)
        MAKE_GETTER_SETTER(glm::vec2, Pos, m_pos)
        MAKE_GETTER_SETTER(glm::vec2, Size, m_size)
        MAKE_GETTER_SETTER(FrameState, State, m_state)
        MAKE_GETTER_SETTER(float, ZValue, m_zValue)
        MAKE_GETTER_SETTER(UUID, Id, m_id)

      private:
        uint8_t stateInt() const {
            return static_cast<uint8_t>(m_state);
        }

      private:
        std::string m_title{"UI Frame"};
        glm::vec2 m_pos{0.f, 0.f};
        glm::vec2 m_size{300.f, 300.f};
        FrameState m_state{FrameState::expanded};
        float m_zValue = 0.f;
        std::shared_ptr<DockContainer> m_dockNode = nullptr;
        UUID m_id = UUID::null;
    };
} // namespace Bess::UI
