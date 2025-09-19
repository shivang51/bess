#include "scene/commands/del_connection_command.h"
#include "bess_uuid.h"
#include "commands/commands.h"
#include "scene/scene.h"
#include "scene/scene_serializer.h"
#include "simulation_engine.h"
#include "json/value.h"

namespace Bess::Canvas::Commands {
    DelConnectionCommand::DelConnectionCommand(const std::vector<UUID> &uuids)
        : m_uuids(uuids) {
        m_jsons = std::vector<Json::Value>(uuids.size(), Json::objectValue);
    }

    bool DelConnectionCommand::execute() {
        auto &simCmdManager = SimEngine::SimulationEngine::instance().getCmdManager();
        auto &scene = Scene::instance();
        auto &sceneReg = scene.getEnttRegistry();
        SceneSerializer ser;

        if (!m_redo) {
            std::vector<SimEngine::Commands::DelConnectionCommandData> simCmdData = {};

            int i = 0;
            for (const auto uuid : m_uuids) {
                if (!scene.isEntityValid(uuid))
                    return false;

                auto entity = scene.getEntityWithUuid(uuid);

                auto &connComp = sceneReg.get<Components::ConnectionComponent>(entity);

                auto &slotCompA = sceneReg.get<Components::SlotComponent>(scene.getEntityWithUuid(connComp.inputSlot));
                auto &slotCompB = sceneReg.get<Components::SlotComponent>(scene.getEntityWithUuid(connComp.outputSlot));

                auto &simCompA = sceneReg.get<Components::SimulationComponent>(scene.getEntityWithUuid(slotCompA.parentId));
                auto &simCompB = sceneReg.get<Components::SimulationComponent>(scene.getEntityWithUuid(slotCompB.parentId));

                auto pinTypeA = slotCompA.slotType == Components::SlotType::digitalInput ? SimEngine::PinType::input : SimEngine::PinType::output;
                auto pinTypeB = slotCompB.slotType == Components::SlotType::digitalInput ? SimEngine::PinType::input : SimEngine::PinType::output;

                SimEngine::Commands::DelConnectionCommandData data = {simCompA.simEngineEntity, slotCompA.idx, pinTypeA,
                                                                      simCompB.simEngineEntity, slotCompB.idx, pinTypeB};
                simCmdData.emplace_back(data);

                auto &json = m_jsons[i++];
                json.clear();
                ser.serializeEntity(uuid, json);
                scene.deleteConnectionFromScene(uuid);
            }

            auto _ = simCmdManager.execute<SimEngine::Commands::DelConnectionCommand, std::string>(simCmdData);
        } else {
            simCmdManager.redo();
            int i = 0;
            for (const auto uuid : m_uuids) {
                auto &json = m_jsons[i++];
                json.clear();
                ser.serializeEntity(uuid, json);
                scene.deleteConnectionFromScene(uuid);
            }
        }

        return true;
    }

    std::any DelConnectionCommand::undo() {
        auto &simCmdManager = SimEngine::SimulationEngine::instance().getCmdManager();
        simCmdManager.undo();

        SceneSerializer ser;

        for (const auto &json : m_jsons) {
            ser.deserializeEntity(json);
        }

        m_redo = true;

        return {};
    }

    std::any DelConnectionCommand::getResult() {
        return std::string("success");
    }
} // namespace Bess::Canvas::Commands
