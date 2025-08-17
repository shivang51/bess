#include "scene/commands/commands.h"
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
        auto& cmdMngr = SimEngine::SimulationEngine::instance().getCmdManager();
        auto simEngineUuid = cmdMngr.execute<SimEngine::Commands::AddCommand, Bess::UUID>(m_compDef->type, m_inpCount, m_outCount);
        Scene::instance().createSimEntity(simEngineUuid);
    }
} // namespace Bess::Canvas::Scene::Commands