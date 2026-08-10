#include "dig_module_def.h"
#include "common/bess_assert.h"
#include "component_catalog.h"
#include "sim_driver/sim_driver.h"
#include "simulation_engine.h"
#include <memory>

namespace Bess::SimEngine {
    std::shared_ptr<Drivers::CompDef> ModuleDefinition::clone() const {
        auto clone = std::make_shared<ModuleDefinition>(*this);

        clone->setSimFn([clone](const TDigSimFnDataPtr &data) {
            return clone->simFunction(data);
        });

        const auto &catalog = ComponentCatalog::instance();

        if (!m_engine) {
            BESS_ERROR("Cannot clone module definition without a simulation "
                       "engine");
            return nullptr;
        }

        const auto &inpDef = m_engine->getComponentDefinition(m_input);
        clone->m_input = m_engine->addComponent(inpDef);

        const auto &outDef = m_engine->getComponentDefinition(m_output);
        clone->m_output = m_engine->addComponent(outDef);

        return clone;
    }

    std::shared_ptr<ModuleDefinition>
    ModuleDefinition::createNew(SimulationEngine &simEngine) {
        auto moduleDef = std::make_shared<ModuleDefinition>();
        moduleDef->setEngine(&simEngine);

        moduleDef->setName("New Module");
        moduleDef->setGroupName("Modules");

        moduleDef->m_inputSlotsInfo = SlotsGroupInfo{
            .type = SlotsGroupType::input,
            .isResizeable = false,
            .count = 1,
        };
        moduleDef->m_outputSlotsInfo = SlotsGroupInfo{
            .type = SlotsGroupType::output,
            .isResizeable = false,
            .count = 1,
        };

        moduleDef->setSimFn([moduleDef](const TDigSimFnDataPtr &data) {
            return moduleDef->simFunction(data);
        });

        const auto &catalog = ComponentCatalog::instance();

        // create a input and output component for the module
        auto inpDef = catalog.getComponentDefinition("Input");
        if (!inpDef) {
            inpDef = catalog.getComponentDefinition("Digital Input");
        }
        if (!inpDef) {
            BESS_ERROR("Input component definition not found in catalog");
            return nullptr;
        }
        moduleDef->m_input = simEngine.addComponent(inpDef);
        if (moduleDef->m_input == UUID::null) {
            BESS_ERROR("Could not create module input component");
            return nullptr;
        }

        auto outDef = catalog.getComponentDefinition("Output");
        if (!outDef) {
            outDef = catalog.getComponentDefinition("Digital Output");
        }
        if (!outDef) {
            simEngine.deleteComponent(moduleDef->m_input);
            BESS_ERROR("Output component definition not found in catalog");
            return nullptr;
        }
        moduleDef->m_output = simEngine.addComponent(outDef);
        if (moduleDef->m_output == UUID::null) {
            simEngine.deleteComponent(moduleDef->m_input);
            BESS_ERROR("Could not create module output component");
            return nullptr;
        }

        return moduleDef;
    }

    ModuleDefinition::TDigSimFnDataPtr
    ModuleDefinition::simFunction(const TDigSimFnDataPtr &data) {
        bool isInputChanged = false;
        const auto &inputs = data->inputStates;
        const auto &prevState = data->prevState;

        if (!m_engine) {
            BESS_ERROR("Cannot simulate module definition without a "
                       "simulation engine");
            return data;
        }

        const auto &outputState = m_engine->getComponentState(m_output);

        bool isChanged = false;

        for (size_t i = 0; i < outputState.inputStates.size(); ++i) {
            data->outputStates[i] = outputState.inputStates[i];
            if (data->outputStates[i].getLogicState() !=
                prevState.outputStates[i].getLogicState()) {
                isChanged = true;
            }
        }
        data->simDependants = isChanged;
        return data;
    }

    std::string ModuleDefinition::getTypeName() const {
        return TypeName;
    }

    Json::Value ModuleDefinition::toJson() const {
        Json::Value json = Drivers::Digital::DigCompDef::toJson();

        JsonConvert::toJsonValue(m_input, json["input"]);
        JsonConvert::toJsonValue(m_output, json["output"]);

        return json;
    }

    void ModuleDefinition::loadJson(const Json::Value &json) {
        if (!json.isObject()) {
            return;
        }

        Drivers::Digital::DigCompDef::loadJson(json);

        if (json.isMember("input")) {
            JsonConvert::fromJsonValue(json["input"], m_input);
        }

        if (json.isMember("output")) {
            JsonConvert::fromJsonValue(json["output"], m_output);
        }
    }
} // namespace Bess::SimEngine
