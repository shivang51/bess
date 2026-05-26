#include "command_system.h"
#include "bess_core/g_app_context.h"
#include "bess_core/project_context.h"
#include "bess_core/scene_driver.h"
#include "common/logger.h"
#include "pages/main_page/main_page_state.h"
#include "scene/scene.h"
#include "simulation_engine.h"

namespace Bess::Cmd {
    namespace {
        std::shared_ptr<Canvas::Scene> resolveCommandScene(
            Command *cmd, const std::shared_ptr<Canvas::Scene> &fallbackScene) {
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
            return existingCmd && newCmd &&
                   existingCmd->sharesSceneContextWith(newCmd) &&
                   existingCmd->canMergeWith(newCmd);
        }
    } // namespace
    void CommandSystem::init() {
        m_redoStack = {};
        m_undoStack = {};
    }

    void CommandSystem::execute(std::unique_ptr<Command> cmd) {
        auto commandScene = resolveCommandScene(cmd.get(), getScene());
        auto simEngine = getSimEngine();
        if (cmd && cmd->execute(commandScene, simEngine)) {
            if (canMergeCommands(m_undoStack.empty() ? nullptr
                                                     : m_undoStack.top().get(),
                                 cmd.get())) {
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
            auto commandScene = resolveCommandScene(cmd.get(), getScene());
            auto simEngine = getSimEngine();
            cmd->undo(commandScene, simEngine);
            BESS_DEBUG("[CommandSystem] Undo: {}", cmd->getName());
            m_redoStack.push(std::move(cmd));
        }
    }

    void CommandSystem::redo() {
        if (!m_redoStack.empty()) {
            auto cmd = std::move(m_redoStack.top());
            m_redoStack.pop();
            auto commandScene = resolveCommandScene(cmd.get(), getScene());
            auto simEngine = getSimEngine();
            cmd->redo(commandScene, simEngine);
            BESS_DEBUG("[CommandSystem] Redo: {}", cmd->getName());
            m_undoStack.push(std::move(cmd));
        }
    }

    void CommandSystem::push(std::unique_ptr<Command> cmd, bool tryMerge) {
        resolveCommandScene(cmd.get(), getScene());
        if (tryMerge &&
            canMergeCommands(m_undoStack.empty() ? nullptr
                                                 : m_undoStack.top().get(),
                             cmd.get())) {
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

    bool CommandSystem::canUndo() const { return !m_undoStack.empty(); }

    bool CommandSystem::canRedo() const { return !m_redoStack.empty(); }

    std::shared_ptr<Canvas::Scene> CommandSystem::getScene() const {
        const auto &appCtx = GAppContext::getInstance();
        const auto &project = appCtx.getSubSystem<ProjectContext>();
        return project->getSubSystem<SceneDriver>()->getActiveScene();
    }

    std::shared_ptr<SimEngine::SimulationEngine>
    CommandSystem::getSimEngine() const {
        const auto &appCtx = GAppContext::getInstance();
        const auto &project = appCtx.getSubSystem<ProjectContext>();
        return project->getSubSystem<SimEngine::SimulationEngine>();
    }

} // namespace Bess::Cmd
