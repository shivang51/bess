#pragma once
#include "component.h"
#include "components_manager/components_manager.h"
#include "scene/renderer/renderer.h"
#include "uuid.h"

namespace Bess::Simulator::Components {
    class Button : public Component {
    public:
        Button(const uuids::uuid& uid, int renderId, OnLeftClickCB cb);
        ~Button() = default;
        Button();


         void render();

         void generate(const glm::vec3& pos = { 0.f, 0.f, 0.f });
    private:
        void onLeftClick(const glm::vec2& pos);
        void onMouseEnter();
        void onMouseLeave();

        OnLeftClickCB m_leftClickCB;
        glm::vec3 m_pos;
        glm::vec3 m_color;
    };
}