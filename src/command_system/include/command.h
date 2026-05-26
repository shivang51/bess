#pragma once

#include <cstddef>
#include <memory>
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
         * @breif Executes the command, applying its changes to the scene and
         * simulation engine.
         * @param scene The current scene to which the command will apply its
         * changes.
         * @param simEngine The simulation engine that may be affected by the
         * command's execution.
         * @return A boolean indicating whether the command executed
         * successfully.
         */
        virtual bool execute(
            const std::shared_ptr<Canvas::Scene> &scene,
            const std::shared_ptr<SimEngine::SimulationEngine> &simEngine) = 0;

        virtual void
        undo(const std::shared_ptr<Canvas::Scene> &scene,
             const std::shared_ptr<SimEngine::SimulationEngine> &simEngine) = 0;

        virtual void
        redo(const std::shared_ptr<Canvas::Scene> &scene,
             const std::shared_ptr<SimEngine::SimulationEngine> &simEngine) = 0;

        virtual bool canMergeWith(const Command *other) const;

        virtual bool mergeWith(const Command *other);

        virtual std::string getName() const;

        void setSceneContext(const std::shared_ptr<Canvas::Scene> &scene);
        std::shared_ptr<Canvas::Scene> getSceneContext() const;
        bool hasSceneContext() const;
        bool sharesSceneContextWith(const Command *other) const;

      protected:
        std::shared_ptr<Canvas::Scene> m_sceneContext = nullptr;
        std::string m_name;
    };
} // namespace Bess::Cmd
