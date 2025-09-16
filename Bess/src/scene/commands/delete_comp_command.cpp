#include "scene/commands/delete_comp_command.h"
#include "scene/scene.h"

// sim engine commands
#include "commands/commands.h"

#include "scene/scene_serializer.h"

namespace Bess::Canvas::Commands {
    DeleteCompCommand::DeleteCompCommand(const UUID &compId) : m_compId(compId) {}

    bool DeleteCompCommand::execute() {
        auto &registry = Scene::instance().getEnttRegistry();

        const auto ent = Scene::instance().getEntityWithUuid(m_compId);

        if (auto* simComp = registry.try_get<Components::SimulationComponent>(ent)) {
            auto &cmdMngr = SimEngine::SimulationEngine::instance().getCmdManager();
            auto res = cmdMngr.execute<SimEngine::Commands::DeleteCompCommand, std::string>(simComp->simEngineEntity);
            m_isSimComponent = true;
        }

        SceneSerializer ser;
        ser.serializeEntity(m_compId, m_compJson);

        Scene::instance().deleteSceneEntity(m_compId);

        return true;
    }

    std::any DeleteCompCommand::undo() {
        if (m_isSimComponent) {
            auto &cmdMngr = SimEngine::SimulationEngine::instance().getCmdManager();
            cmdMngr.undo();
        }

        SceneSerializer ser;
        ser.deserializeEntity(m_compJson);

        return true;
    }

    std::any DeleteCompCommand::getResult() {
        return std::string("Deletion successful");
    }
} // namespace Bess::Canvas::Commands
