#include "common/logger.h"
#include "digital_component.h"
#include "simulation_engine.h"
#include "types.h"
#include <pybind11/eval.h>
#include <pybind11/functional.h>
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

namespace py = pybind11;

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

struct CmdResult {
    py::object result;
    std::string error;
};

void bind_cmd_results(py::module &m);

void bind_cmds(py::module &m) {
    bind_cmd_results(m);

    auto getInpStates = [](const std::string &compName) -> CmdResult {
        auto &simEngine = Bess::SimEngine::SimulationEngine::instance();
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

    m.def("get_inp_states",
          getInpStates,
          "Gets the states of any components input slots. \
					For it work make sure component name is unique in your circuit",
          py::arg("comp_name"));

    auto getOutStates = [](const std::string &compName) -> CmdResult {
        auto &simEngine = Bess::SimEngine::SimulationEngine::instance();
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

    m.def("get_out_states",
          getOutStates,
          "Gets the states of any components output slots. \
					For it work make sure component name is unique in your circuit",
          py::arg("comp_name"));

    auto setInpStateFn = [](const std::string &compName,
                            int slotIdx,
                            const Bess::SimEngine::LogicState &state) -> CmdResult {
        auto &simEngine = Bess::SimEngine::SimulationEngine::instance();
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

        simEngine.setOutputSlotState(comp->id, slotIdx, state);
        return {py::cast(true), ""};
    };

    m.def("set_inp_state",
          setInpStateFn,
          "Sets the state of input coponent slot. \
					For it work make sure input names are unique in your circuit",
          py::arg("comp_name"),
          py::arg("slot_idx"),
          py::arg("state"));

    auto execCmdFn = [](const std::string &cmd) -> CmdResult {
        if (!cmd.starts_with("bessplug.cmds")) {
            BESS_ERROR("Invalid command '{}'", cmd);
            return {py::none(), "Invalid Command"};
        }

        py::gil_scoped_acquire lock{};

        try {
            CmdResult result = py::eval(cmd).cast<CmdResult>();
            return {result.result, result.error};
        } catch (py::error_already_set &e) {
            BESS_ERROR("Error executing Python command '{}': {}", cmd, e.what());
            return {py::none(), e.what()};
        }
    };

    m.def("exec",
          execCmdFn,
          "Executes a Python command.",
          py::arg("cmd"));
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
