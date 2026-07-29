#include "bess_core/g_app_context.h"
#include "project_session/project_session.h"
#include "bverilog/sim_engine_importer.h"
#include "bverilog/yosys_runner.h"

#include <pybind11/functional.h>
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>
#include <pybind11/stl/filesystem.h>

#include <optional>

namespace py = pybind11;

using namespace Bess::Verilog;

void bind_verilog(py::module_ &m) {
    py::enum_<PortDirection>(m, "PortDirection")
        .value("INPUT", PortDirection::input)
        .value("OUTPUT", PortDirection::output)
        .value("INOUT", PortDirection::inout)
        .export_values();

    py::class_<SignalBit>(m, "SignalBit")
        .def(py::init<>())
        .def(py::init<const SignalBit &>())
        .def_static("from_net", &SignalBit::fromNet, py::arg("bit_id"))
        .def_static("from_constant", &SignalBit::fromConstant, py::arg("value"))
        .def("is_net", &SignalBit::isNet)
        .def("is_constant", &SignalBit::isConstant)
        .def("to_string", &SignalBit::toString)
        .def_readwrite("net_id", &SignalBit::netId)
        .def_readwrite("constant", &SignalBit::constant);

    py::class_<Port>(m, "Port")
        .def(py::init<>())
        .def(py::init<const Port &>())
        .def_readwrite("name", &Port::name)
        .def_readwrite("direction", &Port::direction)
        .def_readwrite("bits", &Port::bits);

    py::class_<Cell>(m, "Cell")
        .def(py::init<>())
        .def(py::init<const Cell &>())
        .def_readwrite("name", &Cell::name)
        .def_readwrite("type", &Cell::type)
        .def_readwrite("connections", &Cell::connections)
        .def_readwrite("port_directions", &Cell::portDirections)
        .def_readwrite("parameters", &Cell::parameters)
        .def_readwrite("attributes", &Cell::attributes);

    py::class_<Module>(m, "Module")
        .def(py::init<>())
        .def(py::init<const Module &>())
        .def_readwrite("name", &Module::name)
        .def_readwrite("ports", &Module::ports)
        .def_readwrite("cells", &Module::cells)
        .def_readwrite("attributes", &Module::attributes)
        .def("find_port",
             &Module::findPort,
             py::arg("port_name"),
             py::return_value_policy::reference_internal);

    py::class_<Design>(m, "Design")
        .def(py::init<>())
        .def(py::init<const Design &>())
        .def_readwrite("modules", &Design::modules)
        .def_readwrite("top_module_name", &Design::topModuleName)
        .def("find_module",
             &Design::findModule,
             py::arg("module_name"),
             py::return_value_policy::reference_internal);

    py::class_<YosysRunnerConfig>(m, "YosysRunnerConfig")
        .def(py::init<>())
        .def(py::init<const YosysRunnerConfig &>())
        .def_readwrite("executable_path", &YosysRunnerConfig::executablePath)
        .def_readwrite("top_module_name", &YosysRunnerConfig::topModuleName)
        .def_readwrite("additional_source_files",
                       &YosysRunnerConfig::additionalSourceFiles)
        .def_readwrite("include_directories",
                       &YosysRunnerConfig::includeDirectories)
        .def_readwrite("extra_passes", &YosysRunnerConfig::extraPasses);

    py::class_<ImportedSlotEndpoint>(m, "ImportedSlotEndpoint")
        .def(py::init<>())
        .def(py::init<const ImportedSlotEndpoint &>())
        .def_readwrite("component_id", &ImportedSlotEndpoint::componentId)
        .def_readwrite("direction", &ImportedSlotEndpoint::direction)
        .def_readwrite("signal_kind", &ImportedSlotEndpoint::signalKind)
        .def_readwrite("port_index", &ImportedSlotEndpoint::portIndex);

    py::class_<ImportedModuleInstance>(m, "ImportedModuleInstance")
        .def(py::init<>())
        .def(py::init<const ImportedModuleInstance &>())
        .def_readwrite("instance_path", &ImportedModuleInstance::instancePath)
        .def_readwrite("parent_instance_path",
                       &ImportedModuleInstance::parentInstancePath)
        .def_readwrite("is_flattened", &ImportedModuleInstance::isFlattened)
        .def_readwrite("component_id", &ImportedModuleInstance::componentId)
        .def_readwrite("module_input_id",
                       &ImportedModuleInstance::moduleInputId)
        .def_readwrite("module_output_id",
                       &ImportedModuleInstance::moduleOutputId)
        .def_readwrite("definition_name",
                       &ImportedModuleInstance::definitionName)
        .def_readwrite("input_slot_names",
                       &ImportedModuleInstance::inputSlotNames)
        .def_readwrite("output_slot_names",
                       &ImportedModuleInstance::outputSlotNames)
        .def_readwrite("internal_input_sinks",
                       &ImportedModuleInstance::internalInputSinks)
        .def_readwrite("internal_output_drivers",
                       &ImportedModuleInstance::internalOutputDrivers);

    py::class_<SimEngineImportResult>(m, "SimEngineImportResult")
        .def(py::init<>())
        .def(py::init<const SimEngineImportResult &>())
        .def_readwrite("top_module_name", &SimEngineImportResult::topModuleName)
        .def_readwrite("top", &SimEngineImportResult::top)
        .def_readwrite("top_input_components",
                       &SimEngineImportResult::topInputComponents)
        .def_readwrite("top_output_components",
                       &SimEngineImportResult::topOutputComponents)
        .def_readwrite("created_component_ids",
                       &SimEngineImportResult::createdComponentIds)
        .def_readwrite("instances_by_path",
                       &SimEngineImportResult::instancesByPath)
        .def_readwrite("component_instance_path_by_id",
                       &SimEngineImportResult::componentInstancePathById);

    m.def("get_default_yosys_release_url", &getDefaultYosysReleaseUrl);

    m.def(
        "run_yosys_for_json",
        [](const std::vector<std::filesystem::path> &verilog_files) {
            return runYosysForJson(verilog_files, YosysRunnerConfig{});
        },
        py::arg("verilog_files"));
    m.def(
        "run_yosys_for_json",
        [](const std::vector<std::filesystem::path> &verilog_files,
           const YosysRunnerConfig &config) {
            return runYosysForJson(verilog_files, config);
        },
        py::arg("verilog_files"),
        py::arg("config"));
    m.def(
        "run_yosys_for_json",
        [](const std::filesystem::path &verilog_file) {
            return runYosysForJson(verilog_file, YosysRunnerConfig{});
        },
        py::arg("verilog_file"));
    m.def(
        "run_yosys_for_json",
        [](const std::filesystem::path &verilog_file,
           const YosysRunnerConfig &config) {
            return runYosysForJson(verilog_file, config);
        },
        py::arg("verilog_file"),
        py::arg("config"));

    m.def(
        "import_verilog_to_design",
        [](const std::vector<std::filesystem::path> &verilog_files) {
            return importVerilogToDesign(verilog_files, YosysRunnerConfig{});
        },
        py::arg("verilog_files"));
    m.def(
        "import_verilog_to_design",
        [](const std::vector<std::filesystem::path> &verilog_files,
           const YosysRunnerConfig &config) {
            return importVerilogToDesign(verilog_files, config);
        },
        py::arg("verilog_files"),
        py::arg("config"));
    m.def(
        "import_verilog_to_design",
        [](const std::filesystem::path &verilog_file) {
            return importVerilogToDesign(verilog_file, YosysRunnerConfig{});
        },
        py::arg("verilog_file"));
    m.def(
        "import_verilog_to_design",
        [](const std::filesystem::path &verilog_file,
           const YosysRunnerConfig &config) {
            return importVerilogToDesign(verilog_file, config);
        },
        py::arg("verilog_file"),
        py::arg("config"));

    m.def(
        "import_design_into_simulation_engine",
        [](const Design &design,
           const std::optional<std::string> &top_module_name) {
            return importDesignIntoSimulationEngine(
                design,
                Bess::GAppContext::getInstance()
                    .getSubSystem<Bess::ProjectSession>()
                    ->sim(),
                top_module_name);
        },
        py::arg("design"),
        py::arg("top_module_name") = std::nullopt);

    m.def(
        "import_verilog_file_into_simulation_engine",
        [](const std::filesystem::path &verilog_file) {
            return importVerilogFileIntoSimulationEngine(
                verilog_file,
                Bess::GAppContext::getInstance()
                    .getSubSystem<Bess::ProjectSession>()
                    ->sim(),
                YosysRunnerConfig{});
        },
        py::arg("verilog_file"));
    m.def(
        "import_verilog_file_into_simulation_engine",
        [](const std::filesystem::path &verilog_file,
           const YosysRunnerConfig &config) {
            return importVerilogFileIntoSimulationEngine(
                verilog_file,
                Bess::GAppContext::getInstance()
                    .getSubSystem<Bess::ProjectSession>()
                    ->sim(),
                config);
        },
        py::arg("verilog_file"),
        py::arg("config"));

    m.def(
        "import_verilog_files_into_simulation_engine",
        [](const std::vector<std::filesystem::path> &verilog_files) {
            return importVerilogFilesIntoSimulationEngine(
                verilog_files,
                Bess::GAppContext::getInstance()
                    .getSubSystem<Bess::ProjectSession>()
                    ->sim(),
                YosysRunnerConfig{});
        },
        py::arg("verilog_files"));
    m.def(
        "import_verilog_files_into_simulation_engine",
        [](const std::vector<std::filesystem::path> &verilog_files,
           const YosysRunnerConfig &config) {
            return importVerilogFilesIntoSimulationEngine(
                verilog_files,
                Bess::GAppContext::getInstance()
                    .getSubSystem<Bess::ProjectSession>()
                    ->sim(),
                config);
        },
        py::arg("verilog_files"),
        py::arg("config"));

    m.def("get_from_aux_data_json",
          &getFromAuxDataJson,
          py::arg("aux_data_json"));
}
