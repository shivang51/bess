#include "command_system.h"
#include "common/logger.h"
#include "pages/main_page/main_page_state.h"
#include "scene/scene.h"

namespace Bess::Cmd {
    namespace {
        Canvas::Scene *resolveCommandScene(Command *cmd, Canvas::Scene *fallbackScene) {
            if (!cmd) {
                return fallbackScene;
            }

            if (!cmd->hasSceneContext()) {
                cmd->setSceneContext(fallbackScene);
            }

            return cmd->getSceneContext();
        }

        bool canMergeCommands(const Command *existingCmd,
                              const Command *newCmd) {
            return existingCmd &&
                   newCmd &&
                   existingCmd->sharesSceneContextWith(newCmd) &&
                   existingCmd->canMergeWith(newCmd);
        }
    } // namespace
    void CommandSystem::init() {
        mp_scene = nullptr;
        mp_simEngine = nullptr;
        m_redoStack = {};
        m_undoStack = {};
    }

    void CommandSystem::execute(std::unique_ptr<Command> cmd) {
        auto *commandScene = resolveCommandScene(cmd.get(), mp_scene);
        if (cmd &&
            cmd->execute(commandScene, mp_simEngine)) {
            if (canMergeCommands(m_undoStack.empty() ? nullptr : m_undoStack.top().get(), cmd.get())) {
                m_undoStack.top()->mergeWith(cmd.get());
            } else {
                m_undoStack.push(std::move(cmd));
            }
            m_redoStack = std::stack<std::unique_ptr<Command>>();
        }
    }

    void CommandSystem::undo() {
        if (!m_undoStack.empty()) {
            auto cmd = std::move(m_undoStack.top());
            m_undoStack.pop();
            cmd->undo(resolveCommandScene(cmd.get(), mp_scene), mp_simEngine);
            BESS_DEBUG("[CommandSystem] Undo: {}", cmd->getName());
            m_redoStack.push(std::move(cmd));
        }
    }

    void CommandSystem::redo() {
        if (!m_redoStack.empty()) {
            auto cmd = std::move(m_redoStack.top());
            m_redoStack.pop();
            cmd->redo(resolveCommandScene(cmd.get(), mp_scene), mp_simEngine);
            BESS_DEBUG("[CommandSystem] Redo: {}", cmd->getName());
            m_undoStack.push(std::move(cmd));
        }
    }

    void CommandSystem::push(std::unique_ptr<Command> cmd, bool tryMerge) {
        resolveCommandScene(cmd.get(), mp_scene);
        if (tryMerge &&
            canMergeCommands(m_undoStack.empty() ? nullptr : m_undoStack.top().get(), cmd.get())) {
            m_undoStack.top()->mergeWith(cmd.get());
        } else {
            m_undoStack.push(std::move(cmd));
        }
        m_redoStack = std::stack<std::unique_ptr<Command>>();
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

    Canvas::Scene *CommandSystem::getScene() {
        return mp_scene;
    }
} // namespace Bess::Cmd
