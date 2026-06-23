#pragma once

#include "common/class_helpers.h"
#include "common/types.h"
#include "pages/main_page/scene_components/non_sim_scene_component.h"

#define MONITOR_SCENE_COMP_SER_PROPS                                           \
    ("probedSlots", getProbedSlots, setProbedSlots)

namespace Bess::Canvas {
    class MonitorSceneComp : public NonSimSceneComponent {
      public:
        MonitorSceneComp();
        ~MonitorSceneComp() override = default;

        REG_SCENE_COMP_TYPE("MonitorSceneComp",
                            SceneComponentType::nonSimulation)

        std::vector<std::shared_ptr<SceneComponent>>
        clone(const SceneState &sceneState) const override;

        void draw(SceneDrawContext &context) override;

        void update(TimeMs frameTime, SceneState &state) override;

        bool onMouseButton(const Events::MouseButtonEvent &e) override;
        bool onMouseWheel(const Events::MouseWheelEvent &e) override;

        void addSlotProbe(const SceneState &sceneState, const UUID &slotUuid);

        void removeSlotProbe(const SceneState &sceneState,
                             const UUID &slotUuid);

        MAKE_GETTER_SETTER(OrderedSet<UUID>, ProbedSlots, m_probedSlots)

        SCENE_COMP_SER(MonitorSceneComp,
                       NonSimSceneComponent,
                       MONITOR_SCENE_COMP_SER_PROPS)

      private:
        void subscribeToSlot(const SceneState &sceneState,
                             const UUID &slotUuid);
        void unsubscribeFromSlot(const SceneState &sceneState,
                                 const UUID &slotUuid);

        void plotProbedData(SceneDrawContext &context);

      private:
        OrderedSet<UUID> m_probedSlots;
        HashMap<UUID, std::vector<std::pair<TimeNs, float>>> m_probeData;
        float m_scale = 10.f;
    };

} // namespace Bess::Canvas
