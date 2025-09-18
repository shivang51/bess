#include "scene/commands/add_command.h"
#include "bess_uuid.h"
#include "scene/scene.h"

// sim engine commands
#include "commands/commands.h"
#include "scene/scene_serializer.h"
#include "simulation_engine.h"
#include "json/value.h"

namespace Bess::Canvas::Commands {
    AddCommand::AddCommand(const AddCommandData &data) : m_data(data) {
        m_compJson = Json::objectValue;
        m_compId = UUID::null;
    }

    bool AddCommand::execute() {
        auto &simEngine = SimEngine::SimulationEngine::instance();
        auto &cmdMngr = simEngine.getCmdManager();

        if (m_redo) {
            if (m_data.isSimComp())
                cmdMngr.redo();
            SceneSerializer ser;
            ser.deserializeEntity(m_compJson);
            return true;
        }
        auto &scene = Scene::instance();
        if (m_data.isSimComp()) {
            auto simEngineUuid = cmdMngr.execute<SimEngine::Commands::AddCommand, Bess::UUID>(m_data.def->type, m_data.inputCount, m_data.outputCount);
            if (!simEngineUuid.has_value())
                return false;
            m_compId = scene.createSimEntity(simEngineUuid.value(), m_data.def, m_data.pos);
            scene.setLastCreatedComp({m_data.def, m_data.inputCount, m_data.outputCount});
        } else {
            m_compId = scene.createNonSimEntity(m_data.nsComp, m_data.pos);
        }

        return true;
    }

    std::any AddCommand::undo() {
        if (m_data.isSimComp()) {
            auto &cmdMngr = SimEngine::SimulationEngine::instance().getCmdManager();
            cmdMngr.undo();
        }

        m_compJson.clear();
        SceneSerializer ser;
        ser.serializeEntity(m_compId, m_compJson);
        m_redo = true;

        Scene::instance().deleteSceneEntity(m_compId);
        return {};
    }

    std::any AddCommand::getResult() {
        return m_compId;
    }
} // namespace Bess::Canvas::Commands
