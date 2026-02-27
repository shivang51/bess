#pragma once
#include "bess_json/bess_json.h"
#include "common/bess_uuid.h"
#include "pages/main_page/scene_components/proxy_slot_component.h"
#include "scene/scene_state/components/behaviours/drag_behaviour.h"
#include "scene/scene_state/components/scene_component.h"
#include "scene_comp_types.h"
#include "types.h"

#define CONNJOINT_SC_SER_PROPS ("connSegIdx", getConnSegIdx, setConnSegIdx),                \
                               ("schConnSegIdx", getSchConnSegIdx, setSchConnSegIdx),       \
                               ("connectionId", getConnectionId, setConnectionId),          \
                               ("outputSlotId", getOutputSlotId, setOutputSlotId),          \
                               ("offset", getSegOffset, setSegOffset),                      \
                               ("schematicOffset", getSchematicOffset, setSchematicOffset), \
                               ("segOrientation", getSegOrientation, setSegOrientation),    \
                               ("connections", getConnections, setConnections)

namespace Bess::Canvas {

    class ConnJointSceneComp : public SceneComponent,
                               public DragBehaviour<ConnJointSceneComp>,
                               public ProxySlotComponent {
      public:
        ConnJointSceneComp() = default;
        ConnJointSceneComp(UUID connectionId, int connSegIdx, ConnSegOrientaion segOrientation);

        REG_SCENE_COMP_TYPE("ConnJointSceneComp", SceneComponentType::connJoint)
        SCENE_COMP_SER(Bess::Canvas::ConnJointSceneComp, Bess::Canvas::SceneComponent, CONNJOINT_SC_SER_PROPS)

        MAKE_GETTER_SETTER(int, ConnSegIdx, m_connSegIdx);
        MAKE_GETTER_SETTER(int, SchConnSegIdx, m_schConnSegIdx);
        MAKE_GETTER_SETTER(UUID, ConnectionId, m_connectionId);
        MAKE_GETTER_SETTER(float, SegOffset, m_offset);
        MAKE_GETTER_SETTER(ConnSegOrientaion, SegOrientation, m_segOrientation);
        MAKE_GETTER_SETTER(std::vector<UUID>, Connections, m_connections);
        MAKE_GETTER_SETTER(float, SchematicOffset, m_schematicOffset);

        // Connect this joint to another slot
        bool connectWith(SceneState &sceneState, const UUID &slotId);

        void addConnection(const UUID &connectionId);

        void draw(SceneState &state,
                  std::shared_ptr<Renderer::MaterialRenderer> materialRenderer,
                  std::shared_ptr<Renderer2D::Vulkan::PathRenderer> pathRenderer) override;

        void drawSchematic(SceneState &state,
                           std::shared_ptr<Renderer::MaterialRenderer> materialRenderer,
                           std::shared_ptr<Renderer2D::Vulkan::PathRenderer> pathRenderer) override;

        SimEngine::SlotState getSlotState(const SceneState &state) const;
        void onMouseDragged(const Events::MouseDraggedEvent &e) override;
        void onMouseEnter(const Events::MouseEnterEvent &e) override;
        void onMouseLeave(const Events::MouseLeaveEvent &e) override;
        void onMouseButton(const Events::MouseButtonEvent &e) override;

        glm::vec3 getAbsolutePosition(const SceneState &state) const override;
        void onMouseLeftClick(const Events::MouseButtonEvent &e);

        void removeConnection(const UUID &connectionId);
        std::vector<UUID> cleanup(SceneState &state, UUID caller = UUID::null) override;

        std::vector<UUID> getDependants(const SceneState &state) const override;

      private:
        int m_connSegIdx, m_schConnSegIdx;
        UUID m_connectionId;

        bool m_isHovered = false;

        float m_offset = -1.f, m_schematicOffset = -1.f; // normalized 0-1 offset, signifying pos on segment
        ConnSegOrientaion m_segOrientation = ConnSegOrientaion::horizontal;
        std::vector<UUID> m_connections;
    };
} // namespace Bess::Canvas

REG_SCENE_COMP(Bess::Canvas::ConnJointSceneComp, Bess::Canvas::SceneComponent, CONNJOINT_SC_SER_PROPS)
