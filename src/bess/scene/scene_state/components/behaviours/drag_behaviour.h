#pragma once

#include "event_dispatcher.h"
#include "scene/scene_events.h"

namespace Bess::Canvas {
    constexpr float SNAP_ANOUNT = 2.f;

    class IDragBehaviour {
      public:
        virtual ~IDragBehaviour() = default;
        virtual void onMouseDragged(const Events::MouseDraggedEvent &e) = 0;
        virtual void onMouseDragEnd() = 0;
    };

    template <typename Derived>
    class DragBehaviour : public IDragBehaviour {
      public:
        DragBehaviour() {
            initDragBehaviour();
        }

        void onMouseDragged(const Events::MouseDraggedEvent &e) override {
            if (!m_isDragging) {
                onMouseDragBegin(e);
            }
            auto &self = static_cast<Derived &>(*this);
            auto newPos = e.mousePos + m_dragOffset;
            newPos = glm::round(newPos / SNAP_ANOUNT) * SNAP_ANOUNT;
            self.setPosition(glm::vec3(newPos, self.getTransform().position.z));
        }

        void onMouseDragEnd() override {
            m_isDragging = false;
            auto &self = static_cast<Derived &>(*this);
            EventSystem::EventDispatcher::instance().dispatch(Events::EntityMovedEvent{
                .entityUuid = static_cast<const Derived &>(*this).getUuid(),
                .oldPos = m_dragStartPos,
                .newPos = self.getTransform().position,
            });
        }

      protected:
        void initDragBehaviour() {
            auto &self = static_cast<Derived &>(*this);
            self.setIsDraggable(true);
        }

        virtual void onMouseDragBegin(const Events::MouseDraggedEvent &e) {
            const auto &self = static_cast<const Derived &>(*this);
            m_dragOffset = glm::vec2(self.getAbsolutePosition(*e.sceneState)) - e.mousePos;
            m_dragStartPos = self.getTransform().position;
            m_isDragging = true;
        }

        bool m_isDragging = false;
        glm::vec2 m_dragOffset = {0.f, 0.f};
        glm::vec3 m_dragStartPos = {0.f, 0.f, 0.f};
        friend Derived;
    };
} // namespace Bess::Canvas
