#include "net/net.h"
#include "simulation_engine.h"

#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

namespace py = pybind11;

const std::unordered_map<Bess::UUID, Bess::SimEngine::Net> &
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

void bind_sim_engine_core(py::module_ &m) {
    auto coreMod = m.def_submodule("core", "Core simulation engine bindings");

    coreMod.def("get_nets", &getNetsMap, py::arg("update") = true,
                "Get all nets in the simulation engine");

    coreMod.def("pause", &pauseSimEngine, "Pause the simulation engine");

    coreMod.def("resume", &resumeSimEngine, "Resume the simulation engine");
}
