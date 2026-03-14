#pragma once
#include "common/bess_uuid.h"
#include "scene/scene_state/components/scene_component.h"
#include "scene_comp_types.h"
#include "scene_state/components/behaviours/drag_behaviour.h"

namespace Bess::Canvas {
    class ModuleSceneComponent : public SceneComponent,
                                 public DragBehaviour<ModuleSceneComponent> {
      public:
        ModuleSceneComponent() = default;
        ModuleSceneComponent(const ModuleSceneComponent &other) = default;
        ~ModuleSceneComponent() override = default;

        static std::shared_ptr<ModuleSceneComponent> fromNet(const UUID &netId,
                                                             const std::string &name = "New Module");

        REG_SCENE_COMP_TYPE("ModuleSceneComponent", SceneComponentType::module)
        SCENE_COMP_SER_NP(Bess::Canvas::ModuleSceneComponent, Bess::Canvas::SceneComponent)

        void draw(SceneDrawContext &context) override;
        void update(TimeMs frameTime, SceneState &state) override;

        void onAttach(SceneState &state) override;
        void onSelect() override;

        std::vector<UUID> cleanup(SceneState &state, UUID caller = UUID::null) override;

        MAKE_GETTER_SETTER(UUID, SceneId, m_sceneId)
        MAKE_GETTER_SETTER(UUID, AssociatedInp, m_associatedInp)
        MAKE_GETTER_SETTER(UUID, AssociatedOut, m_associatedOut)

      private:
        glm::vec2 calculateScale(SceneState &state) override;
        void drawBackground(SceneDrawContext &context);

      private:
        UUID m_sceneId = UUID::null;
        UUID m_associatedInp = UUID::null, m_associatedOut = UUID::null;

        bool m_isScaleDirty = true;
    };
} // namespace Bess::Canvas

REG_SCENE_COMP_NP(Bess::Canvas::ModuleSceneComponent, Bess::Canvas::SceneComponent)
