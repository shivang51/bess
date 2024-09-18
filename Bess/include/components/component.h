#pragma once
#include "components_manager/component_type.h"
#include "ext/vector_float2.hpp"
#include "ext/vector_float3.hpp"

#include "uuid.h"

#include <any>
#include <functional>
#include <unordered_map>

namespace Bess::Simulator::Components {

    enum class ComponentEventType {
        none = -1,
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

    typedef std::function<void(const glm::vec2 &pos)> OnLeftClickCB;
    typedef std::function<void(const glm::vec2 &pos)> OnRightClickCB;
    typedef std::function<void()> VoidCB;

    class Component {
      public:
        Component() = default;
        Component(const uuids::uuid &uid, int renderId, glm::vec3 position, ComponentType type);
        virtual ~Component() = default;

        uuids::uuid getId() const;
        std::string getIdStr() const;

        int getRenderId() const;
        glm::vec3 &getPosition();

        ComponentType getType() const;

        void onEvent(ComponentEventData e);

        virtual void render() = 0;

        virtual void update();

        virtual void generate(const glm::vec3 &pos) = 0;

        virtual void deleteComponent() = 0;

        virtual std::string getName() const;
        virtual std::string getRenderName() const;

        virtual void drawProperties();

        virtual void simulate();

      protected:
        int m_renderId{};
        uuids::uuid m_uid;
        glm::vec3 m_position{};
        ComponentType m_type = ComponentType::none;
        std::string m_name = "Unknown";
        std::unordered_map<ComponentEventType, std::any> m_events = {};
        bool m_isSelected = false;
        bool m_isHovered = false;
    };
} // namespace Bess::Simulator::Components
