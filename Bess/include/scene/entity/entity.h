#pragma once

#include "../events/entity_event.h"
#include "scene/transform/transform_2d.h"
#include "uuid.h"
#include <queue>

namespace Bess::Scene {

    class Entity {
      public:
        Entity() = default;
        virtual ~Entity();

        virtual void render() = 0;

        void update();
        void onEvent(const Events::EntityEvent &evt);

      private:
        uuids::uuid m_uid;
        Transform::Transform2D transform;
        bool m_visible = true;
        bool m_selected = false;
        bool m_hovered = false;

        std::queue<Events::EntityEvent> m_events;
    };
} // namespace Bess::Scene
