#include "scene/commands/connect_command.h"
#include "scene/scene.h"

// sim engine commands
#include "commands/commands.h"

namespace Bess::Canvas::Commands {
    ConnectCommand::ConnectCommand(entt::entity startSlot, entt::entity endSlot) {
        m_startSlot = startSlot;
        m_endSlot = endSlot;
    }

    bool ConnectCommand::execute() {
        auto &registry = Scene::instance().getEnttRegistry();
        auto &startSlotComp = registry.get<Components::SlotComponent>(m_startSlot);
        auto &endSlotComp = registry.get<Components::SlotComponent>(m_endSlot);

        auto startSimParent = registry.get<Components::SimulationComponent>(Scene::instance().getEntityWithUuid(startSlotComp.parentId)).simEngineEntity;
        auto endSimParent = registry.get<Components::SimulationComponent>(Scene::instance().getEntityWithUuid(endSlotComp.parentId)).simEngineEntity;

        auto startPinType = startSlotComp.slotType == Components::SlotType::digitalInput ? SimEngine::PinType::input : SimEngine::PinType::output;
        auto dstPinType = endSlotComp.slotType == Components::SlotType::digitalInput ? SimEngine::PinType::input : SimEngine::PinType::output;
        auto &cmdMngr = SimEngine::SimulationEngine::instance().getCmdManager();
        auto res = cmdMngr.execute<Bess::SimEngine::Commands::ConnectCommand, std::string>(startSimParent, startSlotComp.idx, startPinType, endSimParent, endSlotComp.idx, dstPinType);

        if (!res.has_value())
            return false;

        if (startSlotComp.slotType == Components::SlotType::digitalInput) {
            m_connection = Scene::instance().generateBasicConnection(m_startSlot, m_endSlot);
        } else {
            m_connection = Scene::instance().generateBasicConnection(m_endSlot, m_startSlot);
        }
        return true;
    }

    std::any ConnectCommand::undo() {
        auto &cmdMngr = SimEngine::SimulationEngine::instance().getCmdManager();
        cmdMngr.undo();

        Scene::instance().removeConnectionEntt(m_connection);
        return {};
    }

    std::any ConnectCommand::getResult() {
        return true;
    }
} // namespace Bess::Canvas::Commands
