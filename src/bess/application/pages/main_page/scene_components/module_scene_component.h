#pragma once
#include "common/bess_uuid.h"
#include "scene/scene_state/components/scene_component.h"
#include "scene_comp_types.h"

namespace Bess::Canvas {
    class ModuleSceneComponent : public SceneComponent {
      public:
        ModuleSceneComponent() = default;
        ModuleSceneComponent(const ModuleSceneComponent &other) = default;
        ~ModuleSceneComponent() override = default;

        static std::shared_ptr<ModuleSceneComponent> create(const std::string &name) {
            auto comp = std::make_shared<ModuleSceneComponent>();
            comp->setName(name);
            return comp;
        }

        REG_SCENE_COMP_TYPE("ModuleSceneComponent", SceneComponentType::module)
        SCENE_COMP_SER_NP(Bess::Canvas::ModuleSceneComponent, Bess::Canvas::SceneComponent)

        void onAttach(SceneState &state) override;
        void onSelect() override;

        std::vector<UUID> cleanup(SceneState &state, UUID caller = UUID::null) override;

      private:
        UUID sceneId = UUID::null;
    };
} // namespace Bess::Canvas

REG_SCENE_COMP_NP(Bess::Canvas::ModuleSceneComponent, Bess::Canvas::SceneComponent)
