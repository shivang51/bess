#pragma once

#include "bess_api.h"
#include "bverilog/types.h"
#include "bverilog/yosys_runner.h"
#include "common/bess_assert.h"
#include "common/bess_uuid.h"
#include "simulation_engine.h"
#include "json/value.h"
#include <filesystem>
#include <string>
#include <unordered_map>
#include <vector>

namespace Bess::Verilog {
    struct BESS_API VerCompDefAuxData {
        typedef std::function<Json::Value()> SerFunc;
        std::string id;
        static constexpr std::string_view type = "VerCompDefAuxData";
        SerFunc toJsonCb = nullptr;

        Json::Value toJson() const {
            BESS_ASSERT(toJsonCb,
                        std::format("toJson callback is not set for VerCompDefAuxData with id: {}", id));
            Json::Value j;
            j["id"] = id;
            j["type"] = type;
            if (toJsonCb) {
                j["data"] = toJsonCb();
            }
            return j;
        }
    };

    struct BESS_API ImportedSlotEndpoint {
        UUID componentId = UUID::null;
        SimEngine::SlotType slotType = SimEngine::SlotType::digitalInput;
        int slotIndex = 0;
    };

    struct BESS_API ImportedModuleInstance {
        std::string instancePath;
        std::string parentInstancePath;
        bool isFlattened = true;
        UUID componentId = UUID::null;
        UUID moduleInputId = UUID::null;
        UUID moduleOutputId = UUID::null;
        std::string definitionName;
        std::vector<std::string> inputSlotNames;
        std::vector<std::string> outputSlotNames;
        std::vector<std::vector<ImportedSlotEndpoint>> internalInputSinks;
        std::vector<std::vector<ImportedSlotEndpoint>> internalOutputDrivers;
    };

    struct BESS_API SimEngineImportResult {
        std::string topModuleName;
        ImportedModuleInstance top;
        std::unordered_map<std::string, UUID> topInputComponents;
        std::unordered_map<std::string, UUID> topOutputComponents;
        std::vector<UUID> createdComponentIds;
        std::unordered_map<std::string, ImportedModuleInstance> instancesByPath;
        std::unordered_map<UUID, std::string> componentInstancePathById;
    };

    BESS_API SimEngineImportResult importDesignIntoSimulationEngine(
        const Design &design,
        Bess::SimEngine::SimulationEngine &engine,
        const std::optional<std::string> &topModuleName = std::nullopt);

    BESS_API SimEngineImportResult importVerilogFileIntoSimulationEngine(
        const std::filesystem::path &verilogFile,
        Bess::SimEngine::SimulationEngine &engine,
        const YosysRunnerConfig &config = {});

    BESS_API SimEngineImportResult importVerilogFilesIntoSimulationEngine(
        const std::vector<std::filesystem::path> &verilogFiles,
        Bess::SimEngine::SimulationEngine &engine,
        const YosysRunnerConfig &config = {});

    BESS_API std::shared_ptr<SimEngine::ComponentDefinition> getFromAuxDataJson(Json::Value auxDataJson);
} // namespace Bess::Verilog
