#include "scene/commands/del_connection_command.h"
#include "bess_uuid.h"
#include "commands/commands.h"
#include "scene/scene.h"
#include "scene/scene_state/components/connection_scene_component.h"
#include "scene/scene_state/components/sim_scene_component.h"
#include "scene/scene_state/components/slot_scene_component.h"
#include "simulation_engine.h"
#include "json/value.h"
#include <cstdint>

namespace Bess::Canvas::Commands {
    DelConnectionCommand::DelConnectionCommand(const std::vector<UUID> &uuids)
        : m_uuids(uuids) {
        m_jsons = std::vector<Json::Value>(uuids.size(), Json::objectValue);
    }

    bool DelConnectionCommand::execute() {
        auto &simCmdManager = SimEngine::SimulationEngine::instance().getCmdManager();
        auto &sceneState = Scene::instance()->getState();

        if (!m_redo) {
            std::vector<SimEngine::Commands::DelConnectionCommandData> simCmdData = {};

            int i = 0;
            for (const auto uuid : m_uuids) {
                const auto &connComp = sceneState.getComponentByUuid<ConnectionSceneComponent>(uuid);
                const auto &slots = ConnectionSceneComponent::parseSlotsKey(connComp->getSlotsKey());

                const auto &slotCompA = sceneState.getComponentByUuid<SlotSceneComponent>(slots.first);
                const auto &slotCompB = sceneState.getComponentByUuid<SlotSceneComponent>(slots.second);

                const auto &simCompA = sceneState.getComponentByUuid<SimulationSceneComponent>(
                    slotCompA->getParentComponent());
                const auto &simCompB = sceneState.getComponentByUuid<SimulationSceneComponent>(
                    slotCompB->getParentComponent());

                const auto pinTypeA = slotCompA->getSlotType() == SlotType::digitalInput
                                          ? SimEngine::PinType::input
                                          : SimEngine::PinType::output;
                const auto pinTypeB = slotCompB->getSlotType() == SlotType::digitalInput
                                          ? SimEngine::PinType::input
                                          : SimEngine::PinType::output;

                SimEngine::Commands::DelConnectionCommandData data = {simCompA->getSimEngineId(),
                                                                      (uint32_t)slotCompA->getIndex(), pinTypeA,
                                                                      simCompB->getSimEngineId(),
                                                                      (uint32_t)slotCompB->getIndex(), pinTypeB};
                simCmdData.emplace_back(data);

                auto &json = m_jsons[i++];
                json.clear();
                JsonConvert::toJsonValue(*connComp, json);
                sceneState.removeComponent(uuid);
            }

            auto _ = simCmdManager.execute<SimEngine::Commands::DelConnectionCommand, std::string>(simCmdData);
        } else {
            simCmdManager.redo();
            int i = 0;
            for (const auto uuid : m_uuids) {
                auto &json = m_jsons[i++];
                json.clear();
                const auto &connComp = sceneState.getComponentByUuid<ConnectionSceneComponent>(uuid);
                JsonConvert::toJsonValue(*connComp, json);
                sceneState.removeComponent(uuid);
            }
        }

        return true;
    }

    std::any DelConnectionCommand::undo() {
        auto &simCmdManager = SimEngine::SimulationEngine::instance().getCmdManager();
        simCmdManager.undo();

        for (const auto &json : m_jsons) {
            ConnectionSceneComponent comp;

            JsonConvert::fromJsonValue(json, comp);
            Scene::instance()->getState().addComponent<ConnectionSceneComponent>(
                std::make_shared<ConnectionSceneComponent>(comp));
        }

        m_redo = true;

        return {};
    }

    std::any DelConnectionCommand::getResult() {
        return std::string("success");
    }
} // namespace Bess::Canvas::Commands
