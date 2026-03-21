#pragma once
#include "common/bess_uuid.h"
#include "pages/main_page/scene_components/sim_scene_component.h"
#include "scene/scene_state/components/scene_component.h"
#include "scene_comp_types.h"

#define MODULE_SER_PROPS ("sceneId", getSceneId, setSceneId),                   \
                         ("associatedInp", getAssociatedInp, setAssociatedInp), \
                         ("associatedOut", getAssociatedOut, setAssociatedOut)

namespace Bess::Canvas {
    class ModuleSceneComponent : public SimulationSceneComponent {
      public:
        ModuleSceneComponent();
        ModuleSceneComponent(const ModuleSceneComponent &other) = default;

        static std::shared_ptr<ModuleSceneComponent> fromNet(const UUID &netId,
                                                             const std::string &name = "New Module");

        REG_SCENE_COMP_TYPE("ModuleSceneComponent", SceneComponentType::module)
        SCENE_COMP_SER(Bess::Canvas::ModuleSceneComponent,
                       Bess::Canvas::SimulationSceneComponent,
                       MODULE_SER_PROPS)

        std::vector<std::shared_ptr<SceneComponent>> clone(const SceneState &sceneState) const override;

        void onMouseButton(const Events::MouseButtonEvent &e) override;

        void onAttach(SceneState &state) override;
        std::vector<UUID> cleanup(SceneState &state, UUID caller = UUID::null) override;

        MAKE_GETTER_SETTER(UUID, SceneId, m_sceneId)
        MAKE_GETTER_SETTER(UUID, AssociatedInp, m_associatedInp)
        MAKE_GETTER_SETTER(UUID, AssociatedOut, m_associatedOut)

      private:
        UUID m_sceneId = UUID::null;
        UUID m_associatedInp = UUID::null, m_associatedOut = UUID::null;

        bool m_isScaleDirty = true;
    };
} // namespace Bess::Canvas

REG_SCENE_COMP(Bess::Canvas::ModuleSceneComponent,
               Bess::Canvas::SimulationSceneComponent,
               MODULE_SER_PROPS)
