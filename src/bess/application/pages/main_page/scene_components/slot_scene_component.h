#pragma once

#include "common/bess_uuid.h"
#include "fwd.hpp"
#include "scene/renderer/material_renderer.h"
#include "scene/scene_events.h"
#include "scene/scene_state/components/scene_component.h"
#include "scene_comp_types.h"
#include "types.h"

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
#define SLOT_SC_SER_PROPS ("slotType", getSlotType, setSlotType),             \
                          ("index", getIndex, setIndex),                      \
                          ("schematicPos", getSchematicPos, setSchematicPos), \
                          ("connectedConnections", getConnectedConnections, setConnectedConnections)

namespace Bess::Canvas {
    class SlotSceneComponent : public SceneComponent {
      public:
        SlotSceneComponent() = default;
        SlotSceneComponent(const SlotSceneComponent &other) = default;
        ~SlotSceneComponent() override = default;

        void draw(SceneState &state,
                  std::shared_ptr<Renderer::MaterialRenderer> materialRenderer,
                  std::shared_ptr<Renderer2D::Vulkan::PathRenderer> pathRenderer) override;

        void drawSchematic(SceneState &state,
                           std::shared_ptr<Renderer::MaterialRenderer> materialRenderer,
                           std::shared_ptr<Renderer2D::Vulkan::PathRenderer> pathRenderer) override;

        void onMouseEnter(const Events::MouseEnterEvent &e) override;
        void onMouseLeave(const Events::MouseLeaveEvent &e) override;

        void onMouseButton(const Events::MouseButtonEvent &e) override;

        REG_SCENE_COMP_TYPE("SlotSceneComponent", SceneComponentType::slot)
        SCENE_COMP_SER(Bess::Canvas::SlotSceneComponent, Bess::Canvas::SceneComponent, SLOT_SC_SER_PROPS)

        MAKE_GETTER_SETTER(SlotType, SlotType, m_slotType)
        MAKE_GETTER_SETTER(int, Index, m_index)
        MAKE_GETTER_SETTER(glm::vec3, SchematicPos, m_schematicPos)
        MAKE_GETTER_SETTER(std::vector<UUID>, ConnectedConnections, m_connectedConnections)

        void addConnection(const UUID &connectionId);
        void removeConnection(const UUID &connectionId);

        glm::vec3 getAbsolutePosition(const SceneState &state) const override;

        glm::vec3 getConnectionPos(const SceneState &state) const;

        SimEngine::SlotState getSlotState(const SceneState &state) const;
        bool isSlotConnected(const SceneState &state) const;

        bool isResizeSlot() const;

        bool isInputSlot() const;

      private:
        void onRuntimeIdChanged() override;

        glm::vec3 getSchematicPosAbsolute(const SceneState &state) const;

        void onMouseLeftClick(const Events::MouseButtonEvent &e);

      private:
        glm::vec3 m_schematicPos = glm::vec3(0.f);
        SlotType m_slotType = SlotType::none;
        std::vector<UUID> m_connectedConnections;

        bool m_invalidateCache = false;
        int m_index = -1;
    };

} // namespace Bess::Canvas

REG_SCENE_COMP(Bess::Canvas::SlotSceneComponent, Bess::Canvas::SceneComponent, SLOT_SC_SER_PROPS)
