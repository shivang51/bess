#pragma once

#include "common/bess_api.h"

#include "bess_core/scene/scene_draw_context.h"
#include "bess_core/scene/scene_events.h"
#include "bess_core/scene/scene_state/components/scene_component.h"
#include "bess_core/scene/scene_ui/controls/container_comp.h"
#include "bess_core/scene/scene_ui/controls/editable_label_comp.h"
#include "bess_core/scene/scene_ui/controls/label_comp.h"
#include "bess_core/scene/scene_ui/controls/text_box_comp.h"
#include "bess_core/scene/scene_ui/layout.h"
#include "common/bess_uuid.h"
#include "common/types.h"
#include "fwd.hpp"
#include "scene_comp_types.h"

#define SLOT_SC_SER_PROPS                                                      \
    ("portDirection", getPortDirection, setPortDirection),                     \
        ("signalKind", getSignalKind, setSignalKind),                          \
        ("resizeTrigger", getResizeTrigger, setResizeTrigger),                 \
        ("index", getIndex, setIndex),                                         \
        ("schematicPos", getSchematicPos, setSchematicPos),                    \
        ("connectedConnections",                                               \
         getConnectedConnections,                                              \
         setConnectedConnections)

namespace Bess::Canvas {
    class BESS_API SlotSceneComponent : public SceneComponent {
      public:
        SlotSceneComponent() = default;
        SlotSceneComponent(const SlotSceneComponent &other) = default;
        ~SlotSceneComponent() override = default;

        void update(TimeMs frameTime, SceneState &state) override;
        void draw(SceneDrawContext &drawContext) override;

        void drawSchematic(SceneDrawContext &drawContext) override;

        bool onMouseEnter(const Events::MouseEnterEvent &e) override;
        bool onMouseLeave(const Events::MouseLeaveEvent &e) override;
        Core::Viewport::SceneCursor getCursor() const override;

        bool onMouseButton(const Events::MouseButtonEvent &e) override;

        void prepareUI(SceneUIPrepareCtx &ctx) override;

        std::vector<std::shared_ptr<SceneComponent>>
        clone(const SceneState &sceneState) const override;

        REG_SCENE_COMP_TYPE("SlotSceneComponent", SceneComponentType::slot)
        SCENE_COMP_SER(Bess::Canvas::SlotSceneComponent,
                       Bess::Canvas::SceneComponent,
                       SLOT_SC_SER_PROPS)

        MAKE_GETTER_SETTER(SimEngine::PortDirection,
                           PortDirection,
                           m_portDirection)
        MAKE_GETTER_SETTER(SimEngine::SignalKind, SignalKind, m_signalKind)
        MAKE_GETTER_SETTER(bool, ResizeTrigger, m_resizeTrigger)
        MAKE_GETTER_SETTER(int, Index, m_index)
        MAKE_GETTER_SETTER(glm::vec3, SchematicPos, m_schematicPos)
        MAKE_GETTER_SETTER(std::vector<UUID>,
                           ConnectedConnections,
                           m_connectedConnections)

        void addConnection(const UUID &connectionId);
        void removeConnection(const UUID &connectionId);

        glm::vec3 getAbsolutePosition(const SceneState &state,
                                      bool isSchematicMode) const override;

        glm::vec3 getConnectionPos(const SceneState &state,
                                   bool isSchematicMode) const;

        SimEngine::PortRef getPortRef(const SceneState &state) const;

        SimEngine::PortState getSlotState(const SceneState &state) const;
        // Use for draw hot paths
        SimEngine::PortState getSlotState(const SceneDrawContext &ctx) const;
        bool isSlotConnected(const SceneState &state) const;

        // use for draw hot paths
        bool isSlotConnected(const SceneDrawContext &ctx) const;

        bool isResizeSlot() const;

        bool isInputSlot() const;

        std::vector<UUID> getDependants(const SceneState &state) const override;

        std::vector<UUID> cleanup(SceneState &state,
                                  UUID caller = UUID::null) override;

        void onNameChanged() override;

      private:
        void onRuntimeIdChanged() override;

        void resetCloneRuntimeState() override;

        glm::vec3 getSchematicPosAbsolute(const SceneState &state,
                                          bool isSchematicMode) const;

        bool onMouseLeftClick(const Events::MouseButtonEvent &e);
        void setSlotLayoutDirty();

      private:
        glm::vec3 m_schematicPos = glm::vec3(0.f);
        SimEngine::PortDirection m_portDirection =
            SimEngine::PortDirection::none;
        SimEngine::SignalKind m_signalKind = SimEngine::SignalKind::none;
        bool m_resizeTrigger = false;
        std::vector<UUID> m_connectedConnections;
        bool m_isHovered = false;

        bool m_invalidateCache = false;
        int m_index = -1;

        std::shared_ptr<UI::ContainerComp> m_container = nullptr;
        std::shared_ptr<UI::EditableLabelComp> m_label = nullptr;
        std::shared_ptr<UI::TextBoxComp> m_scalarValueTextBox = nullptr;

        UI::UINode *m_slotNode = nullptr;
    };

} // namespace Bess::Canvas

REG_SCENE_COMP(Bess::Canvas::SlotSceneComponent,
               Bess::Canvas::SceneComponent,
               SLOT_SC_SER_PROPS)
