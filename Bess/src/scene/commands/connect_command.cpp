#include "scene/commands/connect_command.h"
#include "scene/scene.h"

// sim engine commands
#include "commands/commands.h"
#include "scene/scene_serializer.h"

namespace Bess::Canvas::Commands {
    ConnectCommand::ConnectCommand(const UUID startSlot, const UUID endSlot) {
        m_startSlot = startSlot;
        m_endSlot = endSlot;
    }

    bool ConnectCommand::execute() {
        if (m_delete)
            return false;
        auto &cmdMngr = SimEngine::SimulationEngine::instance().getCmdManager();

        if (!m_redo) {
            auto &registry = Scene::instance().getEnttRegistry();
            auto &startSlotComp = registry.get<Components::SlotComponent>(
                Scene::instance().getEntityWithUuid(m_startSlot));
            auto &endSlotComp = registry.get<Components::SlotComponent>(
                Scene::instance().getEntityWithUuid(m_endSlot));

            auto startSimParent = registry.get<Components::SimulationComponent>(Scene::instance().getEntityWithUuid(startSlotComp.parentId)).simEngineEntity;
            auto endSimParent = registry.get<Components::SimulationComponent>(Scene::instance().getEntityWithUuid(endSlotComp.parentId)).simEngineEntity;

            auto startPinType = startSlotComp.slotType == Components::SlotType::digitalInput ? SimEngine::PinType::input : SimEngine::PinType::output;
            auto dstPinType = endSlotComp.slotType == Components::SlotType::digitalInput ? SimEngine::PinType::input : SimEngine::PinType::output;
            const auto res = cmdMngr.execute<Bess::SimEngine::Commands::ConnectCommand, std::string>(startSimParent, startSlotComp.idx, startPinType, endSimParent, endSlotComp.idx, dstPinType);

            if (!res.has_value())
                return false;

            if (startSlotComp.slotType == Components::SlotType::digitalInput) {
                m_connection = Scene::instance().connectSlots(m_startSlot, m_endSlot);
            } else {
                m_connection = Scene::instance().connectSlots(m_endSlot, m_startSlot);
            }
        } else {
            cmdMngr.redo();
            SceneSerializer ser;
            ser.deserializeEntity(m_json);
            auto &scene = Scene::instance();
            auto &reg = scene.getEnttRegistry();

            const auto startSlotEntt = scene.getEntityWithUuid(m_startSlot);
            const auto endSlotEntt = scene.getEntityWithUuid(m_endSlot);

            if (!reg.valid(startSlotEntt) || !reg.valid(endSlotEntt)) {
                return false;
            }

            auto startSlotComp = reg.get<Components::SlotComponent>(startSlotEntt);
            auto endSlotComp = reg.get<Components::SlotComponent>(endSlotEntt);

            startSlotComp.connections.emplace_back(m_connection);
            endSlotComp.connections.emplace_back(m_connection);
        }
        return true;
    }

    std::any ConnectCommand::undo() {
        auto &cmdMngr = SimEngine::SimulationEngine::instance().getCmdManager();
        cmdMngr.undo();

        m_json.clear();
        SceneSerializer ser;
        ser.serializeEntity(m_connection, m_json);

        Scene::instance().deleteConnectionFromScene(m_connection);

        m_redo = true;
        return {};
    }

    std::any ConnectCommand::getResult() {
        return m_connection;
    }
} // namespace Bess::Canvas::Commands
