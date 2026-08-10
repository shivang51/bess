#include "common/bess_uuid.h"
#include "common/types.h"
#include "net/net.h"
#include "sim_driver/sim_driver.h"
#include "simulation_engine.h"
#include "ui/project_api.h"

#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

namespace py = pybind11;

std::unordered_map<Bess::UUID, Bess::SimEngine::Net>
getNetsMap(bool update = true) {
    return Bess::UI::Proj::sim().getNetsMap(update);
}

void pauseSimEngine() {
    Bess::UI::Proj::sim().setSimulationState(
        Bess::SimEngine::SimulationState::paused);
}

void resumeSimEngine() {
    Bess::UI::Proj::sim().setSimulationState(
        Bess::SimEngine::SimulationState::running);
}

std::shared_ptr<Bess::SimEngine::Drivers::SimComponent>
getComp(const Bess::UUID &id) {
    return Bess::UI::Proj::sim()
        .getComponentSP<Bess::SimEngine::Drivers::SimComponent>(id);
}

bool isSimStable() {
    return Bess::UI::Proj::sim().isSimStable();
}

void setOutPortState(const Bess::UUID &compId,
                     const int portIdx,
                     const Bess::SimEngine::PortState &state) {
    auto &simEngine = Bess::UI::Proj::sim();
    simEngine.setOutputPortState(compId, portIdx, state);
}

void setOutPortLogicState(const Bess::UUID &compId,
                          const int portIdx,
                          const Bess::SimEngine::LogicState &state) {
    setOutPortState(
        compId, portIdx, Bess::SimEngine::PortState::digital(state));
}

std::vector<Bess::SimEngine::PortState>
getInputPortStates(const Bess::UUID &compId) {
    auto &simEngine = Bess::UI::Proj::sim();
    return simEngine.getInputPortStates(compId);
}

void bind_sim_engine_core(py::module_ &m) {
    auto coreMod = m.def_submodule("core", "Core simulation engine bindings");

    // A simulation callback can need the GIL while the scheduler owns its
    // execution barrier. Python callers must therefore release the GIL before
    // entering any engine operation that can wait on that barrier.
    coreMod.def("get_nets",
                &getNetsMap,
                py::arg("update") = true,
                py::call_guard<py::gil_scoped_release>(),
                "Get all nets in the simulation engine");

    coreMod.def("pause",
                &pauseSimEngine,
                py::call_guard<py::gil_scoped_release>(),
                "Pause the simulation engine");

    coreMod.def("resume",
                &resumeSimEngine,
                py::call_guard<py::gil_scoped_release>(),
                "Resume the simulation engine");

    coreMod.def("get_comp",
                &getComp,
                py::arg("uuid"),
                py::call_guard<py::gil_scoped_release>(),
                "Get sim comp with id");

    coreMod.def("is_sim_stable",
                &isSimStable,
                py::call_guard<py::gil_scoped_release>(),
                "Check if the simulation engine is in a stable state");

    coreMod.def("set_output_port_state",
                &setOutPortState,
                py::arg("comp_id"),
                py::arg("port_idx"),
                py::arg("state"),
                py::call_guard<py::gil_scoped_release>(),
                "Set the state of an output port for a component");
    coreMod.def(
        "set_output_port_state",
        &setOutPortLogicState,
        py::arg("comp_id"),
        py::arg("port_idx"),
        py::arg("state"),
        py::call_guard<py::gil_scoped_release>(),
        "Set the digital logic state of an output port for a component");

    coreMod.def("get_input_port_states",
                &getInputPortStates,
                py::arg("comp_id"),
                py::call_guard<py::gil_scoped_release>(),
                "Get the states of all input ports for a component");
}
