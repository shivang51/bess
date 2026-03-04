#pragma once
#include "common/bess_uuid.h"
#include "pages/main_page/scene_components/non_sim_scene_component.h"
#include "pages/main_page/scene_components/scene_comp_types.h"
#include "scene_state/components/scene_component.h"
#include "types.h"

#define SLOT_PROBE_SER_PROPS ("probedSlot", getProbedSlotUuid, setProbedSlotUuid)

namespace Bess::Canvas {
    class SceneState;

    class SlotProbeSceneComponent : public NonSimSceneComponent {
      public:
        SlotProbeSceneComponent() = default;
        ~SlotProbeSceneComponent() override = default;

        REG_SCENE_COMP_TYPE("SlotProbeSceneComponent",
                            SceneComponentType::nonSimulation)

        MAKE_GETTER_SETTER_WC(UUID, ProbedSlotUuid, m_probedSlotUuid, onProbedSlotChanged)
        typedef std::pair<TimeNs, SimEngine::LogicState> ProbeDataEntry;
        MAKE_GETTER_SETTER(std::vector<ProbeDataEntry>, ProbeData, m_probeData)

        SCENE_COMP_SER(SlotProbeSceneComponent, NonSimSceneComponent, SLOT_PROBE_SER_PROPS)

        void draw(SceneState &sceneState,
                  std::shared_ptr<Renderer::MaterialRenderer> materialRenderer,
                  std::shared_ptr<PathRenderer> pathRenderer) override;

        void update(TimeMs frameTime, SceneState &state) override;

        void onAttach(SceneState &state) override;
        std::vector<UUID> cleanup(SceneState &state, UUID caller = UUID::null) override;

        void onMouseButton(const Events::MouseButtonEvent &e) override;

        std::type_index getTypeIndex() override {
            return typeid(SlotProbeSceneComponent);
        }

      private:
        void onProbedSlotChanged();

      private:
        UUID m_probedSlotUuid = UUID::null;
        std::vector<std::pair<TimeNs, SimEngine::LogicState>> m_probeData;
    };
} // namespace Bess::Canvas

REG_SCENE_COMP(Bess::Canvas::SlotProbeSceneComponent,
               Bess::Canvas::SceneComponent,
               SLOT_PROBE_SER_PROPS)
