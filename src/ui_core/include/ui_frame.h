#pragma once

#include "bess_core/renderer/renderer_2d.h"
#include "common/bess_uuid.h"
#include "common/logger.h"
#include "ext/vector_float2.hpp"
#include <cstdint>
#include <string>
namespace Bess::UI {

    struct UIDrawContext {
        std::shared_ptr<Core::Renderer::IRenderer2D> renderer = nullptr;
    };

    enum class FrameState : uint8_t {
        expanded = 1,
        collapsed = 1 << 1,
        hidden = 1 << 2,
        docked = 1 << 3,
    };

    class Widget {
      protected:
        UUID m_id = UUID::null;
    };

    class IDockNode : public Widget {
      public:
        void dock(const std::shared_ptr<IDockNode> &node, DockZone zone) {
            if (!m_allowsDocking) {
                BESS_WARN("Docking is not allowed for this node. {}",
                          (uint64_t)m_id);
                return;
            }
            m_dockedFrames[zone].push_back(node);
        }

      public:
        bool isSplitter() const {
            return m_nodeType == DockNodeType::split;
        }

        MAKE_GETTER_SETTER(DockNodeType, nodeType, m_nodeType)
        MAKE_GETTER_SETTER(SplitDirection, splitDir, m_splitDir)
        MAKE_GETTER_SETTER(bool, isDocked, m_isDocked)
        MAKE_GETTER_SETTER(bool, allowsDocking, m_allowsDocking)

      protected:
        bool m_isDocked = false;
        bool m_allowsDocking = false;
        HashMap<DockZone, std::vector<std::shared_ptr<IDockNode>>>
            m_dockedFrames;
        DockNodeType m_nodeType = DockNodeType::container;
        SplitDirection m_splitDir = SplitDirection::horizontal;
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
    class Frame : public Widget {
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

      public:
        MAKE_GETTER_SETTER(std::string, title, m_title)
        MAKE_GETTER_SETTER(glm::vec2, pos, m_pos)
        MAKE_GETTER_SETTER(glm::vec2, size, m_size)
        MAKE_GETTER_SETTER(FrameState, state, m_state)
        MAKE_GETTER_SETTER(float, zValue, m_zValue)
        MAKE_GETTER_SETTER(UUID, dockId, m_dockId)

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
        UUID m_dockId = UUID::null;
    };
} // namespace Bess::UI
