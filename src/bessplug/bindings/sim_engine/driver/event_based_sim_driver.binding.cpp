#include "drivers/event_based_sim_driver.h"
#include "drivers/sim_driver.h"

#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

namespace py = pybind11;

class PyEvtBasedSimDriver : public Bess::SimEngine::Drivers::EvtBasedSimDriver {
  public:
    using Bess::SimEngine::Drivers::EvtBasedSimDriver::EvtBasedSimDriver;
    using CanConnectResult = std::pair<bool, std::string>;

    std::string getName() const override {
        PYBIND11_OVERRIDE_PURE_NAME(std::string,
                                    Bess::SimEngine::Drivers::EvtBasedSimDriver,
                                    "get_name",
                                    getName);
    }

    bool suuportsDef(const std::shared_ptr<Bess::SimEngine::Drivers::CompDef> &def) const override {
        PYBIND11_OVERRIDE_PURE_NAME(bool,
                                    Bess::SimEngine::Drivers::EvtBasedSimDriver,
                                    "supports_def",
                                    suuportsDef,
                                    def);
    }

    std::shared_ptr<Bess::SimEngine::Drivers::SimComponent> createComp(
        const std::shared_ptr<Bess::SimEngine::Drivers::CompDef> &def) override {
        PYBIND11_OVERRIDE_PURE_NAME(std::shared_ptr<Bess::SimEngine::Drivers::SimComponent>,
                                    Bess::SimEngine::Drivers::EvtBasedSimDriver,
                                    "create_component",
                                    createComp,
                                    def);
    }

    CanConnectResult canConnectComponents(const Bess::UUID &src,
                                          int srcSlotIdx,
                                          Bess::SimEngine::SlotType srcType,
                                          const Bess::UUID &dst,
                                          int dstSlotIdx,
                                          Bess::SimEngine::SlotType dstType) const override {
        PYBIND11_OVERRIDE_PURE_NAME(CanConnectResult,
                                    Bess::SimEngine::Drivers::EvtBasedSimDriver,
                                    "can_connect_components",
                                    canConnectComponents,
                                    src,
                                    srcSlotIdx,
                                    srcType,
                                    dst,
                                    dstSlotIdx,
                                    dstType);
    }

    bool connectComponent(const Bess::UUID &src,
                          int srcSlotIdx,
                          Bess::SimEngine::SlotType srcType,
                          const Bess::UUID &dst,
                          int dstSlotIdx,
                          Bess::SimEngine::SlotType dstType,
                          bool overrideConn) override {
        PYBIND11_OVERRIDE_PURE_NAME(bool,
                                    Bess::SimEngine::Drivers::EvtBasedSimDriver,
                                    "connect_component",
                                    connectComponent,
                                    src,
                                    srcSlotIdx,
                                    srcType,
                                    dst,
                                    dstSlotIdx,
                                    dstType,
                                    overrideConn);
    }

    void deleteConnection(const Bess::UUID &compA,
                          Bess::SimEngine::SlotType pinAType,
                          int idxA,
                          const Bess::UUID &compB,
                          Bess::SimEngine::SlotType pinBType,
                          int idxB) override {
        PYBIND11_OVERRIDE_PURE_NAME(void,
                                    Bess::SimEngine::Drivers::EvtBasedSimDriver,
                                    "delete_connection",
                                    deleteConnection,
                                    compA,
                                    pinAType,
                                    idxA,
                                    compB,
                                    pinBType,
                                    idxB);
    }

    bool addSlot(const Bess::UUID &compId,
                 Bess::SimEngine::SlotType type,
                 int index) override {
        PYBIND11_OVERRIDE_PURE_NAME(bool,
                                    Bess::SimEngine::Drivers::EvtBasedSimDriver,
                                    "add_slot",
                                    addSlot,
                                    compId,
                                    type,
                                    index);
    }

    bool removeSlot(const Bess::UUID &compId,
                    Bess::SimEngine::SlotType type,
                    int index) override {
        PYBIND11_OVERRIDE_PURE_NAME(bool,
                                    Bess::SimEngine::Drivers::EvtBasedSimDriver,
                                    "remove_slot",
                                    removeSlot,
                                    compId,
                                    type,
                                    index);
    }

    bool simulate(const Bess::SimEngine::Drivers::SimEvt &evt) override {
        PYBIND11_OVERRIDE_PURE_NAME(bool,
                                    Bess::SimEngine::Drivers::EvtBasedSimDriver,
                                    "simulate",
                                    simulate,
                                    evt);
    }

    void addComponent(const std::shared_ptr<Bess::SimEngine::Drivers::EvtBasedSimComp> &comp,
                      bool scheduleSim) override {
        PYBIND11_OVERRIDE_NAME(void,
                               Bess::SimEngine::Drivers::EvtBasedSimDriver,
                               "add_component",
                               addComponent,
                               comp,
                               scheduleSim);
    }

    std::vector<Bess::UUID> getDependants(const Bess::UUID &id) override {
        PYBIND11_OVERRIDE_NAME(std::vector<Bess::UUID>,
                               Bess::SimEngine::Drivers::EvtBasedSimDriver,
                               "get_dependants",
                               getDependants,
                               id);
    }

    std::vector<Bess::SimEngine::SlotState> collapseInputs(const Bess::UUID &id) override {
        PYBIND11_OVERRIDE_NAME(std::vector<Bess::SimEngine::SlotState>,
                               Bess::SimEngine::Drivers::EvtBasedSimDriver,
                               "collapse_inputs",
                               collapseInputs,
                               id);
    }

    void onBeforeRun() override {
        PYBIND11_OVERRIDE_NAME(void,
                               Bess::SimEngine::Drivers::EvtBasedSimDriver,
                               "on_before_run",
                               onBeforeRun);
    }
};

void bind_event_based_sim_driver(py::module_ &m) {
    using namespace Bess::SimEngine::Drivers;

    py::class_<EvtBasedSimDriver, SimDriver, PyEvtBasedSimDriver, std::shared_ptr<EvtBasedSimDriver>>(m, "EvtBasedSimDriver")
        .def(py::init<>())
        .def("run", &EvtBasedSimDriver::run)
        .def("simulate", &EvtBasedSimDriver::simulate, py::arg("event"))
        .def("add_component",
             &EvtBasedSimDriver::addComponent,
             py::arg("component"),
             py::arg("schedule_sim") = true)
        .def("get_dependants", &EvtBasedSimDriver::getDependants, py::arg("component_id"))
        .def("collapse_inputs", &EvtBasedSimDriver::collapseInputs, py::arg("component_id"))
        .def("on_before_run", &EvtBasedSimDriver::onBeforeRun)
        .def("schedule_event",
             &EvtBasedSimDriver::scheduleEvt,
             py::arg("component_id"),
             py::arg("sim_time"),
             py::arg("scheduler_id"),
             py::arg("notify") = true)
        .def("clear_pending_events", &EvtBasedSimDriver::clearPendingEvents);
}
