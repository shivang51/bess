#pragma once

#include "bess_uuid.h"
#include "fwd.hpp"
#include "scene/renderer/material_renderer.h"
#include "scene/scene_events.h"
#include "scene/scene_state/components/scene_component.h"
#include "types.h"

namespace Bess::Canvas {
    enum class SlotType : uint8_t {
        none,
        digitalInput,
        digitalOutput,
        inputsResize,
        outputsResize,
    };

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

        REG_SCENE_COMP(SceneComponentType::slot)

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

        std::vector<UUID> cleanup(SceneState &state, UUID caller = UUID::null) override;

        bool isResizeSlot() const;

        bool isInputSlot() const;

      private:
        glm::vec3 getSchematicPosAbsolute(const SceneState &state) const;

        void onMouseLeftClick(const Events::MouseButtonEvent &e);

      private:
        glm::vec3 m_schematicPos = glm::vec3(0.f);
        SlotType m_slotType = SlotType::none;
        std::vector<UUID> m_connectedConnections;
        int m_index = -1;
    };

} // namespace Bess::Canvas

REFLECT_ENUM(Bess::Canvas::SlotType);

REFLECT_DERIVED_PROPS(Bess::Canvas::SlotSceneComponent, Bess::Canvas::SceneComponent,
                      ("slotType", getSlotType, setSlotType),
                      ("index", getIndex, setIndex),
                      ("schematicPos", getSchematicPos, setSchematicPos),
                      ("connectedConnections", getConnectedConnections, setConnectedConnections));
