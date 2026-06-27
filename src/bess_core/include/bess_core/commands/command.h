#pragma once

#include "bess_core/commands/scene_component_command_hooks.h"
#include "common/bess_api.h"
#include <memory>
#include <string>

namespace Bess::Canvas {
    class Scene;
}

namespace Bess::Cmd {
    struct CommandContext {
        std::shared_ptr<Canvas::Scene> scene = nullptr;
        std::shared_ptr<const SceneComponentCommandHooks> componentHooks =
            defaultSceneComponentCommandHooks();
    };

    class BESS_API Command {
      public:
        Command() = default;
        virtual ~Command() = default;

        virtual bool execute(const CommandContext &context) = 0;
        virtual void undo(const CommandContext &context) = 0;
        virtual void redo(const CommandContext &context) = 0;

        virtual bool canMergeWith(const Command *other) const;
        virtual bool mergeWith(const Command *other);
        virtual std::string getName() const;

        void setSceneContext(const std::shared_ptr<Canvas::Scene> &scene);
        std::shared_ptr<Canvas::Scene> getSceneContext() const;
        bool hasSceneContext() const;
        bool sharesSceneContextWith(const Command *other) const;

        void setComponentHooks(
            std::shared_ptr<const SceneComponentCommandHooks> hooks);
        std::shared_ptr<const SceneComponentCommandHooks>
        getComponentHooks() const;
        bool hasComponentHooks() const;

        CommandContext makeContext(const CommandContext &fallback) const;

      protected:
        std::shared_ptr<Canvas::Scene> m_sceneContext = nullptr;
        std::shared_ptr<const SceneComponentCommandHooks> m_componentHooks =
            nullptr;
        std::string m_name;
    };
} // namespace Bess::Cmd
