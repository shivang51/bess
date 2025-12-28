#pragma once

#include "events/scene_events.h"
#include "scene/renderer/material_renderer.h"
#include "scene/scene_state/components/scene_component.h"

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
        SlotSceneComponent(UUID uuid);

        SlotSceneComponent(UUID uuid, const Transform &transform);
        ~SlotSceneComponent() override = default;

        void draw(SceneState &state,
                  std::shared_ptr<Renderer::MaterialRenderer> materialRenderer,
                  std::shared_ptr<Renderer2D::Vulkan::PathRenderer> pathRenderer) override;

        void onMouseEnter(const Events::MouseEnterEvent &e) override;
        void onMouseLeave(const Events::MouseLeaveEvent &e) override;

        void onMouseButton(const Events::MouseButtonEvent &e) override;

        REG_SCENE_COMP(SceneComponentType::slot)

        MAKE_GETTER_SETTER(SlotType, SlotType, m_slotType)
        MAKE_GETTER_SETTER(UUID, SimEngineId, m_simEngineId)
        MAKE_GETTER_SETTER(int, Index, m_index)

        SimEngine::SlotState getSlotState(const SceneState &state) const;
        SimEngine::SlotState getSlotState(const SceneState *state) const;
        bool isSlotConnected(const SceneState &state) const;

      private:
        SlotType m_slotType = SlotType::none;
        UUID m_simEngineId = UUID::null;
        int m_index = -1;
    };

} // namespace Bess::Canvas

namespace Bess::JsonConvert {
    void toJsonValue(const Bess::Canvas::SlotSceneComponent &component, Json::Value &j);
    void fromJsonValue(const Json::Value &j, Bess::Canvas::SlotSceneComponent &component);
} // namespace Bess::JsonConvert
