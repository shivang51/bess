#include "common/bess_uuid.h"
#include "common/logger.h"
#include "component_catalog.h"
#include "digital_component.h"
#include "pages/main_page/main_page.h"
#include "pages/main_page/scene_components/connection_scene_component.h" // IWYU pragma: keep
#include "pages/main_page/scene_components/sim_scene_component.h"
#include "pages/main_page/scene_components/slot_scene_component.h"
#include "pages/main_page/services/connection_service.h"
#include "simulation_engine.h"
#include "types.h"
#include "ui_main/component_explorer.h"
#include <pybind11/eval.h>
#include <pybind11/functional.h>
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

namespace py = pybind11;

struct CmdResult {
    py::object result = py::none();
    std::string error;
};

struct AsyncScriptStatus {
    std::atomic<bool> isRunning{false};
    CmdResult res = {};
};

class ScriptLogger {
  public:
    void write(const std::string &text) {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_output += text;
    }

    void flush() {}

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

std::shared_ptr<Bess::SimEngine::DigitalComponent> findUniqueDigCompByName(
    const std::string &compName);
std::shared_ptr<Bess::SimEngine::DigitalComponent> findDigCompBySceneId(uint64_t compId);
void bind_cmd_results(py::module &m);
void bind_async_script_status(py::module &m);
void bind_script_logger(py::module &m);
void bind_cmds(py::module &m) {
    bind_cmd_results(m);
    bind_async_script_status(m);
    bind_script_logger(m);

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

    m.def("add_comp",
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
        states.reserve(comp->state.inputStates.size());
        for (const auto &state : comp->state.inputStates) {
            states.push_back(state.state);
        }
        return {py::cast(states), ""};
    };

    m.def("get_inp_states_n",
          getInpStatesN,
          "Gets the states of any components input slots. \
					For it work make sure component name is unique in your circuit",
          py::arg("comp_name"));

    auto getInpStates = [](uint64_t compId) -> CmdResult {
        const auto &comp = findDigCompBySceneId(compId);
        if (!comp) {
            return {py::none(), "Component not found"};
        }

        std::vector<Bess::SimEngine::LogicState> states;
        states.reserve(comp->state.inputStates.size());
        for (const auto &state : comp->state.inputStates) {
            states.push_back(state.state);
        }
        return {py::cast(states), ""};
    };

    m.def("get_inp_states",
          getInpStates,
          "Gets the states of any components input slots.",
          py::arg("comp_id"));

    auto getOutStatesN = [](const std::string &compName) -> CmdResult {
        const auto &comp = findUniqueDigCompByName(compName);
        if (!comp) {
            return {py::none(), "Component not found"};
        }

        std::vector<Bess::SimEngine::LogicState> states;
        states.reserve(comp->state.outputStates.size());
        for (const auto &state : comp->state.outputStates) {
            states.push_back(state.state);
        }
        return {py::cast(states), ""};
    };

    m.def("get_out_states_n",
          getOutStatesN,
          "Gets the states of any components output slots. \
					For it work make sure component name is unique in your circuit",
          py::arg("comp_name"));

    auto getOutStates = [](uint64_t compId) -> CmdResult {
        const auto &comp = findDigCompBySceneId(compId);

        if (!comp) {
            return {py::none(), "Component not found"};
        }

        std::vector<Bess::SimEngine::LogicState> states;
        states.reserve(comp->state.outputStates.size());
        for (const auto &state : comp->state.outputStates) {
            states.push_back(state.state);
        }
        return {py::cast(states), ""};
    };

    m.def("get_out_states",
          getOutStates,
          "Gets the states of any components output slots.",
          py::arg("comp_id"));

    auto setInpStateNFn = [](const std::string &compName,
                             int slotIdx,
                             const Bess::SimEngine::LogicState &state) -> CmdResult {
        const auto &comp = findUniqueDigCompByName(compName);

        if (!comp) {
            return {py::none(), "Component not found"};
        }

        if (comp->definition->getBehaviorType() !=
            Bess::SimEngine::ComponentBehaviorType::input) {

            BESS_ERROR("Component '{}' is not an input component.\
												Only input components can have their input state set.",
                       compName);
            return {py::none(), "Component is not an input component"};
        }

        auto &simEngine = Bess::SimEngine::SimulationEngine::instance();
        simEngine.setOutputSlotState(comp->id, slotIdx, state);
        return {py::cast(true), ""};
    };

    // id is of scene component
    m.def("set_inp_state_n",
          setInpStateNFn,
          "Sets the state of input coponent slot. \
					For it work make sure input names are unique in your circuit",
          py::arg("comp_name"),
          py::arg("slot_idx"),
          py::arg("state"));

    auto setInpStateFn = [](uint64_t compId,
                            int slotIdx,
                            const Bess::SimEngine::LogicState &state) -> CmdResult {
        const auto &comp = findDigCompBySceneId(compId);
        if (!comp) {
            return {py::none(), "Component not found"};
        }

        if (comp->definition->getBehaviorType() !=
            Bess::SimEngine::ComponentBehaviorType::input) {

            BESS_ERROR("Component with id {} is not an input component.\
												Only input components can have their input state set.",
                       compId);
            return {py::none(), "Component is not an input component"};
        }

        auto &simEngine = Bess::SimEngine::SimulationEngine::instance();
        simEngine.setOutputSlotState(comp->id, slotIdx, state);
        return {py::cast(true), ""};
    };

    // connect slots

    auto connectSlotsFn = [](const Bess::UUID &fromCompId, Bess::Canvas::SlotType fromSlotType, int fromSlotIdx,
                             const Bess::UUID &toCompId, Bess::Canvas::SlotType toSlotType, int toSlotIdx) -> CmdResult {
        const auto &sceneDriver = Bess::Pages::MainPage::getInstance()->getState().getSceneDriver();

        auto &connSvc = Bess::Svc::SvcConnection::instance();
        auto conn = connSvc.createConnection(fromCompId, fromSlotType, fromSlotIdx,
                                             toCompId, toSlotType, toSlotIdx, sceneDriver.getActiveScene().get());

        if (conn) {
            return {py::cast(conn->getUuid()), ""};
        } else {
            return {py::none(), "Failed to connect slots. Check if component and slot indices are correct."};
        }
    };

    m.def("connect",
          connectSlotsFn,
          "Connects two slots together.\
					Use slot types and indices to specify the slots to connect.\
					Slot types can be 'input' or 'output'.",
          py::arg("from_comp_id"),
          py::arg("from_slot_type"),
          py::arg("from_slot_idx"),
          py::arg("to_comp_id"),
          py::arg("to_slot_type"),
          py::arg("to_slot_idx"));

    // organize components
    auto orgCompsFn = []() -> CmdResult {
        auto &pageState = Bess::Pages::MainPage::getInstance()->getState();
        const auto result = pageState.applyHierarchicalLayoutToActiveScene();
        if (!result.applied) {
            if (result.laidOutNodes == 0) {
                return {py::cast("Hierarchical layout skipped: no scene components"), ""};
            } else {
                return {py::cast("Hierarchical layout skipped"), ""};
            }
        }
        return {py::cast(std::format("Applied hierarchical layout to {} components", result.laidOutNodes)), ""};
    };

    m.def("org_comps",
          orgCompsFn,
          "Organizes components in the scene using a specified method.\
					Currently only 'hierarchical' method is supported.");

    m.def("set_inp_state",
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
            BESS_ERROR("Error executing Python command '{}': {}", cmd, e.what());
            return {py::none(), e.what()};
        }
    };

    m.def("exec",
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

    m.def("exec_script", execScriptFn,
          "Executes a Python script containing multiple bessplug commands.",
          py::arg("script"));

    auto execAsyncScriptFn = [execScriptFn](const std::string &script) -> CmdResult {
        if (status->isRunning.load()) {
            BESS_WARN("Attempted to execute a new script while another script is still running.");
            return {py::none(), "Another script is already running"};
        }

        status->isRunning.store(true);
        status->res.error = "";

        std::thread([execScriptFn, script]() {
            py::gil_scoped_acquire lock{};
            status->res = execScriptFn(script);
            status->res.result = py::cast(scriptLogger->popLogs());
            status->isRunning.store(false);
        }).detach();

        return {py::cast(true), ""};
    };

    m.def("exec_script_async", execAsyncScriptFn,
          "Executes a Python script asynchronously. Returns immediately with success status.\
					Use get_status() to check if the script is still running.",
          py::arg("script"));

    auto getAsyncScriptStatusFn = []() -> std::shared_ptr<AsyncScriptStatus> {
        return status;
    };

    m.def("get_async_script_status",
          getAsyncScriptStatusFn,
          "Gets the status of the currently running asynchronous script.");
}

void bind_cmd_results(py::module &m) {
    py::class_<CmdResult>(m, "CmdResult")
        .def_property_readonly("result", [](const CmdResult &self) { return self.result; })
        .def_property_readonly("error", [](const CmdResult &self) { return self.error; })
        .def("__repr__", [](const CmdResult &self) {
            if (self.error.empty()) {
                return "<CmdResult: success - " + py::repr(self.result).cast<std::string>() + ">";
            } else {
                return "<CmdResult: error - " + self.error + ">";
            }
        });
}

void bind_async_script_status(py::module &m) {
    py::class_<AsyncScriptStatus, std::shared_ptr<AsyncScriptStatus>>(m, "AsyncScriptStatus")
        .def_property_readonly("is_running", [](const AsyncScriptStatus &self) { return self.isRunning.load(); })
        .def_property_readonly("result", [](const AsyncScriptStatus &self) -> py::object {
            py::gil_scoped_acquire lock;
            return self.res.result;
        })
        .def_property_readonly("error", [](const AsyncScriptStatus &self) { return self.res.error; })
        .def("__repr__", [](const AsyncScriptStatus &self) -> std::string {
            if (self.isRunning.load()) {
                return "<AsyncScriptStatus: running>";
            } else if (!self.res.error.empty()) {
                return "<AsyncScriptStatus: error - " + self.res.error + ">";
            } else {
                return "<AsyncScriptStatus: completed successfully - " +
                       py::repr(self.res.result).cast<std::string>() + ">";
            }
        });
}

void bind_script_logger(py::module &m) {
    py::class_<ScriptLogger, std::shared_ptr<ScriptLogger>>(m, "ScriptLogger")
        .def("write",
             &ScriptLogger::write)
        .def("flush",
             &ScriptLogger::flush)
        .def("pop_logs",
             &ScriptLogger::popLogs,
             "Retrieves and clears the current logs captured from script execution.")
        .def("get_logs",
             &ScriptLogger::getLogs,
             "Retrieves the current logs captured from script execution without clearing them.");
}

std::shared_ptr<Bess::SimEngine::DigitalComponent> findUniqueDigCompByName(const std::string &compName) {
    auto &simEngine = Bess::SimEngine::SimulationEngine::instance();
    const auto &simEngineState = simEngine.getSimEngineState();
    auto comps = simEngineState.findCompsByName(compName);
    if (comps.empty()) {
        BESS_ERROR("No component found with name '{}'", compName);
        return nullptr;
    } else if (comps.size() > 1) {
        BESS_ERROR("Multiple components found with name '{}'.\
												Make sure component names are unique.",
                   compName);
        return nullptr;
    }
    return comps.front();
}

std::shared_ptr<Bess::SimEngine::DigitalComponent> findDigCompBySceneId(uint64_t compId) {
    const auto &sceneDriver = Bess::Pages::MainPage::getInstance()->getState().getSceneDriver();
    const auto &simComp = sceneDriver->getState()
                              .getComponentByUuid<Bess::Canvas::SimulationSceneComponent>(compId);

    if (!simComp) {
        return nullptr;
    }

    const auto &simEngineId = simComp->getSimEngineId();

    auto &simEngine = Bess::SimEngine::SimulationEngine::instance();
    const auto &comp = simEngine.getDigitalComponent(simEngineId);

    return comp;
}
