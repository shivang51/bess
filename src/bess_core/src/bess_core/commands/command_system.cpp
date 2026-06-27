#include "bess_core/commands/command_system.h"

#include "bess_core/g_app_context.h"
#include "bess_core/project_context.h"
#include "bess_core/scene/scene.h"
#include "bess_core/scene_driver.h"
#include "common/logger.h"

namespace Bess::Cmd {
    namespace {
        bool canMergeCommands(const Command *existingCmd,
                              const Command *newCmd) {
            return existingCmd && newCmd &&
                   existingCmd->sharesSceneContextWith(newCmd) &&
                   existingCmd->canMergeWith(newCmd);
        }
    } // namespace

    void CommandSystem::onDestroy() {
    }

    void CommandSystem::onInit() {
        reset();
    }

    void CommandSystem::onShutdown() {
        BESS_DEBUG("[CommandSystem] Shutting down Command System");
        reset();
    }

    void CommandSystem::execute(std::unique_ptr<Command> cmd) {
        if (!cmd) {
            return;
        }

        const auto context = getContext(cmd.get());
        if (!context.scene) {
            BESS_WARN("[CommandSystem] Cannot execute command without scene");
            return;
        }

        if (!cmd->execute(context)) {
            return;
        }

        if (canMergeCommands(m_undoStack.empty() ? nullptr
                                                 : m_undoStack.top().get(),
                             cmd.get())) {
            m_undoStack.top()->mergeWith(cmd.get());
        } else {
            m_undoStack.push(std::move(cmd));
        }
        m_redoStack = std::stack<std::unique_ptr<Command>>();
    }

    void CommandSystem::undo() {
        if (m_undoStack.empty()) {
            return;
        }

        auto cmd = std::move(m_undoStack.top());
        m_undoStack.pop();
        const auto context = getContext(cmd.get());
        if (!context.scene) {
            BESS_WARN("[CommandSystem] Cannot undo command without scene");
            return;
        }

        cmd->undo(context);
        BESS_DEBUG("[CommandSystem] Undo: {}", cmd->getName());
        m_redoStack.push(std::move(cmd));
    }

    void CommandSystem::redo() {
        if (m_redoStack.empty()) {
            return;
        }

        auto cmd = std::move(m_redoStack.top());
        m_redoStack.pop();
        const auto context = getContext(cmd.get());
        if (!context.scene) {
            BESS_WARN("[CommandSystem] Cannot redo command without scene");
            return;
        }

        cmd->redo(context);
        BESS_DEBUG("[CommandSystem] Redo: {}", cmd->getName());
        m_undoStack.push(std::move(cmd));
    }

    void CommandSystem::push(std::unique_ptr<Command> cmd, bool tryMerge) {
        if (!cmd) {
            return;
        }

        const auto context = getContext(cmd.get());
        if (!context.scene) {
            BESS_WARN("[CommandSystem] Cannot push command without scene");
            return;
        }

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

    bool CommandSystem::canUndo() const {
        return !m_undoStack.empty();
    }

    bool CommandSystem::canRedo() const {
        return !m_redoStack.empty();
    }

    std::shared_ptr<Canvas::Scene> CommandSystem::getInternalScene() const {
        return m_scene;
    }

    CommandContext CommandSystem::getContext(Command *cmd) const {
        CommandContext context;
        context.scene = getScene();
        context.componentHooks = m_componentHooks
                                     ? m_componentHooks
                                     : defaultSceneComponentCommandHooks();

        if (cmd) {
            if (!cmd->hasSceneContext()) {
                cmd->setSceneContext(context.scene);
            }
            if (!cmd->hasComponentHooks()) {
                cmd->setComponentHooks(context.componentHooks);
            }
            context = cmd->makeContext(context);
        }

        return context;
    }

    std::shared_ptr<Canvas::Scene> CommandSystem::getScene() const {
        if (m_scene) {
            return m_scene;
        }

        const auto &appCtx = GAppContext::getInstance();
        const auto &project = appCtx.getSubSystem<ProjectContext>();
        if (!project) {
            return nullptr;
        }

        const auto sceneDriver = project->getSubSystem<SceneDriver>();
        return sceneDriver ? sceneDriver->getActiveScene() : nullptr;
    }

    void CommandSystem::setScene(const std::shared_ptr<Canvas::Scene> &scene) {
        m_scene = scene;
    }

    void CommandSystem::setSceneComponentHooks(
        std::shared_ptr<const SceneComponentCommandHooks> hooks) {
        m_componentHooks =
            hooks ? std::move(hooks) : defaultSceneComponentCommandHooks();
    }

    std::shared_ptr<const SceneComponentCommandHooks>
    CommandSystem::getSceneComponentHooks() const {
        return m_componentHooks ? m_componentHooks
                                : defaultSceneComponentCommandHooks();
    }
} // namespace Bess::Cmd
