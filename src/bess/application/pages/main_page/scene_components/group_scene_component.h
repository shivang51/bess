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

        REG_SCENE_COMP_TYPE(SceneComponentType::group)

        MAKE_GETTER_SETTER(bool, IsExpanded, m_isExpanded)

        void onAttach(SceneState &state) override;
        void onSelect() override;

        std::vector<UUID> cleanup(SceneState &state, UUID caller = UUID::null) override;

      private:
        bool m_isExpanded = true; // For ProjectExplorer UI
    };
} // namespace Bess::Canvas
