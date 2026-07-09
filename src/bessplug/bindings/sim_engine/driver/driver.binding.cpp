#include "sim_driver/event_based_sim_driver.h"
#include "sim_driver/sim_driver.h"

#include <pybind11/functional.h>
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

namespace py = pybind11;

class PySimDriver : public Bess::SimEngine::Drivers::SimDriver {
  public:
    using Bess::SimEngine::Drivers::SimDriver::SimDriver;
    using CanConnectResult = std::pair<bool, std::string>;

    void run() override {
        PYBIND11_OVERRIDE_PURE_NAME(
            void, Bess::SimEngine::Drivers::SimDriver, "run", run);
    }

    std::string getName() const override {
        PYBIND11_OVERRIDE_PURE_NAME(std::string,
                                    Bess::SimEngine::Drivers::SimDriver,
                                    "get_name",
                                    getName);
    }

    bool supportsDef(const std::shared_ptr<Bess::SimEngine::Drivers::CompDef>
                         &def) const override {
        PYBIND11_OVERRIDE_PURE_NAME(bool,
                                    Bess::SimEngine::Drivers::SimDriver,
                                    "supports_def",
                                    suuportsDef,
                                    def);
    }

    std::shared_ptr<Bess::SimEngine::Drivers::SimComponent>
    createComp(const std::shared_ptr<Bess::SimEngine::Drivers::CompDef> &def,
               bool cloneDef = true) override {
        PYBIND11_OVERRIDE_PURE_NAME(
            std::shared_ptr<Bess::SimEngine::Drivers::SimComponent>,
            Bess::SimEngine::Drivers::SimDriver,
            "create_component",
            createComp,
            def,
            cloneDef);
    }

    CanConnectResult
    canConnectPorts(const Bess::SimEngine::PortRef &src,
                    const Bess::SimEngine::PortRef &dst) const override {
        PYBIND11_OVERRIDE_PURE_NAME(CanConnectResult,
                                    Bess::SimEngine::Drivers::SimDriver,
                                    "can_connect_ports",
                                    canConnectPorts,
                                    src,
                                    dst);
    }

    bool connectPorts(const Bess::SimEngine::PortRef &src,
                      const Bess::SimEngine::PortRef &dst,
                      bool overrideConn) override {
        PYBIND11_OVERRIDE_PURE_NAME(bool,
                                    Bess::SimEngine::Drivers::SimDriver,
                                    "connect_ports",
                                    connectPorts,
                                    src,
                                    dst,
                                    overrideConn);
    }

    void deleteConnection(const Bess::SimEngine::PortRef &portA,
                          const Bess::SimEngine::PortRef &portB) override {
        PYBIND11_OVERRIDE_PURE_NAME(void,
                                    Bess::SimEngine::Drivers::SimDriver,
                                    "delete_connection",
                                    deleteConnection,
                                    portA,
                                    portB);
    }

    Bess::SimEngine::Drivers::PortCountChangeRes
    addPort(const Bess::SimEngine::PortRef &port,
            bool force = false) override {
        PYBIND11_OVERRIDE_PURE_NAME(
            Bess::SimEngine::Drivers::PortCountChangeRes,
            Bess::SimEngine::Drivers::SimDriver,
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
            Bess::SimEngine::Drivers::SimDriver,
            "remove_port",
            removePort,
            port,
            force);
    }

    void clearPendingEvents() override {
        PYBIND11_OVERRIDE_NAME(void,
                               Bess::SimEngine::Drivers::SimDriver,
                               "clear_pending_events",
                               clearPendingEvents);
    }

    Json::Value toJson() const override {
        PYBIND11_OVERRIDE_PURE_NAME(Json::Value,
                                    Bess::SimEngine::Drivers::SimDriver,
                                    "to_json",
                                    toJson);
    }

    void loadJson(const Json::Value &json) override {
        PYBIND11_OVERRIDE_PURE_NAME(void,
                                    Bess::SimEngine::Drivers::SimDriver,
                                    "load_json",
                                    loadJson,
                                    json);
    }

    bool isSimStable() const override {
        PYBIND11_OVERRIDE_PURE_NAME(bool,
                                    Bess::SimEngine::Drivers::SimDriver,
                                    "is_sim_stable",
                                    isSimStable);
    }
};

class PyCompDef : public Bess::SimEngine::Drivers::CompDef,
                  public py::trampoline_self_life_support {
  public:
    using Bess::SimEngine::Drivers::CompDef::CompDef;

    std::shared_ptr<Bess::SimEngine::Drivers::CompDef> clone() const override {
        PYBIND11_OVERRIDE_NAME(
            std::shared_ptr<Bess::SimEngine::Drivers::CompDef>,
            CompDef,
            "clone",
            clone);
    }

    std::string getTypeName() const override {
        PYBIND11_OVERRIDE_NAME(
            std::string, CompDef, "get_type_name", getTypeName);
    }

    Json::Value toJson() const override {
        PYBIND11_OVERRIDE_NAME(Json::Value, CompDef, "to_json", toJson);
    }
};

void bind_event_based_sim_driver(py::module_ &m);
void bind_dig_sim_driver(py::module_ &m);
void bind_math_sim_driver(py::module_ &m);

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

    py::class_<PortCountChangeRes>(m, "PortCountChangeRes")
        .def(py::init<>())
        .def_static("no_change", &PortCountChangeRes::noChange)
        .def_static("inputs_changed", &PortCountChangeRes::inputsChanged)
        .def_static("outputs_changed", &PortCountChangeRes::outputsChanged)
        .def_static("both_changed", &PortCountChangeRes::bothChanged)
        .def_readwrite("changed_inputs", &PortCountChangeRes::changedInputs)
        .def_readwrite("changed_outputs", &PortCountChangeRes::changedOutputs)
        .def("has_change", &PortCountChangeRes::hasChange);

    py::class_<SimFnDataBase, std::shared_ptr<SimFnDataBase>>(m, "SimFnData")
        .def(py::init<>())
        .def_readwrite("sim_dependants", &SimFnDataBase::simDependants);

    py::class_<CompDef, PyCompDef, py::smart_holder>(m, "CompDef")
        .def_property("name",
                      py::overload_cast<>(&CompDef::getName),
                      py::overload_cast<const std::string &>(&CompDef::setName))
        .def("type", &CompDef::getTypeName)
        .def("clone", &CompDef::clone)
        .def("to_json", &CompDef::toJson)
        .def("get_type_name", &CompDef::getTypeName)
        .def_property(
            "group_name",
            py::overload_cast<>(&CompDef::getGroupName),
            py::overload_cast<const std::string &>(&CompDef::setGroupName));

    py::class_<SimComponent, std::shared_ptr<SimComponent>>(m, "SimComponent")
        .def(py::init<>())
        .def_property(
            "uuid",
            py::overload_cast<>(&SimComponent::getUuid),
            py::overload_cast<const Bess::UUID &>(&SimComponent::setUuid))
        .def_property(
            "name",
            py::overload_cast<>(&SimComponent::getName),
            py::overload_cast<const std::string &>(&SimComponent::setName))
        .def_property_readonly("definition",
                               [](const SimComponent &self) {
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

    py::class_<SimDriver, PySimDriver, std::shared_ptr<SimDriver>>(m,
                                                                   "SimDriver")
        .def(py::init<>())
        .def("get_name", &SimDriver::getName)
        .def("supports_def", &SimDriver::supportsDef, py::arg("definition"))
        .def("create_component",
             &SimDriver::createComp,
             py::arg("definition"),
             py::arg("clone_def") = true)
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
        .def("can_connect_ports",
             &SimDriver::canConnectPorts,
             py::arg("src"),
             py::arg("dst"))
        .def("connect_ports",
             &SimDriver::connectPorts,
             py::arg("src"),
             py::arg("dst"),
             py::arg("override_conn") = false)
        .def("delete_connection",
             &SimDriver::deleteConnection,
             py::arg("port_a"),
             py::arg("port_b"))
        .def("add_port",
             &SimDriver::addPort,
             py::arg("port"),
             py::arg("force") = false)
        .def("remove_port",
             &SimDriver::removePort,
             py::arg("port"),
             py::arg("force") = false)
        .def("clear_pending_events", &SimDriver::clearPendingEvents)
        .def("is_sim_stable", &SimDriver::isSimStable)
        .def("to_json", &SimDriver::toJson)
        .def("load_json", &SimDriver::loadJson, py::arg("json"))
        .def("component_count", [](const SimDriver &self) {
            return self.getComponentsMap().size();
        });

    py::class_<Net, std::shared_ptr<Net>>(m, "Net")
        .def(py::init<>())
        .def_property("uuid",
                      &Net::getUUID,
                      py::overload_cast<const Bess::UUID &>(&Net::setUUID))
        .def("add_component", &Net::addComponent, py::arg("component_uuid"))
        .def("get_components",
             &Net::getComponents,
             py::return_value_policy::reference_internal)
        .def("join", &Net::join, py::arg("other"))
        .def("remove_component",
             &Net::removeComponent,
             py::arg("component_uuid"))
        .def("remove_components",
             &Net::removeComponents,
             py::arg("component_uuids"))
        .def("add_components", &Net::addComponents, py::arg("component_uuids"))
        .def("set_components", &Net::setComponents, py::arg("component_uuids"))
        .def("clear", &Net::clear)
        .def("size", &Net::size);

    bind_event_based_sim_driver(m);
    bind_dig_sim_driver(m);
    bind_math_sim_driver(m);
}
