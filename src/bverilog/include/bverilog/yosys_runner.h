#pragma once

#include "bverilog/types.h"
#include <filesystem>
#include <json/value.h>
#include <optional>
#include <string>
#include <vector>

namespace Bess::Verilog {
    struct BESS_API YosysRunnerConfig {
        std::filesystem::path executablePath = "yosys";
        std::optional<std::string> topModuleName;
        std::vector<std::filesystem::path> additionalSourceFiles;
        std::vector<std::filesystem::path> includeDirectories;
        std::vector<std::string> extraPasses;
    };

    BESS_API std::string getDefaultYosysReleaseUrl();

    BESS_API Json::Value runYosysForJson(const std::vector<std::filesystem::path> &verilogFiles,
                                         const YosysRunnerConfig &config = {});

    BESS_API Json::Value runYosysForJson(const std::filesystem::path &verilogFile,
                                         const YosysRunnerConfig &config = {});

    BESS_API Design importVerilogToDesign(const std::vector<std::filesystem::path> &verilogFiles,
                                          const YosysRunnerConfig &config = {});

    BESS_API Design importVerilogToDesign(const std::filesystem::path &verilogFile,
                                          const YosysRunnerConfig &config = {});
} // namespace Bess::Verilog
