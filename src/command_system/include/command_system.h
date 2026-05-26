#pragma once

#include "command.h"
#include "common/sub_system.h"
#include <memory>
#include <stack>

namespace Bess::Canvas {
    class Scene;
}

namespace Bess::SimEngine {
    class SimulationEngine;
}

namespace Bess::Cmd {
    class CommandSystem : public ISubSystem {
      public:
        CommandSystem() = default;

        void onInit() override;
        void onDestroy() override;

        void execute(std::unique_ptr<Command> cmd);

        // For commands that are already executed,
        // just want to push to undo stack without executing again
        void push(std::unique_ptr<Command> cmd, bool tryMerge = true);

        void undo();
        void redo();

        void reset();

      public:
        bool canUndo() const;
        bool canRedo() const;

        std::shared_ptr<Canvas::Scene> getInternalScene() const;

        void setScene(const std::shared_ptr<Canvas::Scene> &scene);
        void setSimEngine(
            const std::shared_ptr<SimEngine::SimulationEngine> &simEngine);

      private:
        std::shared_ptr<Canvas::Scene> getScene() const;
        std::shared_ptr<SimEngine::SimulationEngine> getSimEngine() const;

      private:
        std::stack<std::unique_ptr<Command>> m_undoStack;
        std::stack<std::unique_ptr<Command>> m_redoStack;

        std::shared_ptr<Canvas::Scene> m_scene = nullptr;
        std::shared_ptr<SimEngine::SimulationEngine> m_simEngine = nullptr;
    };

} // namespace Bess::Cmd
