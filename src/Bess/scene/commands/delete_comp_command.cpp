#include "scene/commands/delete_comp_command.h"
#include "bess_uuid.h"
#include "scene/scene.h"
#include "simulation_engine.h"

// sim engine commands
#include "commands/commands.h"

#include "scene/scene_serializer.h"
#include "json/value.h"

namespace Bess::Canvas::Commands {
    DeleteCompCommand::DeleteCompCommand(const std::vector<UUID> &compIds) : m_compIds(compIds) {
        m_delData = std::vector<DeleteCompCommandData>(m_compIds.size(), {UUID::null, UUID::null, {}, Json::objectValue});
    }

    bool DeleteCompCommand::execute() {
        return false;
        // auto scene = Scene::instance();
        // auto &registry = scene->getEnttRegistry();
        // auto &cmdMngr = SimEngine::SimulationEngine::instance().getCmdManager();
        //
        // int i = 0;
        // m_simEngineComps = {};
        // SceneSerializer ser;
        // for (const auto compId : m_compIds) {
        //     const auto ent = Scene::instance()->getEntityWithUuid(compId);
        //
        //     auto &data = m_delData[i++];
        //
        //     if (const auto *simComp = registry.try_get<Components::SimulationComponent>(ent)) {
        //         data.simCompId = simComp->simEngineEntity;
        //         m_simEngineComps.emplace_back(data.simCompId);
        //     }
        //
        //     data.compJson.clear();
        //     ser.serializeEntity(compId, data.compJson);
        //     scene->deleteSceneEntity(compId);
        // }
        //
        // if (!m_simEngineComps.empty()) {
        //     if (!m_redo) {
        //         auto _ = cmdMngr.execute<SimEngine::Commands::DeleteCompCommand, std::string>(m_simEngineComps);
        //     } else {
        //         cmdMngr.redo();
        //     }
        // }
        //
        // return true;
    }

    std::any DeleteCompCommand::undo() {
        if (!m_simEngineComps.empty()) {
            // auto &cmdMngr = SimEngine::SimulationEngine::instance().getCmdManager();
            // cmdMngr.undo();
        }

        SceneSerializer ser;
        for (const auto &data : m_delData) {
            ser.deserializeEntity(data.compJson);
        }

        m_redo = true;

        return true;
    }

    std::any DeleteCompCommand::getResult() {
        return std::string("Deletion successful");
    }
} // namespace Bess::Canvas::Commands
