#pragma once

#include "bess_core/commands/command.h"
#include "common/sub_system.h"
#include <memory>
#include <stack>

namespace Bess::Canvas {
    class Scene;
}

namespace Bess::Cmd {
    class BESS_API CommandSystem : public ISubSystem {
      public:
        CommandSystem() = default;

        void onInit() override;
        void onShutdown() override;
        void onDestroy() override;

        void execute(std::unique_ptr<Command> cmd);

        // For commands that have already mutated state and only need history.
        void push(std::unique_ptr<Command> cmd, bool tryMerge = true);

        void undo();
        void redo();
        void reset();

        bool canUndo() const;
        bool canRedo() const;

        std::shared_ptr<Canvas::Scene> getInternalScene() const;

        void setScene(const std::shared_ptr<Canvas::Scene> &scene);

        void setSceneComponentHooks(
            std::shared_ptr<const SceneComponentCommandHooks> hooks);
        std::shared_ptr<const SceneComponentCommandHooks>
        getSceneComponentHooks() const;

      private:
        CommandContext getContext(Command *cmd) const;
        std::shared_ptr<Canvas::Scene> getScene() const;

      private:
        std::stack<std::unique_ptr<Command>> m_undoStack;
        std::stack<std::unique_ptr<Command>> m_redoStack;

        std::shared_ptr<Canvas::Scene> m_scene = nullptr;
        std::shared_ptr<const SceneComponentCommandHooks> m_componentHooks =
            defaultSceneComponentCommandHooks();
    };
} // namespace Bess::Cmd
