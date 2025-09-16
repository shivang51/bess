#include "scene/commands/add_command.h"
#include "scene/scene.h"

// sim engine commands
#include "commands/commands.h"

namespace Bess::Canvas::Commands {
    AddCommand::AddCommand(std::shared_ptr<const SimEngine::ComponentDefinition> comp,
                           const glm::vec2 &pos,
                           int inpCount, int outCount) {
        m_compDef = comp;
        m_lastPos = pos;
        m_inpCount = inpCount;
        m_outCount = outCount;
    }

    bool AddCommand::execute() {
        auto &cmdMngr = SimEngine::SimulationEngine::instance().getCmdManager();
        auto simEngineUuid = cmdMngr.execute<SimEngine::Commands::AddCommand, Bess::UUID>(m_compDef->type, m_inpCount, m_outCount);
        if (!simEngineUuid.has_value())
            return false;
        m_compId = Scene::instance().createSimEntity(simEngineUuid.value(), m_compDef, m_lastPos);
        Scene::instance().setLastCreatedComp({m_compDef, m_inpCount, m_outCount});
        return true;
    }

    std::any AddCommand::undo() {
        auto &cmdMngr = SimEngine::SimulationEngine::instance().getCmdManager();
        cmdMngr.undo();

        Scene::instance().deleteSceneEntity(m_compId);
        return {};
    }

    std::any AddCommand::getResult() {
        return m_compId;
    }
} // namespace Bess::Canvas::Commands
