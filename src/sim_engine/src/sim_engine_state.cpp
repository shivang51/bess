#include "sim_engine_state.h"
#include "component_catalog.h"
#include <memory>

namespace Bess::SimEngine {

    typedef std::shared_ptr<SimEngineState::TSimComp> TSimCompPtr;

    SimEngineState::SimEngineState() = default;

    SimEngineState::~SimEngineState() = default;

    void SimEngineState::addComponent(const TSimCompPtr &comp) {
        m_components[comp->getUuid()] = comp;
    }

    void SimEngineState::removeComponent(const UUID &uuid) {
        m_components.erase(uuid);
    }

    const std::unordered_map<UUID, TSimCompPtr> &SimEngineState::getComponents() const {
        return m_components;
    }

    void SimEngineState::clearComponents() {
        m_components.clear();
    }

    TSimCompPtr SimEngineState::getComponent(const UUID &uuid) const {
        auto it = m_components.find(uuid);
        if (it != m_components.end()) {
            return it->second;
        }
        return nullptr;
    }

    void SimEngineState::addNet(const Net &net) {
        m_nets[net.getUUID()] = net;
    }

    void SimEngineState::removeNet(const UUID &uuid) {
        m_nets.erase(uuid);
    }

    const std::unordered_map<UUID, Net> &SimEngineState::getNetsMap() const {
        return m_nets;
    }

    void SimEngineState::clearNets() {
        m_nets.clear();
    }

    void SimEngineState::reset() {
        clearNets();
        clearComponents();
    }

    bool SimEngineState::isComponentValid(const UUID &uuid) const {
        return m_components.contains(uuid);
    }

    std::vector<TSimCompPtr> SimEngineState::findCompsByName(
        const std::string &name) const {
        std::vector<TSimCompPtr> result;
        for (const auto &[uuid, comp] : m_components) {
            if (comp->getName() == name) {
                result.push_back(comp);
            }
        }
        return result;
    }

} // namespace Bess::SimEngine

namespace Bess::JsonConvert {
    void toJsonValue(const Bess::SimEngine::SimEngineState &state, Json::Value &j) {
        j["digital_components"] = Json::arrayValue;
        for (const auto &[uuid, comp] : state.getComponents()) {
            Json::Value compJson;
            // JsonConvert::toJsonValue(compJson, *comp);

            // FIXME: Aux data
            // const auto auxData = comp->definition->getAuxData();
            // if (auxData.has_value() && auxData.type() == typeid(Bess::Verilog::VerCompDefAuxData)) {
            //     auto verAuxData = std::any_cast<Bess::Verilog::VerCompDefAuxData>(auxData);
            //     compJson["definition"]["aux_data"] = verAuxData.toJson();
            // }
            j["digital_components"].append(compJson);
        }

        j["nets"] = Json::arrayValue;
        for (const auto &[netUuid, net] : state.getNetsMap()) {
            JsonConvert::toJsonValue(net, j["nets"].append(Json::Value()));
        }
    }

    void fromJsonValue(const Json::Value &j, Bess::SimEngine::SimEngineState &state) {
        state.reset();

        const auto &compCatalog = SimEngine::ComponentCatalog::instance();
        for (const auto &compJson : j["digital_components"]) {
            break;
            // FIXME: toJson
            // auto comp = std::make_shared<SimEngine::Drivers::SimComponent>();
            // auto compDef = std::dynamic_pointer_cast<SimEngine::Drivers::Digital::DigCompDef>(comp->getDefinition());
            // // JsonConvert::fromJsonValue(compJson, *comp);
            //
            // bool isModule = compJson["definition"].isMember("is_module");
            //
            // if (isModule) {
            //     auto moduleDef = std::dynamic_pointer_cast<SimEngine::ModuleDefinition>(comp->getDefinition());
            //     // FIXME: SimFn
            //     // auto simFn = [moduleDef](const std::vector<SimEngine::SlotState> &inputs,
            //     //                          SimEngine::SimTime simTime,
            //     //                          const SimEngine::ComponentState &prevState) {
            //     //     return moduleDef->simulationFunction(inputs, simTime, prevState);
            //     // };
            //     // comp->getDefinition<SimEngine::Drivers::::DigCompDef>()->setSimFn(simFn);
            // } else {
            //     if (compJson["definition"].isMember("aux_data")) {
            //         auto auxDataJson = compJson["definition"]["aux_data"];
            //         // Just to register it in catalog
            //         auto def = Bess::Verilog::getFromAuxDataJson(auxDataJson);
            //     }
            //
            //     if (!compCatalog.isRegistered(comp->getDefinition()->getName())) {
            //         BESS_ERROR("Component definition with name {} is not registered in the catalog. Skipping.",
            //                    comp->getDefinition()->getName());
            //
            //         // temp
            //         BESS_ASSERT(false, compJson.toStyledString());
            //         continue;
            //     }
            //
            //     auto baseDefinition = compCatalog.getComponentDefinition(comp->getDefinition()->getName())
            //                               ->clone();
            //
            //     auto baseDef = std::dynamic_pointer_cast<SimEngine::Drivers:: ::DigCompDef>(baseDefinition);
            //
            //     baseDef->setInputSlotsInfo(compDef->getInputSlotsInfo());
            //     baseDef->setOutputSlotsInfo(compDef->getOutputSlotsInfo());
            //
            //     comp->setDefinition(std::move(baseDef));
            // }
            //
            // // Very important, do no change the order of following ops
            // // As expressions need to be set in auxData
            // compDef->computeExpressionsIfNeeded();
            //
            // // FIXME: Aux data
            // // if (compDef->getAuxData().has_value()) {
            // //     comp->state.auxData = &comp->definition->getAuxData();
            // // }
            //
            // state.addComponent(comp);
        }

        for (const auto &netJson : j["nets"]) {
            SimEngine::Net net;
            JsonConvert::fromJsonValue(netJson, net);
            state.addNet(net);
        }
    }
} // namespace Bess::JsonConvert
