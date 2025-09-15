#include "scene/commands/commands.h"
#include "scene/scene.h"

// sim engine commands
#include "commands/commands.h"

#include "scene/scene_serializer.h"

namespace Bess::Canvas::Commands {
    AddCommand::AddCommand(std::shared_ptr<const SimEngine::ComponentDefinition> comp,
                           const glm::vec2 &pos,
                           int inpCount, int outCount) {
        m_compDef = comp;
        m_lastPos = pos;
        m_inpCount = inpCount;
        m_outCount = outCount;
    }

    bool AddCommand::execute() {
        auto &cmdMngr = SimEngine::SimulationEngine::instance().getCmdManager();
        auto simEngineUuid = cmdMngr.execute<SimEngine::Commands::AddCommand, Bess::UUID>(m_compDef->type, m_inpCount, m_outCount);
        if (!simEngineUuid.has_value())
            return false;
        m_compId = Scene::instance().createSimEntity(simEngineUuid.value(), m_compDef, m_lastPos);
        Scene::instance().setLastCreatedComp({m_compDef, m_inpCount, m_outCount});
        return true;
    }

    std::any AddCommand::undo() {
        auto &cmdMngr = SimEngine::SimulationEngine::instance().getCmdManager();
        cmdMngr.undo();

        Scene::instance().deleteSceneEntity(m_compId);
        return {};
    }

    std::any AddCommand::getResult() {
        return m_compId;
    }

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
