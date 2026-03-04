#pragma once
#include "scene/scene_state/components/scene_component.h"
#include "scene_comp_types.h"

namespace Bess::Canvas {
    class GroupSceneComponent : public SceneComponent {
      public:
        GroupSceneComponent() = default;
        GroupSceneComponent(const GroupSceneComponent &other) = default;
        ~GroupSceneComponent() override = default;

        static std::shared_ptr<GroupSceneComponent> create(const std::string &name) {
            auto comp = std::make_shared<GroupSceneComponent>();
            comp->setName(name);
            return comp;
        }

        REG_SCENE_COMP_TYPE("GroupSceneComponent", SceneComponentType::group)
        SCENE_COMP_SER_NP(Bess::Canvas::GroupSceneComponent, Bess::Canvas::SceneComponent)

        void onAttach(SceneState &state) override;
        void onSelect() override;

        std::vector<UUID> cleanup(SceneState &state, UUID caller = UUID::null) override;
    };
} // namespace Bess::Canvas

REG_SCENE_COMP_NP(Bess::Canvas::GroupSceneComponent, Bess::Canvas::SceneComponent)
