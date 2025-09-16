#include "scene/commands/del_connection_command.h"
#include "bess_uuid.h"
#include "commands/commands.h"
#include "scene/scene.h"
#include "scene/scene_serializer.h"
#include "simulation_engine.h"

namespace Bess::Canvas::Commands {
    DelConnectionCommand::DelConnectionCommand(UUID uuid)
        : m_uuid(uuid) {}

    bool DelConnectionCommand::execute() {
        auto &simCmdManager = SimEngine::SimulationEngine::instance().getCmdManager();
        auto &scene = Scene::instance();

        if (!m_redo) {
            auto &reg = scene.getEnttRegistry();
            auto entity = scene.getEntityWithUuid(m_uuid);

            auto &connComp = reg.get<Components::ConnectionComponent>(entity);

            auto &slotCompA = reg.get<Components::SlotComponent>(scene.getEntityWithUuid(connComp.inputSlot));
            auto &slotCompB = reg.get<Components::SlotComponent>(scene.getEntityWithUuid(connComp.outputSlot));

            auto &simCompA = reg.get<Components::SimulationComponent>(scene.getEntityWithUuid(slotCompA.parentId));
            auto &simCompB = reg.get<Components::SimulationComponent>(scene.getEntityWithUuid(slotCompB.parentId));

            auto pinTypeA = slotCompA.slotType == Components::SlotType::digitalInput ? SimEngine::PinType::input : SimEngine::PinType::output;
            auto pinTypeB = slotCompB.slotType == Components::SlotType::digitalInput ? SimEngine::PinType::input : SimEngine::PinType::output;

            auto _ = simCmdManager.execute<SimEngine::Commands::DelConnectionCommand, std::string>(simCompA.simEngineEntity, slotCompA.idx, pinTypeA, simCompB.simEngineEntity, slotCompB.idx, pinTypeB);
        } else {
            simCmdManager.redo();
        }

        m_json.clear();
        SceneSerializer ser;
        ser.serializeEntity(m_uuid, m_json);

        scene.deleteConnectionFromScene(m_uuid);

        return true;
    }

    std::any DelConnectionCommand::undo() {
        auto &simCmdManager = SimEngine::SimulationEngine::instance().getCmdManager();
        simCmdManager.undo();

        SceneSerializer ser;
        ser.deserializeEntity(m_json);

        m_redo = true;

        return {};
    }

    std::any DelConnectionCommand::getResult() {
        return std::string("success");
    }
} // namespace Bess::Canvas::Commands
