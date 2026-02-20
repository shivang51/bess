#include "command_system.h"
#include "common/logger.h"
#include "scene/scene.h"

namespace Bess::Cmd {

    void CommandSystem::init(Canvas::Scene *scene,
                             SimEngine::SimulationEngine *simEngine) {
        this->mp_scene = scene;
        this->mp_simEngine = simEngine;
    }

    void CommandSystem::execute(std::unique_ptr<Command> cmd) {
        if (cmd &&
            cmd->execute(mp_scene, mp_simEngine)) {
            if (!m_undoStack.empty() && m_undoStack.top()->canMergeWith(cmd.get())) {
                m_undoStack.top()->mergeWith(cmd.get());
            } else {
                m_undoStack.push(std::move(cmd));
            }
        }
    }

    void CommandSystem::undo() {
        if (!m_undoStack.empty()) {
            auto cmd = std::move(m_undoStack.top());
            m_undoStack.pop();
            cmd->undo(mp_scene, mp_simEngine);
            BESS_DEBUG("[CommandSystem] Undo: {}", cmd->getName());
            m_redoStack.push(std::move(cmd));
        }
    }

    void CommandSystem::redo() {
        if (!m_redoStack.empty()) {
            auto cmd = std::move(m_redoStack.top());
            m_redoStack.pop();
            cmd->redo(mp_scene, mp_simEngine);
            BESS_DEBUG("[CommandSystem] Redo: {}", cmd->getName());
            m_undoStack.push(std::move(cmd));
        }
    }

    void CommandSystem::push(std::unique_ptr<Command> cmd, bool tryMerge) {
        if (tryMerge &&
            !m_undoStack.empty() &&
            m_undoStack.top()->canMergeWith(cmd.get())) {
            m_undoStack.top()->mergeWith(cmd.get());
        } else {
            m_undoStack.push(std::move(cmd));
        }
    }

    void CommandSystem::reset() {
        m_undoStack = std::stack<std::unique_ptr<Command>>();
        m_redoStack = std::stack<std::unique_ptr<Command>>();
    }

    void CommandSystem::setScene(Canvas::Scene *scene) {
        this->mp_scene = scene;
    }

    void CommandSystem::setSimEngine(SimEngine::SimulationEngine *simEngine) {
        this->mp_simEngine = simEngine;
    }

    bool CommandSystem::canUndo() const {
        return !m_undoStack.empty();
    }

    bool CommandSystem::canRedo() const {
        return !m_redoStack.empty();
    }
} // namespace Bess::Cmd
