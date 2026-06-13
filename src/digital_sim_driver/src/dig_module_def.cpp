#include "dig_module_def.h"
#include "bess_core/g_app_context.h"
#include "bess_core/project_context.h"
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

        auto &appCtx = Bess::GAppContext::getInstance();
        auto projectCtx = appCtx.getSubSystem<Bess::ProjectContext>();
        auto &simEngine = projectCtx->getSimEngine();

        const auto &inpDef = simEngine.getComponentDefinition(m_input);
        clone->m_input = simEngine.addComponent(inpDef);

        const auto &outDef = simEngine.getComponentDefinition(m_output);
        clone->m_output = simEngine.addComponent(outDef);

        return clone;
    }

    std::shared_ptr<ModuleDefinition> ModuleDefinition::createNew() {
        auto &appCtx = Bess::GAppContext::getInstance();
        auto projectCtx = appCtx.getSubSystem<Bess::ProjectContext>();
        auto &simEngine = projectCtx->getSimEngine();

        auto moduleDef = std::make_shared<ModuleDefinition>();

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
        const auto &inpDef = catalog.getComponentDefinition("Input");
        BESS_ASSERT(inpDef, "Input component definition not found in catalog");
        moduleDef->m_input = simEngine.addComponent(inpDef);

        const auto &outDef = catalog.getComponentDefinition("Output");
        BESS_ASSERT(outDef, "Output component definition not found in catalog");
        moduleDef->m_output = simEngine.addComponent(outDef);

        return moduleDef;
    }

    ModuleDefinition::TDigSimFnDataPtr
    ModuleDefinition::simFunction(const TDigSimFnDataPtr &data) {
        bool isInputChanged = false;
        const auto &inputs = data->inputStates;
        const auto &prevState = data->prevState;

        auto &appCtx = Bess::GAppContext::getInstance();
        auto projectCtx = appCtx.getSubSystem<Bess::ProjectContext>();
        auto &simEngine = projectCtx->getSimEngine();

        const auto &outputState = simEngine.getComponentState(m_output);

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
