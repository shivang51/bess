#pragma once

#include "bess_uuid.h"
#include "events/scene_events.h"
#include "scene/renderer/material_renderer.h"
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

        void addConnection(const UUID &connectionId);
        void removeConnection(const UUID &connectionId);

        glm::vec3 getSchematicPosAbsolute(const SceneState &state) const;

        SimEngine::SlotState getSlotState(const SceneState &state) const;
        SimEngine::SlotState getSlotState(const SceneState *state) const;
        bool isSlotConnected(const SceneState &state) const;

        std::vector<UUID> cleanup(SceneState &state, UUID caller = UUID::null) override;

        bool isResizeSlot() const;

      private:
        glm::vec3 m_schematicPos = glm::vec3(0.f);
        SlotType m_slotType = SlotType::none;
        std::vector<UUID> m_connectedConnections;
        int m_index = -1;
    };

} // namespace Bess::Canvas

namespace Bess::JsonConvert {
    void toJsonValue(const Bess::Canvas::SlotSceneComponent &component, Json::Value &j);
    void fromJsonValue(const Json::Value &j, Bess::Canvas::SlotSceneComponent &component);
} // namespace Bess::JsonConvert
