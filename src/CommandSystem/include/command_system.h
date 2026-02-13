#pragma once

#include "command.h"
#include "scene/scene_events.h"
#include <memory>
#include <stack>

namespace Bess::Canvas {
    class Scene;
}

namespace Bess::SimEngine {
    class SimulationEngine;
}

namespace Bess::Cmd {
    class CommandSystem {
      public:
        CommandSystem() = default;
        ~CommandSystem() = default;

        void init(Canvas::Scene *scene, SimEngine::SimulationEngine *simEngine);

        void execute(std::unique_ptr<Command> cmd);

        // For commands that are already executed,
        // just want to push to undo stack without executing again
        void push(std::unique_ptr<Command> cmd);

        void undo();
        void redo();

        void reset();

      public:
        void setScene(Canvas::Scene *scene);

        void setSimEngine(SimEngine::SimulationEngine *simEngine);

      private:
        std::stack<std::unique_ptr<Command>> m_undoStack;
        std::stack<std::unique_ptr<Command>> m_redoStack;

        Canvas::Scene *mp_scene;
        SimEngine::SimulationEngine *mp_simEngine;
    };

} // namespace Bess::Cmd
