#pragma once

#include "bverilog/types.h"
#include "bverilog/yosys_runner.h"
#include "common/bess_uuid.h"
#include "simulation_engine.h"
#include <filesystem>
#include <string>
#include <unordered_map>

namespace Bess::Verilog {
    struct BESS_API ImportedModuleInstance {
        std::string instancePath;
        std::string parentInstancePath;
        bool isFlattened = true;
        UUID componentId = UUID::null;
        UUID moduleInputId = UUID::null;
        UUID moduleOutputId = UUID::null;
        std::string definitionName;
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
} // namespace Bess::Verilog
