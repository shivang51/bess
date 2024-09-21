#pragma once
#include "components_manager/component_type.h"
#include "ext/vector_float2.hpp"
#include "ext/vector_float3.hpp"
#include "scene/transform/transform_2d.h"

#include "uuid.h"

#include <any>
#include <functional>
#include <queue>
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
        dragStart,
        drag,
        dragEnd,
    };

    struct ComponentEventData {
        ComponentEventType type;
        glm::vec2 pos;
    };

    typedef std::function<void(const glm::vec2 &pos)> OnLeftClickCB;
    typedef std::function<void(const glm::vec2 &pos)> OnRightClickCB;
    typedef std::function<void(const glm::vec2 &pos)> Vec2CB;
    typedef std::function<void()> VoidCB;

    class Component {
      public:
        Component() = default;
        Component(const uuids::uuid &uid, int renderId, glm::vec3 position, ComponentType type);
        virtual ~Component() = default;

        uuids::uuid getId() const;
        std::string getIdStr() const;

        int getRenderId() const;
        const glm::vec3 &getPosition();
        void setPosition(const glm::vec3 &pos);

        ComponentType
        getType() const;

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
        ComponentType m_type = ComponentType::none;
        std::string m_name = "Unknown";
        std::unordered_map<ComponentEventType, std::any> m_events = {};
        bool m_isSelected = false;
        bool m_isHovered = false;
        Scene::Transform::Transform2D m_transform{};

      private:
        std::queue<ComponentEventData> m_eventsQueue = {};
    };
} // namespace Bess::Simulator::Components
