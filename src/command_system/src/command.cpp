#include "command.h"

namespace Bess::Cmd {
    std::string Command::getName() const {
        return m_name;
    }

    bool Command::mergeWith(const Command *other) {
        return false;
    }

    bool Command::canMergeWith(const Command *other) const {
        return false;
    }

    void Command::setSceneContext(Canvas::Scene *scene) {
        m_sceneContext = scene;
    }

    Canvas::Scene *Command::getSceneContext() const {
        return m_sceneContext;
    }

    bool Command::hasSceneContext() const {
        return m_sceneContext != nullptr;
    }

    bool Command::sharesSceneContextWith(const Command *other) const {
        return other && m_sceneContext != nullptr && m_sceneContext == other->m_sceneContext;
    }
} // namespace Bess::Cmd
