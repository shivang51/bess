#pragma once
#include "ext/vector_float2.hpp"
#include "components_manager/component_type.h"

#include "uuid_v4.h"

#include <functional>
#include <unordered_map>
#include <any>

namespace Bess::Simulator::Components {

    enum class ComponentEventType {
        leftClick,
        rightClick,
        mouseEnter,
        mouseLeave,
        mouseHover,
        focus,
        focusLost,
    };

    struct ComponentEventData {
        ComponentEventType type;
        glm::vec2 pos;
    };

    typedef std::function<void(const glm::vec2& pos)> OnLeftClickCB;
    typedef std::function<void(const glm::vec2& pos)> OnRightClickCB;
    typedef std::function<void()> VoidCB;


    class Component {
    public:
        Component(const UUIDv4::UUID& uid, int renderId, glm::vec2 position, ComponentType type);
        virtual ~Component() = default;


        UUIDv4::UUID getId() const;
        std::string getIdStr() const;

        int getRenderId() const;
        glm::vec2& getPosition();

        ComponentType getType() const;

        void onEvent(ComponentEventData e);

        virtual void render() = 0;

        std::string getName() const;

    protected:
        int m_renderId;
        UUIDv4::UUID m_uid;
        glm::vec2 m_position;
        ComponentType m_type;

        std::unordered_map<ComponentEventType, std::any> m_events;
    };
} // namespace Bess::Components
