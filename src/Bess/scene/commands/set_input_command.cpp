#include "scene/commands/set_input_command.h"
#include "application/types.h"
#include "commands/commands.h"
#include "commands/commands_manager.h"
#include "scene/scene.h"
#include "simulation_engine.h"

namespace Bess::Canvas::Commands {
    SetInputCommand::SetInputCommand(const UUID &compId, SimEngine::LogicState state)
        : m_compId(compId), m_state(state) {}

    bool SetInputCommand::execute() {
        return false;
        // auto scene = Scene::instance();
        // auto &simComp = scene->getEnttRegistry().get<Components::SimulationComponent>(scene->getEntityWithUuid(m_compId));
        //
        // auto &simCmdManager = SimEngine::SimulationEngine::instance().getCmdManager();
        //
        // bool status = true;
        // if (!m_redo) {
        //     const auto res = simCmdManager.execute<SimEngine::Commands::SetInputCommand, bool>(simComp.simEngineEntity, m_state);
        //     status = res.has_value();
        // } else {
        //     simCmdManager.redo();
        // }
        //
        // return status;
    }

    std::any SetInputCommand::undo() {
        // auto &simCmdManager = SimEngine::SimulationEngine::instance().getCmdManager();
        // simCmdManager.undo();
        m_redo = true;
        return {};
    }

    std::any SetInputCommand::getResult() {
        return std::string("success");
    }
} // namespace Bess::Canvas::Commands
