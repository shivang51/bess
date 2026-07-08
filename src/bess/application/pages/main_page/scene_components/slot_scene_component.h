#pragma once

#include "bess_core/scene/scene_draw_context.h"
#include "bess_core/scene/scene_events.h"
#include "bess_core/scene/scene_state/components/scene_component.h"
#include "bess_core/scene/scene_ui/controls/container_comp.h"
#include "bess_core/scene/scene_ui/controls/label_comp.h"
#include "bess_core/scene/scene_ui/layout.h"
#include "common/bess_uuid.h"
#include "fwd.hpp"
#include "scene_comp_types.h"

namespace Bess::Canvas {
    enum class SlotType : uint8_t {
        none,
        digitalInput,
        digitalOutput,
        inputsResize,
        outputsResize,
    };
}

REFLECT_ENUM(Bess::Canvas::SlotType);
#define SLOT_SC_SER_PROPS                                                      \
    ("slotType", getSlotType, setSlotType), ("index", getIndex, setIndex),     \
        ("schematicPos", getSchematicPos, setSchematicPos),                    \
        ("connectedConnections",                                               \
         getConnectedConnections,                                              \
         setConnectedConnections)

namespace Bess::Canvas {
    class SlotSceneComponent : public SceneComponent {
      public:
        SlotSceneComponent() = default;
        SlotSceneComponent(const SlotSceneComponent &other) = default;
        ~SlotSceneComponent() override = default;

        void update(TimeMs frameTime, SceneState &state) override;
        void draw(SceneDrawContext &drawContext) override;

        void drawSchematic(SceneDrawContext &drawContext) override;

        bool onMouseEnter(const Events::MouseEnterEvent &e) override;
        bool onMouseLeave(const Events::MouseLeaveEvent &e) override;

        bool onMouseButton(const Events::MouseButtonEvent &e) override;

        void prepareUI(SceneUIPrepareCtx &ctx) override;

        std::vector<std::shared_ptr<SceneComponent>>
        clone(const SceneState &sceneState) const override;

        REG_SCENE_COMP_TYPE("SlotSceneComponent", SceneComponentType::slot)
        SCENE_COMP_SER(Bess::Canvas::SlotSceneComponent,
                       Bess::Canvas::SceneComponent,
                       SLOT_SC_SER_PROPS)

        MAKE_GETTER_SETTER(SlotType, SlotType, m_slotType)
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

        SimEngine::SlotState getSlotState(const SceneState &state) const;
        bool isSlotConnected(const SceneState &state) const;

        bool isResizeSlot() const;

        bool isInputSlot() const;

        std::vector<UUID> getDependants(const SceneState &state) const override;

        void onNameChanged() override;

      private:
        void onRuntimeIdChanged() override;

        void resetCloneRuntimeState() override;

        glm::vec3 getSchematicPosAbsolute(const SceneState &state,
                                          bool isSchematicMode) const;

        bool onMouseLeftClick(const Events::MouseButtonEvent &e);

      private:
        glm::vec3 m_schematicPos = glm::vec3(0.f);
        SlotType m_slotType = SlotType::none;
        std::vector<UUID> m_connectedConnections;
        bool m_isHovered = false;

        bool m_invalidateCache = false;
        int m_index = -1;

        std::shared_ptr<UI::ContainerComp> m_container = nullptr;
        std::shared_ptr<UI::LabelComp> m_label = nullptr;

        UI::UINode *m_slotNode = nullptr;
    };

} // namespace Bess::Canvas

REG_SCENE_COMP(Bess::Canvas::SlotSceneComponent,
               Bess::Canvas::SceneComponent,
               SLOT_SC_SER_PROPS)
