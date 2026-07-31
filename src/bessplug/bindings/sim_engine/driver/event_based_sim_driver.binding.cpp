#include "sim_driver/event_based_sim_driver.h"
#include "common/bess_uuid.h"
#include "common/types.h"
#include "sim_driver/sim_driver.h"

#include <memory>
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

namespace py = pybind11;

class PyEvtBasedSimDriver : public Bess::SimEngine::Drivers::EvtBasedSimDriver {
  public:
    using Bess::SimEngine::Drivers::EvtBasedSimDriver::EvtBasedSimDriver;
    using CanConnectResult = std::pair<bool, std::string>;

    bool isSimStable() const override {
        PYBIND11_OVERRIDE_PURE_NAME(bool,
                                    Bess::SimEngine::Drivers::EvtBasedSimDriver,
                                    "is_sim_stable",
                                    isSimStable);
    }

    std::string getName() const override {
        PYBIND11_OVERRIDE_PURE_NAME(std::string,
                                    Bess::SimEngine::Drivers::EvtBasedSimDriver,
                                    "get_name",
                                    getName);
    }

    bool supportsDef(const std::shared_ptr<Bess::SimEngine::Drivers::CompDef>
                         &def) const override {
        PYBIND11_OVERRIDE_PURE_NAME(bool,
                                    Bess::SimEngine::Drivers::EvtBasedSimDriver,
                                    "supports_def",
                                    suuportsDef,
                                    def);
    }

    std::shared_ptr<Bess::SimEngine::Drivers::SimComponent>
    createComp(const std::shared_ptr<Bess::SimEngine::Drivers::CompDef> &def,
               bool cloneDef = true) override {
        PYBIND11_OVERRIDE_PURE_NAME(
            std::shared_ptr<Bess::SimEngine::Drivers::SimComponent>,
            Bess::SimEngine::Drivers::EvtBasedSimDriver,
            "create_component",
            createComp,
            def,
            cloneDef);
    }

    CanConnectResult
    canConnectPorts(const Bess::SimEngine::PortRef &src,
                    const Bess::SimEngine::PortRef &dst) const override {
        PYBIND11_OVERRIDE_PURE_NAME(CanConnectResult,
                                    Bess::SimEngine::Drivers::EvtBasedSimDriver,
                                    "can_connect_ports",
                                    canConnectPorts,
                                    src,
                                    dst);
    }

    bool connectPorts(const Bess::SimEngine::PortRef &src,
                      const Bess::SimEngine::PortRef &dst,
                      bool overrideConn) override {
        PYBIND11_OVERRIDE_PURE_NAME(bool,
                                    Bess::SimEngine::Drivers::EvtBasedSimDriver,
                                    "connect_ports",
                                    connectPorts,
                                    src,
                                    dst,
                                    overrideConn);
    }

    void deleteConnection(const Bess::SimEngine::PortRef &portA,
                          const Bess::SimEngine::PortRef &portB) override {
        PYBIND11_OVERRIDE_PURE_NAME(void,
                                    Bess::SimEngine::Drivers::EvtBasedSimDriver,
                                    "delete_connection",
                                    deleteConnection,
                                    portA,
                                    portB);
    }

    Bess::SimEngine::Drivers::PortCountChangeRes
    addPort(const Bess::SimEngine::PortRef &port, bool force = false) override {
        PYBIND11_OVERRIDE_PURE_NAME(
            Bess::SimEngine::Drivers::PortCountChangeRes,
            Bess::SimEngine::Drivers::EvtBasedSimDriver,
            "add_port",
            addPort,
            port,
            force);
    }

    Bess::SimEngine::Drivers::PortCountChangeRes
    removePort(const Bess::SimEngine::PortRef &port,
               bool force = false) override {
        PYBIND11_OVERRIDE_PURE_NAME(
            Bess::SimEngine::Drivers::PortCountChangeRes,
            Bess::SimEngine::Drivers::EvtBasedSimDriver,
            "remove_port",
            removePort,
            port,
            force);
    }

    bool
    simulate(const Bess::SimEngine::Drivers::SimEvt &evt,
             const std::vector<Bess::SimEngine::PortState> &inputs) override {
        PYBIND11_OVERRIDE_PURE_NAME(bool,
                                    Bess::SimEngine::Drivers::EvtBasedSimDriver,
                                    "simulate",
                                    simulate,
                                    evt,
                                    inputs);
    }

    Bess::UUID addComponent(
        const std::shared_ptr<Bess::SimEngine::Drivers::SimComponent> &comp,
        bool scheduleSim) override {
        PYBIND11_OVERRIDE_NAME(Bess::UUID,
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

    std::vector<Bess::SimEngine::PortState>
    collapseInputs(const Bess::UUID &id) override {
        PYBIND11_OVERRIDE_NAME(std::vector<Bess::SimEngine::PortState>,
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

    Json::Value toJson() const override {
        PYBIND11_OVERRIDE_PURE_NAME(Json::Value,
                                    Bess::SimEngine::Drivers::EvtBasedSimDriver,
                                    "to_json",
                                    toJson);
    }

    void loadJson(const Json::Value &json) override {
        PYBIND11_OVERRIDE_PURE_NAME(void,
                                    Bess::SimEngine::Drivers::EvtBasedSimDriver,
                                    "load_json",
                                    loadJson,
                                    json);
    }
};

class PyEvtBasedCompDef : public Bess::SimEngine::Drivers::EvtBasedCompDef,
                          public py::trampoline_self_life_support {
  public:
    using Bess::SimEngine::Drivers::EvtBasedCompDef::EvtBasedCompDef;

    Bess::TimeNs getSelfSimDelay() override {
        PYBIND11_OVERRIDE_NAME(Bess::TimeNs,
                               Bess::SimEngine::Drivers::EvtBasedCompDef,
                               "get_self_sim_delay",
                               getSelfSimDelay);
    }

    Json::Value toJson() const override {
        PYBIND11_OVERRIDE_NAME(Json::Value,
                               Bess::SimEngine::Drivers::EvtBasedCompDef,
                               "to_json",
                               toJson);
    }
};

void bind_event_based_sim_driver(py::module_ &m) {
    using namespace Bess::SimEngine::Drivers;

    py::class_<EvtBasedCompDef, PyEvtBasedCompDef, CompDef, py::smart_holder>(
        m, "EvtBasedCompDef")
        .def_property("auto_reschedule",
                      py::overload_cast<>(&EvtBasedCompDef::getAutoReschedule),
                      py::overload_cast<const bool &>(
                          &EvtBasedCompDef::setAutoReschedule))
        .def_property("prop_delay",
                      py::overload_cast<>(&EvtBasedCompDef::getPropDelay),
                      py::overload_cast<const Bess::TimeNs &>(
                          &EvtBasedCompDef::setPropDelay))
        .def("get_self_sim_delay", &EvtBasedCompDef::getSelfSimDelay);

    py::class_<EvtBasedSimComp, SimComponent, std::shared_ptr<EvtBasedSimComp>>(
        m, "EvtBasedSimComp")
        .def(py::init<>())
        .def("get_prop_delay", &EvtBasedSimComp::getPropDelay)
        .def("get_sim_self", &EvtBasedSimComp::getSimSelf)
        .def("get_self_sim_delay", &EvtBasedSimComp::getSelfSimDelay);

    py::class_<EvtBasedSimDriver,
               SimDriver,
               PyEvtBasedSimDriver,
               std::shared_ptr<EvtBasedSimDriver>>(m, "EvtBasedSimDriver")
        .def(py::init<>())
        .def("run", &EvtBasedSimDriver::run)
        .def("simulate",
             &EvtBasedSimDriver::simulate,
             py::arg("event"),
             py::arg("inputs"))
        .def("add_component",
             &EvtBasedSimDriver::addComponent,
             py::arg("component"),
             py::arg("schedule_sim") = true)
        .def("get_dependants",
             &EvtBasedSimDriver::getDependants,
             py::arg("component_id"))
        .def("collapse_inputs",
             &EvtBasedSimDriver::collapseInputs,
             py::arg("component_id"))
        .def("on_before_run", &EvtBasedSimDriver::onBeforeRun)
        .def("schedule_event",
             &EvtBasedSimDriver::scheduleEvt,
             py::arg("component_id"),
             py::arg("sim_time"),
             py::arg("scheduler_id"),
             py::arg("notify") = true)
        .def("clear_pending_events", &EvtBasedSimDriver::clearPendingEvents);
}
