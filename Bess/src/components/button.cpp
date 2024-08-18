#include "components/button.h"

#include "common/bind_helpers.h"
#include "ui/ui.h"

namespace Bess::Simulator::Components {


    Button::Button(const uuids::uuid& uid, int renderId, OnLeftClickCB cb)
        : Component(uid, renderId, { 0.f, 0.f, -1.f }, ComponentType::connection) {
        m_events[ComponentEventType::leftClick] = (OnLeftClickCB)cb;
        m_events[ComponentEventType::mouseEnter] = (VoidCB)BIND_FN(Button::onMouseEnter);
        m_events[ComponentEventType::mouseEnter] = (VoidCB)BIND_FN(Button::onMouseLeave);

        m_name = "Connection";
    }


    Button::Button()
    {
    }

    void Button::render()
    {
    }

    void Button::generate(const glm::vec3& pos)
    {
    }

    void Button::onLeftClick(const glm::vec2& pos)
    {
    }

    void Button::onMouseEnter()
    {
        UI::UIMain::setCursorPointer();
    }

    void Button::onMouseLeave()
    {
    }

}