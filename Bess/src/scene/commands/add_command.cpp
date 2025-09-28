#include "scene/commands/add_command.h"
#include "bess_uuid.h"
#include "scene/scene.h"

// sim engine commands
#include "commands/commands.h"
#include "scene/scene_serializer.h"
#include "simulation_engine.h"
#include "json/value.h"

namespace Bess::Canvas::Commands {
    AddCommand::AddCommand(const std::vector<AddCommandData> &data) : m_data(data) {
        m_compJsons = std::vector<Json::Value>(m_data.size(), Json::objectValue);
    }

    bool AddCommand::execute() {
        auto &simEngine = SimEngine::SimulationEngine::instance();
        auto &cmdMngr = simEngine.getCmdManager();

        SceneSerializer ser;
        auto &scene = Scene::instance();

        std::vector<SimEngine::Commands::AddCommandData> simAddCmdData{};
        if (m_redo) {
            cmdMngr.redo();
        }

        for (size_t i = 0; i < m_data.size(); i++) {
            const auto &data = m_data[i];
            auto &compJson = m_compJsons[i];

            if (m_redo) {
                ser.deserializeEntity(compJson);
                continue;
            }

            if (data.isSimComp()) {
                simAddCmdData.emplace_back(data.def->type, data.inputCount, data.outputCount);
            } else {
                m_compIds.emplace_back(scene.createNonSimEntity(data.nsComp, data.pos));
            }
        }

        if (m_redo || simAddCmdData.empty())
            return true;

        const auto simEngineUuids = cmdMngr.execute<SimEngine::Commands::AddCommand, std::vector<UUID>>(simAddCmdData);

        if (!simEngineUuids.has_value())
            return false;

        int i = 0;
        for (const auto &simEngineId : simEngineUuids.value()) {
            const auto &data = m_data[i];
            m_compIds.emplace_back(scene.createSimEntity(simEngineId, data.def, data.pos));
            if (i == m_data.size() - 1)
                scene.setLastCreatedComp({.componentDefinition = data.def, .inputCount = data.inputCount, .outputCount = data.outputCount});
            i++;
        }

        return true;
    }

    std::any AddCommand::undo() {
        auto &cmdMngr = SimEngine::SimulationEngine::instance().getCmdManager();
        cmdMngr.undo();

        auto &scene = Scene::instance();
        for (size_t i = 0; i < m_data.size(); i++) {
            auto &compJson = m_compJsons[i];
            compJson.clear();
            SceneSerializer ser;
            ser.serializeEntity(m_compIds[i], compJson);
            scene.deleteSceneEntity(m_compIds[i]);
        }

        m_redo = true;
        return {};
    }

    std::any AddCommand::getResult() {
        return m_compIds;
    }
} // namespace Bess::Canvas::Commands
