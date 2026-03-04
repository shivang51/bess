#pragma once

#include <string>
namespace Bess::Canvas {
    class Scene;
}

namespace Bess::SimEngine {
    class SimulationEngine;
}

namespace Bess::Cmd {
    class Command {
      public:
        Command() = default;
        virtual ~Command() = default;

        /**
         * @breif Executes the command, applying its changes to the scene and simulation engine.
         * @param scene The current scene to which the command will apply its changes.
         * @param simEngine The simulation engine that may be affected by the command's execution.
         * @return A boolean indicating whether the command executed successfully.
         */
        virtual bool execute(Canvas::Scene *scene,
                             SimEngine::SimulationEngine *simEngine) = 0;

        virtual void undo(Canvas::Scene *scene,
                          SimEngine::SimulationEngine *simEngine) = 0;

        virtual void redo(Canvas::Scene *scene,
                          SimEngine::SimulationEngine *simEngine) = 0;

        virtual bool canMergeWith(const Command *other) const;

        virtual bool mergeWith(const Command *other);

        virtual std::string getName() const;

      protected:
        std::string m_name;
    };
} // namespace Bess::Cmd
