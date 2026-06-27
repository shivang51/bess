#include "bess_core/commands/command.h"

namespace Bess::Cmd {
    std::string Command::getName() const {
        return m_name;
    }

    bool Command::mergeWith(const Command *other) {
        (void)other;
        return false;
    }

    bool Command::canMergeWith(const Command *other) const {
        (void)other;
        return false;
    }

    void Command::setSceneContext(const std::shared_ptr<Canvas::Scene> &scene) {
        m_sceneContext = scene;
    }

    std::shared_ptr<Canvas::Scene> Command::getSceneContext() const {
        return m_sceneContext;
    }

    bool Command::hasSceneContext() const {
        return m_sceneContext != nullptr;
    }

    bool Command::sharesSceneContextWith(const Command *other) const {
        return other && m_sceneContext != nullptr &&
               m_sceneContext == other->m_sceneContext;
    }

    void Command::setComponentHooks(
        std::shared_ptr<const SceneComponentCommandHooks> hooks) {
        m_componentHooks =
            hooks ? std::move(hooks) : defaultSceneComponentCommandHooks();
    }

    std::shared_ptr<const SceneComponentCommandHooks>
    Command::getComponentHooks() const {
        return m_componentHooks;
    }

    bool Command::hasComponentHooks() const {
        return m_componentHooks != nullptr;
    }

    CommandContext Command::makeContext(const CommandContext &fallback) const {
        CommandContext context = fallback;
        if (m_sceneContext) {
            context.scene = m_sceneContext;
        }

        if (m_componentHooks) {
            context.componentHooks = m_componentHooks;
        }

        if (!context.componentHooks) {
            context.componentHooks = defaultSceneComponentCommandHooks();
        }

        return context;
    }
} // namespace Bess::Cmd
