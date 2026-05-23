#include "common/bess_uuid.h"
#include "common/types.h"
#include "net/net.h"
#include "sim_driver/sim_driver.h"
#include "simulation_engine.h"

#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

namespace py = pybind11;

std::unordered_map<Bess::UUID, Bess::SimEngine::Net>
getNetsMap(bool update = true) {
    return Bess::SimEngine::SimulationEngine::instance().getNetsMap(update);
}

void pauseSimEngine() {
    Bess::SimEngine::SimulationEngine::instance().setSimulationState(
        Bess::SimEngine::SimulationState::paused);
}

void resumeSimEngine() {
    Bess::SimEngine::SimulationEngine::instance().setSimulationState(
        Bess::SimEngine::SimulationState::running);
}

std::shared_ptr<Bess::SimEngine::Drivers::SimComponent>
getComp(const Bess::UUID &id) {
    return Bess::SimEngine::SimulationEngine::instance()
        .getComponent<Bess::SimEngine::Drivers::SimComponent>(id);
}

bool isSimStable() {
    return Bess::SimEngine::SimulationEngine::instance().isSimStable();
}

void setOutSlotState(const Bess::UUID &compId, const int slotIdx,
                     const Bess::SimEngine::LogicState &state) {
    auto &simEngine = Bess::SimEngine::SimulationEngine::instance();
    simEngine.setOutputSlotState(compId, slotIdx, state);
}

std::vector<Bess::SimEngine::SlotState> getInpStates(const Bess::UUID &compId) {
    auto &simEngine = Bess::SimEngine::SimulationEngine::instance();
    return simEngine.getInputSlotsState(compId);
}

void bind_sim_engine_core(py::module_ &m) {
    auto coreMod = m.def_submodule("core", "Core simulation engine bindings");

    coreMod.def("get_nets", &getNetsMap, py::arg("update") = true,
                "Get all nets in the simulation engine");

    coreMod.def("pause", &pauseSimEngine, "Pause the simulation engine");

    coreMod.def("resume", &resumeSimEngine, "Resume the simulation engine");

    coreMod.def("get_comp", &getComp, py::arg("uuid"), "Get sim comp with id");

    coreMod.def("is_sim_stable", &isSimStable,
                "Check if the simulation engine is in a stable state");

    coreMod.def("set_out_slot_state", &setOutSlotState, py::arg("comp_id"),
                py::arg("slot_idx"), py::arg("state"),
                "Set the state of an output slot for a component");

    coreMod.def("get_inp_slots_states", &getInpStates, py::arg("comp_id"),
                "Get the states of all input slots for a component");
}
