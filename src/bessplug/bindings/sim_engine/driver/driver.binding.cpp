#include "drivers/event_based_sim_driver.h"
#include "drivers/sim_driver.h"

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

    bool supportsDef(const std::shared_ptr<Bess::SimEngine::Drivers::CompDef> &def) const override {
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

    bool connectComponent(const Bess::UUID &src,
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

    void deleteConnection(const Bess::UUID &compA,
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

    bool addSlot(const Bess::UUID &compId,
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

    bool removeSlot(const Bess::UUID &compId,
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
void bind_dig_sim_driver(py::module_ &m);

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

    py::class_<SimEvt>(m, "SimEvt")
        .def(py::init<>())
        .def_readwrite("evt_id", &SimEvt::evtId)
        .def_readwrite("comp_id", &SimEvt::compId)
        .def_readwrite("scheduler_id", &SimEvt::schedulerId)
        .def_readwrite("sim_time", &SimEvt::simTime);

    py::class_<SimDriver, PySimDriver, std::shared_ptr<SimDriver>>(m, "SimDriver")
        .def(py::init<>())
        .def("get_name", &SimDriver::getName)
        .def("supports_def", &SimDriver::supportsDef, py::arg("definition"))
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
        .def("can_connect_components",
             &SimDriver::canConnectComponents,
             py::arg("src"), py::arg("src_slot_idx"), py::arg("src_type"),
             py::arg("dst"), py::arg("dst_slot_idx"), py::arg("dst_type"))
        .def("connect_component",
             &SimDriver::connectComponent,
             py::arg("src"), py::arg("src_slot_idx"), py::arg("src_type"),
             py::arg("dst"), py::arg("dst_slot_idx"), py::arg("dst_type"),
             py::arg("override_conn") = false)
        .def("delete_connection",
             &SimDriver::deleteConnection,
             py::arg("comp_a"), py::arg("pin_a_type"), py::arg("idx_a"),
             py::arg("comp_b"), py::arg("pin_b_type"), py::arg("idx_b"))
        .def("add_slot",
             &SimDriver::addSlot,
             py::arg("component_id"),
             py::arg("slot_type"),
             py::arg("index"))
        .def("remove_slot",
             &SimDriver::removeSlot,
             py::arg("component_id"),
             py::arg("slot_type"),
             py::arg("index"))
        .def("clear_pending_events",
             &SimDriver::clearPendingEvents)
        .def("component_count",
             [](const SimDriver &self) { return self.getComponentsMap().size(); });

    bind_event_based_sim_driver(m);
    bind_dig_sim_driver(m);
}
