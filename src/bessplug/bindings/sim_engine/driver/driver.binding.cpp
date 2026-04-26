#include "common/types.h"
#include "drivers/digital_sim_driver.h"
#include "drivers/event_based_sim_driver.h"
#include "drivers/sim_driver.h"
#include "simulation_engine.h"

#include <pybind11/functional.h>
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

namespace py = pybind11;

class PySimDriver : public Bess::SimEngine::Drivers::SimDriver {
  public:
    using Bess::SimEngine::Drivers::SimDriver::SimDriver;
    using CanConnectResult = std::pair<bool, std::string>;

    void run() override {
        PYBIND11_OVERRIDE_PURE_NAME(void,
                                    Bess::SimEngine::Drivers::SimDriver,
                                    "run",
                                    run);
    }

    std::string getName() const override {
        PYBIND11_OVERRIDE_PURE_NAME(std::string,
                                    Bess::SimEngine::Drivers::SimDriver,
                                    "get_name",
                                    getName);
    }

    bool suuportsDef(const std::shared_ptr<Bess::SimEngine::Drivers::CompDef> &def) const override {
        PYBIND11_OVERRIDE_PURE_NAME(bool,
                                    Bess::SimEngine::Drivers::SimDriver,
                                    "supports_def",
                                    suuportsDef,
                                    def);
    }

    std::shared_ptr<Bess::SimEngine::Drivers::SimComponent> createComp(
        const std::shared_ptr<Bess::SimEngine::Drivers::CompDef> &def) override {
        PYBIND11_OVERRIDE_PURE_NAME(std::shared_ptr<Bess::SimEngine::Drivers::SimComponent>,
                                    Bess::SimEngine::Drivers::SimDriver,
                                    "create_component",
                                    createComp,
                                    def);
    }

    CanConnectResult canConnectComponents(
        Bess::SimEngine::SimulationEngine &engine,
        const Bess::UUID &src,
        int srcSlotIdx,
        Bess::SimEngine::SlotType srcType,
        const Bess::UUID &dst,
        int dstSlotIdx,
        Bess::SimEngine::SlotType dstType) const override {
        PYBIND11_OVERRIDE_PURE_NAME(CanConnectResult,
                                    Bess::SimEngine::Drivers::SimDriver,
                                    "can_connect_components",
                                    canConnectComponents,
                                    src,
                                    srcSlotIdx,
                                    srcType,
                                    dst,
                                    dstSlotIdx,
                                    dstType);
    }

    bool connectComponent(Bess::SimEngine::SimulationEngine &engine,
                          const Bess::UUID &src,
                          int srcSlotIdx,
                          Bess::SimEngine::SlotType srcType,
                          const Bess::UUID &dst,
                          int dstSlotIdx,
                          Bess::SimEngine::SlotType dstType,
                          bool overrideConn) override {
        PYBIND11_OVERRIDE_PURE_NAME(bool,
                                    Bess::SimEngine::Drivers::SimDriver,
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

    void deleteConnection(Bess::SimEngine::SimulationEngine &engine,
                          const Bess::UUID &compA,
                          Bess::SimEngine::SlotType pinAType,
                          int idxA,
                          const Bess::UUID &compB,
                          Bess::SimEngine::SlotType pinBType,
                          int idxB) override {
        PYBIND11_OVERRIDE_PURE_NAME(void,
                                    Bess::SimEngine::Drivers::SimDriver,
                                    "delete_connection",
                                    deleteConnection,
                                    compA,
                                    pinAType,
                                    idxA,
                                    compB,
                                    pinBType,
                                    idxB);
    }

    bool addSlot(Bess::SimEngine::SimulationEngine &engine,
                 const Bess::UUID &compId,
                 Bess::SimEngine::SlotType type,
                 int index) override {
        PYBIND11_OVERRIDE_PURE_NAME(bool,
                                    Bess::SimEngine::Drivers::SimDriver,
                                    "add_slot",
                                    addSlot,
                                    compId,
                                    type,
                                    index);
    }

    bool removeSlot(Bess::SimEngine::SimulationEngine &engine,
                    const Bess::UUID &compId,
                    Bess::SimEngine::SlotType type,
                    int index) override {
        PYBIND11_OVERRIDE_PURE_NAME(bool,
                                    Bess::SimEngine::Drivers::SimDriver,
                                    "remove_slot",
                                    removeSlot,
                                    compId,
                                    type,
                                    index);
    }

    void clearPendingEvents() override {
        PYBIND11_OVERRIDE_NAME(void,
                               Bess::SimEngine::Drivers::SimDriver,
                               "clear_pending_events",
                               clearPendingEvents);
    }
};

void bind_event_based_sim_driver(py::module_ &m);

void bind_sim_engine_driver(py::module_ &m) {
    using namespace Bess::SimEngine;
    using namespace Bess::SimEngine::Drivers;

    py::enum_<SimDriverState>(m, "SimDriverState")
        .value("UNINITIALIZED", SimDriverState::uninitialized)
        .value("DESTROYED", SimDriverState::destroyed)
        .value("STOPPED", SimDriverState::stopped)
        .value("RUNNING", SimDriverState::running)
        .value("PAUSED", SimDriverState::paused)
        .export_values();

    py::class_<SimFnDataBase, std::shared_ptr<SimFnDataBase>>(m, "SimFnData")
        .def(py::init<>())
        .def_readwrite("sim_dependants", &SimFnDataBase::simDependants);

    py::class_<CompDef, std::shared_ptr<CompDef>>(m, "CompDef")
        .def_property("name",
                      py::overload_cast<>(&CompDef::getName),
                      py::overload_cast<const std::string &>(&CompDef::setName))
        .def("type",
             &CompDef::getTypeName)
        .def("clone",
             &CompDef::clone)
        .def_property("group_name",
                      py::overload_cast<>(&CompDef::getGroupName),
                      py::overload_cast<const std::string &>(&CompDef::setGroupName));

    py::class_<SimComponent, std::shared_ptr<SimComponent>>(m, "SimComponent")
        .def(py::init<>())
        .def_property("uuid",
                      py::overload_cast<>(&SimComponent::getUuid),
                      py::overload_cast<const Bess::UUID &>(&SimComponent::setUuid))
        .def_property("name",
                      py::overload_cast<>(&SimComponent::getName),
                      py::overload_cast<const std::string &>(&SimComponent::setName))
        .def_property_readonly("definition", [](const SimComponent &self) {
            return self.getDefinition<CompDef>();
        })
        .def("to_json", &SimComponent::toJson)
        .def("simulate", &SimComponent::simulate, py::arg("data"));

    py::class_<EvtBasedCompDef, CompDef, std::shared_ptr<EvtBasedCompDef>>(m, "EvtBasedCompDef")
        .def_property("auto_reschedule",
                      py::overload_cast<>(&EvtBasedCompDef::getAutoReschedule),
                      py::overload_cast<const bool &>(&EvtBasedCompDef::setAutoReschedule))
        .def_property("prop_delay",                                                                    //
                      [](const EvtBasedCompDef &self) -> Bess::TimeNs { return self.getPropDelay(); }, //
                      [](EvtBasedCompDef &self, const Bess::TimeNs &value) { self.setPropDelay(value); })
        .def("get_self_sim_delay", &EvtBasedCompDef::getSelfSimDelay);

    py::class_<EvtBasedSimComp, SimComponent, std::shared_ptr<EvtBasedSimComp>>(m, "EvtBasedSimComp")
        .def(py::init<>())
        .def("get_prop_delay", &EvtBasedSimComp::getPropDelay)
        .def("get_sim_self", &EvtBasedSimComp::getSimSelf)
        .def("get_self_sim_delay", &EvtBasedSimComp::getSelfSimDelay);

    py::class_<SimEvt>(m, "SimEvt")
        .def(py::init<>())
        .def_readwrite("evt_id", &SimEvt::evtId)
        .def_readwrite("comp_id", &SimEvt::compId)
        .def_readwrite("scheduler_id", &SimEvt::schedulerId)
        .def_readwrite("sim_time", &SimEvt::simTime);

    py::class_<Digital::DigCompState>(m, "DigCompState")
        .def(py::init<>())
        .def_readwrite("input_states", &Digital::DigCompState::inputStates)
        .def_readwrite("output_states", &Digital::DigCompState::outputStates);

    py::class_<Digital::DigCompSimData, SimFnDataBase, std::shared_ptr<Digital::DigCompSimData>>(m, "DigCompSimData")
        .def(py::init<>())
        .def_readwrite("input_states", &Digital::DigCompSimData::inputStates)
        .def_readwrite("output_states", &Digital::DigCompSimData::outputStates)
        .def_readwrite("prev_state", &Digital::DigCompSimData::prevState)
        .def_readwrite("sim_time", &Digital::DigCompSimData::simTime)
        .def_readwrite("expressions", &Digital::DigCompSimData::expressions);

    py::class_<Digital::DigCompDef, EvtBasedCompDef, std::shared_ptr<Digital::DigCompDef>>(m, "DigCompDef")
        .def(py::init<>())
        .def_property("input_slots_info",
                      py::overload_cast<>(&Digital::DigCompDef::getInputSlotsInfo),
                      py::overload_cast<const SlotsGroupInfo &>(&Digital::DigCompDef::setInputSlotsInfo))
        .def_property("output_slots_info",
                      py::overload_cast<>(&Digital::DigCompDef::getOutputSlotsInfo),
                      py::overload_cast<const SlotsGroupInfo &>(&Digital::DigCompDef::setOutputSlotsInfo))
        .def_property("op_info",
                      py::overload_cast<>(&Digital::DigCompDef::getOpInfo),
                      py::overload_cast<const OperatorInfo &>(&Digital::DigCompDef::setOpInfo))
        .def_property("behavior_type",
                      py::overload_cast<>(&Digital::DigCompDef::getBehaviorType),
                      py::overload_cast<const ComponentBehaviorType &>(&Digital::DigCompDef::setBehaviorType))
        .def_property("output_expressions",
                      py::overload_cast<>(&Digital::DigCompDef::getOutputExpressions),
                      py::overload_cast<const std::vector<std::string> &>(&Digital::DigCompDef::setOutputExpressions))
        .def_property("auto_reschedule",
                      py::overload_cast<>(&Digital::DigCompDef::getAutoReschedule),
                      py::overload_cast<const bool &>(&Digital::DigCompDef::setAutoReschedule))
        .def_property("sim_fn",
                      py::overload_cast<>(&Digital::DigCompDef::getSimFn),
                      py::overload_cast<const Digital::DigCompDef::TDigSimFn &>(
                          &Digital::DigCompDef::setSimFn))
        // .def_property("prop_delay",                                                                        //
        //               [](const Digital::DigCompDef &self) -> Bess::TimeNs { return self.getPropDelay(); }, //
        //               [](Digital::DigCompDef &self, const Bess::TimeNs &value) { self.setPropDelay(value); })

        .def("compute_expressions_if_needed", &Digital::DigCompDef::computeExpressionsIfNeeded)
        .def("clone", &Digital::DigCompDef::clone);

    py::class_<Digital::DigSimComp, EvtBasedSimComp, std::shared_ptr<Digital::DigSimComp>>(m, "DigSimComp")
        .def(py::init<>())
        .def_static("from_def", &Digital::DigSimComp::fromDef)
        .def_property("input_states",
                      py::overload_cast<>(&Digital::DigSimComp::getInputStates),
                      py::overload_cast<const std::vector<SlotState> &>(&Digital::DigSimComp::setInputStates))
        .def_property("output_states",
                      py::overload_cast<>(&Digital::DigSimComp::getOutputStates),
                      py::overload_cast<const std::vector<SlotState> &>(&Digital::DigSimComp::setOutputStates))
        .def_property("input_connections",
                      py::overload_cast<>(&Digital::DigSimComp::getInputConnections),
                      py::overload_cast<const Connections &>(&Digital::DigSimComp::setInputConnections))
        .def_property("output_connections",
                      py::overload_cast<>(&Digital::DigSimComp::getOutputConnections),
                      py::overload_cast<const Connections &>(&Digital::DigSimComp::setOutputConnections))
        .def_property("input_connected",
                      py::overload_cast<>(&Digital::DigSimComp::getIsInputConnected),
                      py::overload_cast<const std::vector<bool> &>(&Digital::DigSimComp::setIsInputConnected))
        .def_property("output_connected",
                      py::overload_cast<>(&Digital::DigSimComp::getIsOutputConnected),
                      py::overload_cast<const std::vector<bool> &>(&Digital::DigSimComp::setIsOutputConnected))
        .def_property("net_uuid",
                      py::overload_cast<>(&Digital::DigSimComp::getNetUuid),
                      py::overload_cast<const Bess::UUID &>(&Digital::DigSimComp::setNetUuid))
        .def("to_json", &Digital::DigSimComp::toJson);

    py::class_<SimDriver, PySimDriver, std::shared_ptr<SimDriver>>(m, "SimDriver")
        .def(py::init<>())
        .def("get_name", &SimDriver::getName)
        .def("supports_def", &SimDriver::suuportsDef, py::arg("definition"))
        .def("create_component", &SimDriver::createComp, py::arg("definition"))
        .def("init", &SimDriver::init)
        .def("pause", &SimDriver::pause)
        .def("resume", &SimDriver::resume)
        .def("stop", &SimDriver::stop)
        .def("reset", &SimDriver::reset)
        .def("destroy", &SimDriver::destroy)
        .def("step", &SimDriver::step)
        .def("is_initialized", &SimDriver::isInitialized)
        .def("is_running", &SimDriver::isRunning)
        .def("is_paused", &SimDriver::isPaused)
        .def("is_stopped", &SimDriver::isStopped)
        .def("is_destroyed", &SimDriver::isDestroyed)
        .def("has_component", &SimDriver::hasComponent)
        .def("can_connect_components", [](const SimDriver &self, const Bess::UUID &src, int srcSlotIdx, SlotType srcType, const Bess::UUID &dst, int dstSlotIdx, SlotType dstType) { return self.canConnectComponents(SimulationEngine::instance(),
                                                                                                                                                                                                                      src,
                                                                                                                                                                                                                      srcSlotIdx,
                                                                                                                                                                                                                      srcType,
                                                                                                                                                                                                                      dst,
                                                                                                                                                                                                                      dstSlotIdx,
                                                                                                                                                                                                                      dstType); }, py::arg("src"), py::arg("src_slot_idx"), py::arg("src_type"), py::arg("dst"), py::arg("dst_slot_idx"), py::arg("dst_type"))
        .def("connect_component", [](SimDriver &self, const Bess::UUID &src, int srcSlotIdx, SlotType srcType, const Bess::UUID &dst, int dstSlotIdx, SlotType dstType, bool overrideConn) { return self.connectComponent(SimulationEngine::instance(),
                                                                                                                                                                                                                          src,
                                                                                                                                                                                                                          srcSlotIdx,
                                                                                                                                                                                                                          srcType,
                                                                                                                                                                                                                          dst,
                                                                                                                                                                                                                          dstSlotIdx,
                                                                                                                                                                                                                          dstType,
                                                                                                                                                                                                                          overrideConn); }, py::arg("src"), py::arg("src_slot_idx"), py::arg("src_type"), py::arg("dst"), py::arg("dst_slot_idx"), py::arg("dst_type"), py::arg("override_conn") = false)
        .def("delete_connection", [](SimDriver &self, const Bess::UUID &compA, SlotType pinAType, int idxA, const Bess::UUID &compB, SlotType pinBType, int idxB) { self.deleteConnection(SimulationEngine::instance(),
                                                                                                                                                                                          compA,
                                                                                                                                                                                          pinAType,
                                                                                                                                                                                          idxA,
                                                                                                                                                                                          compB,
                                                                                                                                                                                          pinBType,
                                                                                                                                                                                          idxB); }, py::arg("comp_a"), py::arg("pin_a_type"), py::arg("idx_a"), py::arg("comp_b"), py::arg("pin_b_type"), py::arg("idx_b"))
        .def("add_slot", [](SimDriver &self, const Bess::UUID &compId, SlotType type, int index) { return self.addSlot(SimulationEngine::instance(), compId, type, index); }, py::arg("component_id"), py::arg("slot_type"), py::arg("index"))
        .def("remove_slot", [](SimDriver &self, const Bess::UUID &compId, SlotType type, int index) { return self.removeSlot(SimulationEngine::instance(), compId, type, index); }, py::arg("component_id"), py::arg("slot_type"), py::arg("index"))
        .def("clear_pending_events", &SimDriver::clearPendingEvents)
        .def("component_count", [](const SimDriver &self) { return self.getComponentsMap().size(); });

    bind_event_based_sim_driver(m);

    py::class_<Digital::DigitalSimDriver, EvtBasedSimDriver, std::shared_ptr<Digital::DigitalSimDriver>>(m, "DigitalSimDriver")
        .def(py::init<>())
        .def("supports_def", &Digital::DigitalSimDriver::suuportsDef)
        .def("create_component", &Digital::DigitalSimDriver::createComp)
        .def("simulate", &Digital::DigitalSimDriver::simulate)
        .def("add_component", &Digital::DigitalSimDriver::addComponent,
             py::arg("component"), py::arg("schedule_sim") = true)
        .def("can_connect_components", [](Digital::DigitalSimDriver &self, const Bess::UUID &src, int srcSlotIdx, SlotType srcType, const Bess::UUID &dst, int dstSlotIdx, SlotType dstType) { return self.canConnectComponents(SimulationEngine::instance(),
                                                                                                                                                                                                                                src,
                                                                                                                                                                                                                                srcSlotIdx,
                                                                                                                                                                                                                                srcType,
                                                                                                                                                                                                                                dst,
                                                                                                                                                                                                                                dstSlotIdx,
                                                                                                                                                                                                                                dstType); }, py::arg("src"), py::arg("src_slot_idx"), py::arg("src_type"), py::arg("dst"), py::arg("dst_slot_idx"), py::arg("dst_type"))
        .def("connect_component", [](Digital::DigitalSimDriver &self, const Bess::UUID &src, int srcSlotIdx, SlotType srcType, const Bess::UUID &dst, int dstSlotIdx, SlotType dstType, bool overrideConn) { return self.connectComponent(SimulationEngine::instance(),
                                                                                                                                                                                                                                          src,
                                                                                                                                                                                                                                          srcSlotIdx,
                                                                                                                                                                                                                                          srcType,
                                                                                                                                                                                                                                          dst,
                                                                                                                                                                                                                                          dstSlotIdx,
                                                                                                                                                                                                                                          dstType,
                                                                                                                                                                                                                                          overrideConn); }, py::arg("src"), py::arg("src_slot_idx"), py::arg("src_type"), py::arg("dst"), py::arg("dst_slot_idx"), py::arg("dst_type"), py::arg("override_conn") = false)
        .def("delete_connection", [](Digital::DigitalSimDriver &self, const Bess::UUID &compA, SlotType pinAType, int idxA, const Bess::UUID &compB, SlotType pinBType, int idxB) { self.deleteConnection(SimulationEngine::instance(),
                                                                                                                                                                                                          compA,
                                                                                                                                                                                                          pinAType,
                                                                                                                                                                                                          idxA,
                                                                                                                                                                                                          compB,
                                                                                                                                                                                                          pinBType,
                                                                                                                                                                                                          idxB); }, py::arg("comp_a"), py::arg("pin_a_type"), py::arg("idx_a"), py::arg("comp_b"), py::arg("pin_b_type"), py::arg("idx_b"))
        .def("add_slot", [](Digital::DigitalSimDriver &self, const Bess::UUID &compId, SlotType type, int index) { return self.addSlot(SimulationEngine::instance(), compId, type, index); }, py::arg("component_id"), py::arg("slot_type"), py::arg("index"))
        .def("remove_slot", [](Digital::DigitalSimDriver &self, const Bess::UUID &compId, SlotType type, int index) { return self.removeSlot(SimulationEngine::instance(), compId, type, index); }, py::arg("component_id"), py::arg("slot_type"), py::arg("index"));
}
