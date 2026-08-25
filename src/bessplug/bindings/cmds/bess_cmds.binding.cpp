#include "bess_core/scene_driver.h"
#include "common/bess_uuid.h"
#include "common/logger.h"
#include "common/types.h"
#include "component_catalog.h"
#include "dig_sim_driver.h"
#include "pages/main_page/scene_components/connection_scene_component.h" // IWYU pragma: keep
#include "pages/main_page/scene_components/sim_scene_component.h"
#include "simulation_engine.h"
#include "ui/project_api.h"
#include "ui/ui_main/component_explorer.h"
#include <pybind11/eval.h>
#include <pybind11/functional.h>
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>
#include <type_traits>

namespace py = pybind11;

struct CmdResult {
    py::object result = py::none();
    std::string error;
};

struct AsyncScriptStatus {
    std::atomic<bool> isRunning{false};
    std::string error;
    std::string log;
};

class ScriptLogger {
  public:
    void write(const std::string &text) {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_output += text;
    }

    void flush() {
    }

    std::string popLogs() {
        std::lock_guard<std::mutex> lock(m_mutex);
        std::string out = m_output;
        m_output.clear();
        return out;
    }

    const std::string &getLogs() {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_output;
    }

  private:
    std::string m_output;
    std::mutex m_mutex;
};

const auto scriptLogger = std::make_shared<ScriptLogger>();
const auto status = std::make_shared<AsyncScriptStatus>();

std::shared_ptr<Bess::SimEngine::Drivers::Digital::DigSimComp>
findUniqueDigCompByName(const std::string &compName);
std::shared_ptr<Bess::SimEngine::Drivers::Digital::DigSimComp>
findDigCompBySceneId(uint64_t compId);
void bind_cmd_results(py::module &m);
void bind_async_script_status(py::module &m);
void bind_script_logger(py::module &m);

template <typename... Args> auto collectArgs(Args... args) {
    return std::vector<std::common_type_t<Args...>>{args...};
}

void bind_cmds(py::module &m) {
    bind_cmd_results(m);
    bind_async_script_status(m);
    bind_script_logger(m);

    struct CmdDefs {
        std::string name;
        std::string desc;
        std::vector<py::arg> args;
        std::string _argsCache;
    };

    static std::vector<CmdDefs> cmdDefs;

    const auto addCmd = [&m](const std::string &name,
                             const auto &function,
                             const std::string &desc,
                             const auto &...args) {
        m.def(name.c_str(), function, desc.c_str(), args...);

        std::vector<py::arg> vec{args...};
        cmdDefs.push_back({name, desc, std::move(vec)});
    };

    auto addCompFn = [](const std::string &name) -> CmdResult {
        const auto &catalog = Bess::SimEngine::ComponentCatalog::instance();
        const auto &def = catalog.findDefByName(name);

        if (!def) {
            return {py::none(), "Component definition not found"};
        }

        auto compId = Bess::UI::ComponentExplorer::createComponent(def);

        if (compId == Bess::UUID::null) {
            return {py::none(), "Failed to add component"};
        }

        return {py::cast(compId), ""};
    };

    addCmd("add",
           addCompFn,
           "Adds a component to the current circuit by definition name.",
           py::arg("comp_name"));

    // N stands for by name
    auto getInpStatesN = [](const std::string &compName) -> CmdResult {
        const auto &comp = findUniqueDigCompByName(compName);
        if (!comp) {
            return {py::none(), "Component not found"};
        }

        std::vector<Bess::SimEngine::LogicState> states;
        states.reserve(comp->getInputStates().size());
        for (const auto &state : comp->getInputStates()) {
            states.push_back(state.getLogicState());
        }
        return {py::cast(states), ""};
    };

    addCmd("get_inp_states_n",
           getInpStatesN,
           "Gets the states of any components input slots. "
           "For it work make sure component name is unique in your circuit",
           py::arg("comp_name"));

    auto getInpStates = [](uint64_t compId) -> CmdResult {
        const auto &comp = findDigCompBySceneId(compId);
        if (!comp) {
            return {py::none(), "Component not found"};
        }

        std::vector<Bess::SimEngine::LogicState> states;
        states.reserve(comp->getInputStates().size());
        for (const auto &state : comp->getInputStates()) {
            states.push_back(state.getLogicState());
        }
        return {py::cast(states), ""};
    };

    addCmd("get_inp_states",
           getInpStates,
           "Gets the states of any components input slots.",
           py::arg("comp_id"));

    auto getOutStatesN = [](const std::string &compName) -> CmdResult {
        const auto &comp = findUniqueDigCompByName(compName);
        if (!comp) {
            return {py::none(), "Component not found"};
        }

        std::vector<Bess::SimEngine::LogicState> states;
        states.reserve(comp->getOutputStates().size());
        for (const auto &state : comp->getOutputStates()) {
            states.push_back(state.getLogicState());
        }
        return {py::cast(states), ""};
    };

    addCmd("get_out_states_n",
           getOutStatesN,
           "Gets the states of any components output slots. "
           "For it work make sure component name is unique in your circuit",
           py::arg("comp_name"));

    auto getOutStates = [](uint64_t compId) -> CmdResult {
        const auto &comp = findDigCompBySceneId(compId);

        if (!comp) {
            return {py::none(), "Component not found"};
        }

        std::vector<Bess::SimEngine::LogicState> states;
        states.reserve(comp->getOutputStates().size());
        for (const auto &state : comp->getOutputStates()) {
            states.push_back(state.getLogicState());
        }
        return {py::cast(states), ""};
    };

    addCmd("get_out_states",
           getOutStates,
           "Gets the states of any components output slots.",
           py::arg("comp_id"));

    auto setInpStateNFn =
        [](const std::string &compName,
           int slotIdx,
           const Bess::SimEngine::LogicState &state) -> CmdResult {
        const auto &comp = findUniqueDigCompByName(compName);

        if (!comp) {
            return {py::none(), "Component not found"};
        }

        if (comp->getDefinition<Bess::SimEngine::Drivers::Digital::DigCompDef>()
                ->getBehaviorType() !=
            Bess::SimEngine::ComponentBehaviorType::input) {

            BESS_ERROR("Component '{}' is not an input component. "
                       "Only input components can have their input state set.",
                       compName);
            return {py::none(), "Component is not an input component"};
        }

        auto &simEngine = Bess::UI::Proj::sim();
        simEngine.setOutputPortState(comp->getUuid(), slotIdx, state);
        return {py::cast(true), ""};
    };

    // id is of scene component
    addCmd("set_inp_comp_state_n",
           setInpStateNFn,
           "Sets the state of input coponent slot. "
           "For it work make sure input names are unique in your circuit",
           py::arg("comp_name"),
           py::arg("slot_idx"),
           py::arg("state"));

    auto setInpStateFn =
        [](uint64_t compId,
           int slotIdx,
           const Bess::SimEngine::LogicState &state) -> CmdResult {
        const auto &comp = findDigCompBySceneId(compId);
        if (!comp) {
            return {py::none(), "Component not found"};
        }

        if (comp->getDefinition<Bess::SimEngine::Drivers::Digital::DigCompDef>()
                ->getBehaviorType() !=
            Bess::SimEngine::ComponentBehaviorType::input) {

            BESS_ERROR("Component with id {} is not an input component. "
                       "Only input components can have their input state set.",
                       compId);
            return {py::none(), "Component is not an input component"};
        }

        auto &simEngine = Bess::UI::Proj::sim();
        simEngine.setOutputPortState(comp->getUuid(), slotIdx, state);
        return {py::cast(true), ""};
    };

    auto setScalarPortState =
        [](uint64_t compId,
           int slotIdx,
           const Bess::SimEngine::LogicState &state) -> CmdResult {
        const auto &comp = findDigCompBySceneId(compId);
        if (!comp) {
            return {py::none(), "Component not found"};
        }

        return {.error = ""};
    };

    // connect slots

    auto connectSlotsFn = [](const Bess::UUID &fromCompId,
                             const std::string &fromDir,
                             int fromPortIdx,
                             const Bess::UUID &toCompId,
                             const std::string &toDir,
                             int toPortIdx) -> CmdResult {
        auto &sceneDriver = Bess::UI::Proj::scenes();
        const auto scene = sceneDriver.getActiveScene();
        if (!scene) {
            return {py::none(), "No active scene"};
        }

        if (fromDir != "inp" && fromDir != "out") {
            return {py::none(),
                    "Invalid slot direction. It should be either 'inp' or "
                    "'out'"};
        }

        if (toDir != "inp" && toDir != "out") {
            return {py::none(),
                    "Invalid slot direction. It should be either 'inp' or "
                    "'out'"};
        }

        const auto fromComp =
            scene->getState()
                .getComponentByUuid<Bess::Canvas::SimulationSceneComponent>(
                    fromCompId);
        const auto toComp =
            scene->getState()
                .getComponentByUuid<Bess::Canvas::SimulationSceneComponent>(
                    toCompId);
        if (!fromComp || !toComp || fromPortIdx < 0 || toPortIdx < 0) {
            return {py::none(), "Component or port index is invalid"};
        }

        const auto parseDir = [](const std::string &dir) {
            return dir == "inp" ? Bess::SimEngine::PortDirection::input
                                : Bess::SimEngine::PortDirection::output;
        };

        const auto validDir = [](Bess::SimEngine::PortDirection direction) {
            return direction == Bess::SimEngine::PortDirection::input ||
                   direction == Bess::SimEngine::PortDirection::output;
        };

        const auto fromDirection = parseDir(fromDir);
        const auto toDirection = parseDir(toDir);

        if (!validDir(fromDirection) || !validDir(toDirection)) {
            return {py::none(), "Port direction is invalid"};
        }

        const auto slotAt = [](const auto *comp,
                               Bess::SimEngine::PortDirection direction,
                               int index) {
            const auto &slots =
                direction == Bess::SimEngine::PortDirection::input
                    ? comp->getInputSlots()
                    : comp->getOutputSlots();
            return static_cast<std::size_t>(index) < slots.size()
                       ? slots[static_cast<std::size_t>(index)]
                       : Bess::UUID::null;
        };
        const auto fromSlot = slotAt(fromComp, fromDirection, fromPortIdx);
        const auto toSlot = slotAt(toComp, toDirection, toPortIdx);
        if (fromSlot == Bess::UUID::null || toSlot == Bess::UUID::null) {
            return {py::none(), "Port index is out of range"};
        }

        auto conn = std::make_shared<Bess::Canvas::ConnectionSceneComponent>();
        conn->setStartEndSlots(fromSlot, toSlot);
        const auto result = Bess::UI::Proj::addConn(conn, scene->getSceneId());
        return result ? CmdResult{py::cast(conn->getUuid()), ""}
                      : CmdResult{py::none(), result.msg};
    };

    addCmd("connect",
           connectSlotsFn,
           "Connects two slots together. "
           "Use port directions and indices to specify the ports to connect.",
           py::arg("from_comp_id"),
           py::arg("from_direction"),
           py::arg("from_port_idx"),
           py::arg("to_comp_id"),
           py::arg("to_direction"),
           py::arg("to_port_idx"));

    // organize components
    auto orgCompsFn = []() -> CmdResult {
        const auto result = Bess::UI::Proj::layout();
        if (!result.applied) {
            if (result.count == 0) {
                return {py::cast(
                            "Hierarchical layout skipped: no scene components"),
                        ""};
            } else {
                return {py::cast("Hierarchical layout skipped"), ""};
            }
        }
        return {
            py::cast(std::format("Applied hierarchical layout to {} components",
                                 result.count)),
            ""};
    };

    addCmd("org_comps",
           orgCompsFn,
           "Organizes components in the scene using a specified method."
           "Currently only 'hierarchical' method is supported.");

    addCmd("set_inp_comp_state",
           setInpStateFn,
           "Sets the state of input component slot.",
           py::arg("comp_id"),
           py::arg("slot_idx"),
           py::arg("state"));

    auto execCmdFn = [](const std::string &cmd) -> CmdResult {
        if (!cmd.starts_with("bessplug.cmds")) {
            BESS_ERROR("Invalid command '{}'", cmd);
            return {py::none(), "Invalid Command"};
        }

        py::gil_scoped_acquire lock{};

        try {
            py::exec("from bessplug.api.sim_engine import *");
            CmdResult result = py::eval(cmd).cast<CmdResult>();
            return {result.result, result.error};
        } catch (py::error_already_set &e) {
            BESS_ERROR(
                "Error executing Python command '{}': {}", cmd, e.what());
            return {py::none(), e.what()};
        }
    };

    addCmd(
        "clear",
        []() -> CmdResult {
            const auto status = Bess::UI::Proj::newProj();
            if (!status) {
                return {py::none(), status.msg};
            }
            return CmdResult{py::cast(true), ""};
        },
        "Clears the current circuit, removing all components and connections.");

    addCmd("exec",
           execCmdFn,
           "Executes a single bessplug command.",
           py::arg("cmd"));

    auto execScriptFn = [](const std::string &script) -> CmdResult {
        py::gil_scoped_acquire lock{};

        auto sys = py::module_::import("sys");
        py::object old_stdout = sys.attr("stdout");
        py::object old_stderr = sys.attr("stderr");

        try {
            scriptLogger->popLogs();

            sys.attr("stdout") = scriptLogger;
            sys.attr("stderr") = scriptLogger;

            py::exec(script);

            sys.attr("stdout") = old_stdout;
            sys.attr("stderr") = old_stderr;

            return {py::cast(true), ""};
        } catch (py::error_already_set &e) {
            BESS_ERROR("Error executing Python script: {}", e.what());
            scriptLogger->write(std::string(e.what()) + "\n");
            sys.attr("stdout") = old_stdout;
            sys.attr("stderr") = old_stderr;
            return {py::none(), e.what()};
        }
    };

    addCmd("exec_script",
           execScriptFn,
           "Executes a Python script containing multiple bessplug commands.",
           py::arg("script"));

    auto execAsyncScriptFn =
        [execScriptFn](const std::string &script) -> CmdResult {
        if (status->isRunning.load()) {
            BESS_WARN("Attempted to execute a new script while another script "
                      "is still running.");
            return {py::none(), "Another script is already running"};
        }

        status->isRunning.store(true);
        status->error = "";

        std::thread([execScriptFn, script]() {
            auto &driver = Bess::UI::Proj::scenes();
            driver.setIsPaused(true);
            py::gil_scoped_acquire lock{};
            status->error = execScriptFn(script).error;
            status->log = scriptLogger->popLogs();
            status->isRunning.store(false);
            driver.setIsPaused(false);
        }).detach();

        return {py::cast(true), ""};
    };

    addCmd("exec_script_async",
           execAsyncScriptFn,
           "Executes a Python script asynchronously. Returns immediately with "
           "success status."
           "Use get_status() to check if the script is still running.",
           py::arg("script"));

    auto getAsyncScriptStatusFn = []() -> std::shared_ptr<AsyncScriptStatus> {
        return status;
    };

    addCmd("get_async_script_status",
           getAsyncScriptStatusFn,
           "Gets the status of the currently running asynchronous script.");

    /// KEEP THIS PRINT CMD AT THE END
    m.def(
        "print_cmds",
        []() {
            py::print("Available commands:");

            for (auto &cmd : cmdDefs) {
                if (cmd._argsCache.empty() && !cmd.args.empty()) {
                    std::string argsStr;
                    for (size_t i = 0; i < cmd.args.size(); ++i) {
                        argsStr += cmd.args[i].name;
                        if (i < cmd.args.size() - 1) {
                            argsStr += ", ";
                        }
                    }
                    argsStr += ")";
                    cmd._argsCache = argsStr;
                }

                py::print("\t" + cmd.name + "(" + cmd._argsCache + "):\n\t\t" +
                          cmd.desc);
            }
        },
        "Prints all the cmds.");
}

void bind_cmd_results(py::module &m) {
    py::class_<CmdResult>(m, "CmdResult")
        .def_property_readonly(
            "result", [](const CmdResult &self) { return self.result; })
        .def_property_readonly("error",
                               [](const CmdResult &self) { return self.error; })
        .def("reset",
             [](CmdResult &self) {
                 self.result = py::none();
                 self.error = "";
             })
        .def("__repr__", [](const CmdResult &self) {
            if (self.error.empty()) {
                return "<CmdResult: success - " +
                       py::repr(self.result).cast<std::string>() + ">";
            } else {
                return "<CmdResult: error - " + self.error + ">";
            }
        });
}

void bind_async_script_status(py::module &m) {
    py::class_<AsyncScriptStatus, std::shared_ptr<AsyncScriptStatus>>(
        m, "AsyncScriptStatus")
        .def_property_readonly(
            "is_running",
            [](const AsyncScriptStatus &self) { return self.isRunning.load(); })
        .def_property_readonly(
            "log",
            [](const AsyncScriptStatus &self) -> std::string {
                return self.log;
            })
        .def_property_readonly(
            "error", [](const AsyncScriptStatus &self) { return self.log; })
        .def("__repr__", [](const AsyncScriptStatus &self) -> std::string {
            if (self.isRunning.load()) {
                return "<AsyncScriptStatus: running>";
            } else if (!self.error.empty()) {
                return "<AsyncScriptStatus: error - " + self.error + ">";
            } else {
                return "<AsyncScriptStatus: completed successfully - " +
                       self.log + ">";
            }
        });
}

void bind_script_logger(py::module &m) {
    py::class_<ScriptLogger, std::shared_ptr<ScriptLogger>>(m, "ScriptLogger")
        .def("write", &ScriptLogger::write)
        .def("flush", &ScriptLogger::flush)
        .def("pop_logs",
             &ScriptLogger::popLogs,
             "Retrieves and clears the current logs captured from script "
             "execution.")
        .def("get_logs",
             &ScriptLogger::getLogs,
             "Retrieves the current logs captured from script execution "
             "without clearing them.");
}

std::shared_ptr<Bess::SimEngine::Drivers::Digital::DigSimComp>
findUniqueDigCompByName(const std::string &compName) {
    auto &simEngine = Bess::UI::Proj::sim();

    std::shared_ptr<Bess::SimEngine::Drivers::Digital::DigSimComp> found;
    for (const auto &driver : simEngine.getDrivers()) {
        for (const auto &[uuid, comp] : driver->getComponentsMap()) {
            if (comp && comp->getName() == compName) {
                if (found) {
                    BESS_ERROR("Multiple components found with name '{}'. Make "
                               "sure component names are unique.",
                               compName);
                    return nullptr;
                }

                auto digComp = std::dynamic_pointer_cast<
                    Bess::SimEngine::Drivers::Digital::DigSimComp>(comp);
                BESS_ASSERT(
                    digComp,
                    "Component with name '{}' is not a digital component",
                    compName);
                auto snapshot = std::make_shared<
                    Bess::SimEngine::Drivers::Digital::DigSimComp>(
                    *digComp.get());
            }
        }
    }

    if (!found) {
        BESS_ERROR("No component found with name '{}'", compName);
    }

    return found;
}

std::shared_ptr<Bess::SimEngine::Drivers::Digital::DigSimComp>
findDigCompBySceneId(uint64_t compId) {
    auto &sceneDriver = Bess::UI::Proj::scenes();
    const auto &simComp =
        sceneDriver.getActiveScene()
            ->getState()
            .getComponentByUuid<Bess::Canvas::SimulationSceneComponent>(compId);

    if (!simComp) {
        return nullptr;
    }

    const auto &simEngineId = simComp->getSimEngineId();

    auto &simEngine = Bess::UI::Proj::sim();
    const auto &comp =
        simEngine.getComponent<Bess::SimEngine::Drivers::Digital::DigSimComp>(
            simEngineId);

    if (!comp) {
        return nullptr;
    }

    auto snapshot =
        std::make_shared<Bess::SimEngine::Drivers::Digital::DigSimComp>(*comp);
    return snapshot;
}
