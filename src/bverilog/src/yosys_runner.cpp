#include "bverilog/yosys_runner.h"
#include "bverilog/yosys_json_parser.h"
#include <filesystem>
#include <fstream>
#include <json/reader.h>
#include <sstream>
#include <stdexcept>
#include <string>
#include <unordered_set>

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

        std::filesystem::path normalizePath(const std::filesystem::path &path) {
            if (path.empty()) {
                return path;
            }

            try {
                return std::filesystem::weakly_canonical(path);
            } catch (const std::exception &) {
                return std::filesystem::absolute(path).lexically_normal();
            }
        }

        void appendUniquePath(const std::filesystem::path &path,
                              std::vector<std::filesystem::path> &ordered,
                              std::unordered_set<std::string> &seen) {
            if (path.empty()) {
                return;
            }

            const auto normalized = normalizePath(path);
            const auto key = normalized.generic_string();
            if (seen.insert(key).second) {
                ordered.push_back(normalized);
            }
        }

        std::vector<std::filesystem::path> buildSourceFiles(
            const std::vector<std::filesystem::path> &verilogFiles,
            const YosysRunnerConfig &config) {
            std::vector<std::filesystem::path> sourceFiles;
            std::unordered_set<std::string> seen;

            for (const auto &sourceFile : verilogFiles) {
                appendUniquePath(sourceFile, sourceFiles, seen);
            }

            for (const auto &sourceFile : config.additionalSourceFiles) {
                appendUniquePath(sourceFile, sourceFiles, seen);
            }

            if (sourceFiles.empty()) {
                throw std::runtime_error("No Verilog source files were provided for Yosys import");
            }

            for (const auto &sourceFile : sourceFiles) {
                if (!std::filesystem::exists(sourceFile) || !std::filesystem::is_regular_file(sourceFile)) {
                    throw std::runtime_error("Verilog source file does not exist: " + sourceFile.string());
                }
            }

            return sourceFiles;
        }

        std::vector<std::filesystem::path> buildIncludeDirectories(
            const std::vector<std::filesystem::path> &sourceFiles,
            const YosysRunnerConfig &config) {
            std::vector<std::filesystem::path> includeDirectories;
            std::unordered_set<std::string> seen;

            for (const auto &sourceFile : sourceFiles) {
                appendUniquePath(sourceFile.parent_path(), includeDirectories, seen);
            }

            for (const auto &includeDirectory : config.includeDirectories) {
                appendUniquePath(includeDirectory, includeDirectories, seen);
            }

            for (const auto &includeDirectory : includeDirectories) {
                if (!std::filesystem::exists(includeDirectory) || !std::filesystem::is_directory(includeDirectory)) {
                    throw std::runtime_error("Verilog include directory does not exist: " + includeDirectory.string());
                }
            }

            return includeDirectories;
        }

        std::string buildUniqueStem(const std::vector<std::filesystem::path> &sourceFiles) {
            size_t hashValue = 1469598103934665603ULL;

            const auto hashCombine = [&](size_t value) {
                hashValue ^= value + 0x9e3779b97f4a7c15ULL + (hashValue << 6U) + (hashValue >> 2U);
            };

            for (const auto &sourceFile : sourceFiles) {
                hashCombine(std::hash<std::string>{}(sourceFile.generic_string()));
                hashCombine(static_cast<size_t>(std::filesystem::file_size(sourceFile)));

                const auto timestamp = std::filesystem::last_write_time(sourceFile).time_since_epoch().count();
                hashCombine(static_cast<size_t>(timestamp));
            }

            return "multi_" + std::to_string(hashValue);
        }

        bool containsSystemVerilogSource(const std::vector<std::filesystem::path> &sourceFiles) {
            for (const auto &sourceFile : sourceFiles) {
                const auto extension = sourceFile.extension().string();
                if (extension == ".sv" || extension == ".svh") {
                    return true;
                }
            }
            return false;
        }
    } // namespace

    std::string getDefaultYosysReleaseUrl() {
        return "https://github.com/YosysHQ/yosys/releases/download/v0.63/yosys.tar.gz";
    }

    Json::Value runYosysForJson(const std::vector<std::filesystem::path> &verilogFiles,
                                const YosysRunnerConfig &config) {
        const auto sourceFiles = buildSourceFiles(verilogFiles, config);
        const auto includeDirectories = buildIncludeDirectories(sourceFiles, config);

        const auto tempRoot = std::filesystem::temp_directory_path() / "bess_yosys";
        std::filesystem::create_directories(tempRoot);

        const auto uniqueStem = buildUniqueStem(sourceFiles);
        const auto scriptPath = tempRoot / (uniqueStem + ".ys");
        const auto jsonPath = tempRoot / (uniqueStem + ".json");

        std::ostringstream script;
        script << "read_verilog";
        if (containsSystemVerilogSource(sourceFiles)) {
            script << " -sv";
        }
        for (const auto &includeDirectory : includeDirectories) {
            script << " -I " << quote(includeDirectory);
        }
        for (const auto &sourceFile : sourceFiles) {
            script << " " << quote(sourceFile);
        }
        script << "\n";
        // Demote inout ports to directional ports where possible so importer can map IO boundaries.
        script << "deminout\n";
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

    Json::Value runYosysForJson(const std::filesystem::path &verilogFile,
                                const YosysRunnerConfig &config) {
        return runYosysForJson(std::vector<std::filesystem::path>{verilogFile}, config);
    }

    Design importVerilogToDesign(const std::vector<std::filesystem::path> &verilogFiles,
                                 const YosysRunnerConfig &config) {
        return parseDesignFromYosysJson(runYosysForJson(verilogFiles, config), config.topModuleName);
    }

    Design importVerilogToDesign(const std::filesystem::path &verilogFile,
                                 const YosysRunnerConfig &config) {
        return importVerilogToDesign(std::vector<std::filesystem::path>{verilogFile}, config);
    }
} // namespace Bess::Verilog
