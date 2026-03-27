#include "bverilog/yosys_runner.h"
#include "bverilog/yosys_json_parser.h"
#include <filesystem>
#include <fstream>
#include <json/reader.h>
#include <sstream>
#include <stdexcept>
#include <string>

namespace Bess::Verilog {
    namespace {
        std::string quote(const std::filesystem::path &path) {
            std::string value = path.string();
            std::string escaped;
            escaped.reserve(value.size() + 2);
            escaped.push_back('"');
            for (const char ch : value) {
                if (ch == '"' || ch == '\\') {
                    escaped.push_back('\\');
                }
                escaped.push_back(ch);
            }
            escaped.push_back('"');
            return escaped;
        }

        Json::Value parseJsonFile(const std::filesystem::path &path) {
            std::ifstream stream(path);
            if (!stream.is_open()) {
                throw std::runtime_error("Failed to open Yosys JSON output: " + path.string());
            }

            Json::CharReaderBuilder builder;
            builder["collectComments"] = false;
            Json::Value root;
            std::string errors;
            if (!Json::parseFromStream(builder, stream, &root, &errors)) {
                throw std::runtime_error("Failed to parse Yosys JSON output: " + errors);
            }
            return root;
        }
    } // namespace

    std::string getDefaultYosysReleaseUrl() {
        return "https://github.com/YosysHQ/yosys/releases/download/v0.63/yosys.tar.gz";
    }

    Json::Value runYosysForJson(const std::filesystem::path &verilogFile,
                                const YosysRunnerConfig &config) {
        if (!std::filesystem::exists(verilogFile)) {
            throw std::runtime_error("Verilog file does not exist: " + verilogFile.string());
        }

        const auto tempRoot = std::filesystem::temp_directory_path() / "bess_yosys";
        std::filesystem::create_directories(tempRoot);

        const auto uniqueStem = verilogFile.stem().string() + "_" + std::to_string(std::filesystem::file_size(verilogFile));
        const auto scriptPath = tempRoot / (uniqueStem + ".ys");
        const auto jsonPath = tempRoot / (uniqueStem + ".json");

        std::ostringstream script;
        script << "read_verilog " << quote(verilogFile) << "\n";
        script << "hierarchy -check ";
        if (config.topModuleName.has_value()) {
            script << "-top " << *config.topModuleName << "\n";
        } else {
            script << "-auto-top\n";
        }
        script << "proc\n";
        script << "opt\n";
        script << "memory\n";
        script << "opt\n";
        script << "techmap\n";
        script << "opt\n";
        script << "simplemap\n";
        for (const auto &extraPass : config.extraPasses) {
            script << extraPass << "\n";
        }
        script << "clean\n";
        script << "write_json " << quote(jsonPath) << "\n";

        {
            std::ofstream scriptFile(scriptPath);
            if (!scriptFile.is_open()) {
                throw std::runtime_error("Failed to create temporary Yosys script: " + scriptPath.string());
            }
            scriptFile << script.str();
        }

        const auto command = quote(config.executablePath) + " -q -s " + quote(scriptPath);
        const int exitCode = std::system(command.c_str());
        if (exitCode != 0) {
            throw std::runtime_error("Yosys command failed with exit code " + std::to_string(exitCode) +
                                     ". Configure a usable executable from " + getDefaultYosysReleaseUrl());
        }

        return parseJsonFile(jsonPath);
    }

    Design importVerilogToDesign(const std::filesystem::path &verilogFile,
                                 const YosysRunnerConfig &config) {
        return parseDesignFromYosysJson(runYosysForJson(verilogFile, config), config.topModuleName);
    }
} // namespace Bess::Verilog
