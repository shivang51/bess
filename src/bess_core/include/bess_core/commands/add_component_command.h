#pragma once

#include "common/bess_api.h"

#include "bess_core/commands/command.h"
#include "bess_core/commands/scene_component_command_hooks.h"
#include "bess_core/scene/scene.h"
#include "bess_core/scene/scene_state/components/scene_component.h"
#include "common/bess_assert.h"
#include "common/logger.h"
#include <memory>
#include <type_traits>
#include <vector>

namespace Bess::Cmd {
    class BESS_API AddSceneComponentCmd : public Command {
      public:
        AddSceneComponentCmd();

        explicit AddSceneComponentCmd(
            std::shared_ptr<Canvas::SceneComponent> component);

        AddSceneComponentCmd(
            std::shared_ptr<Canvas::SceneComponent> component,
            std::vector<std::shared_ptr<Canvas::SceneComponent>>
                childComponents);

        bool execute(const CommandContext &context) override;
        void undo(const CommandContext &context) override;
        void redo(const CommandContext &context) override;

        const std::shared_ptr<Canvas::SceneComponent> &getComponent() const;

      private:
        bool addToScene(const CommandContext &context, bool setZ);

        std::shared_ptr<Canvas::SceneComponent> m_component = nullptr;
        std::vector<std::shared_ptr<Canvas::SceneComponent>> m_childComponents;
    };

    template <typename TComponent>
        requires std::is_base_of_v<Canvas::SceneComponent, TComponent>
    class BESS_API AddCompCmd : public AddSceneComponentCmd {
      public:
        AddCompCmd() = default;

        explicit AddCompCmd(std::shared_ptr<TComponent> component)
            : AddSceneComponentCmd(std::move(component)) {
        }

        AddCompCmd(std::shared_ptr<TComponent> component,
                   std::vector<std::shared_ptr<Canvas::SceneComponent>>
                       childComponents)
            : AddSceneComponentCmd(std::move(component),
                                   std::move(childComponents)) {
        }
    };
} // namespace Bess::Cmd
