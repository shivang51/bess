#include "math_sim_driver.h"
#include "common/types.h"
#include "sim_driver/event_based_sim_driver.h"
#include "sim_driver/sim_driver.h"

#include <pybind11/functional.h>
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

namespace py = pybind11;

class PyMathCompDef : public Bess::SimEngine::Drivers::Math::MathCompDef,
                      public py::trampoline_self_life_support {
  public:
    using Bess::SimEngine::Drivers::Math::MathCompDef::MathCompDef;

    std::shared_ptr<Bess::SimEngine::Drivers::CompDef> clone() const override {
        PYBIND11_OVERRIDE_NAME(
            std::shared_ptr<Bess::SimEngine::Drivers::CompDef>,
            MathCompDef,
            "clone",
            clone);
    }

    std::string getTypeName() const override {
        PYBIND11_OVERRIDE_NAME(
            std::string, MathCompDef, "get_type_name", getTypeName);
    }

    Bess::TimeNs getSelfSimDelay() override {
        PYBIND11_OVERRIDE_NAME(
            Bess::TimeNs, MathCompDef, "get_self_sim_delay", getSelfSimDelay);
    }

    Bess::TimeNs getInitialSimDelay() override {
        PYBIND11_OVERRIDE_NAME(Bess::TimeNs,
                               MathCompDef,
                               "get_initial_sim_delay",
                               getInitialSimDelay);
    }

    Bess::TimeNs
    getSelfSimDelayAfter(uint64_t completedSelfSimulations) override {
        PYBIND11_OVERRIDE_NAME(Bess::TimeNs,
                               MathCompDef,
                               "get_self_sim_delay_after",
                               getSelfSimDelayAfter,
                               completedSelfSimulations);
    }

    Json::Value toJson() const override {
        PYBIND11_OVERRIDE_NAME(Json::Value, MathCompDef, "to_json", toJson);
    }
};

void bind_math_sim_driver(py::module_ &m) {
    using namespace Bess::SimEngine;
    using namespace Bess::SimEngine::Drivers;
    using namespace Bess::SimEngine::Drivers::Math;

    py::enum_<MathOpKind>(m, "MathOpKind")
        .value("NONE", MathOpKind::none)
        .value("ADD", MathOpKind::add)
        .value("SUBTRACT", MathOpKind::subtract)
        .export_values();

    py::class_<MathCompState>(m, "MathCompState")
        .def(py::init<>())
        .def_readwrite("input_states", &MathCompState::inputStates)
        .def_readwrite("output_states", &MathCompState::outputStates);

    py::class_<MathCompSimData,
               SimFnDataBase,
               std::shared_ptr<MathCompSimData>>(m, "MathCompSimData")
        .def(py::init<>())
        .def_readwrite("input_states", &MathCompSimData::inputStates)
        .def_readwrite("output_states", &MathCompSimData::outputStates)
        .def_readwrite("sim_time", &MathCompSimData::simTime)
        .def_readwrite("prev_state", &MathCompSimData::prevState);

    auto from_scalar_fn = [](const std::string &name,
                             const std::string &group_name,
                             const PortDescriptor &inputs,
                             const PortDescriptor &outputs,
                             Bess::TimeNs prop_delay,
                             const py::function &scalar_function)
        -> std::shared_ptr<MathCompDef> {
        auto compDef = std::make_shared<MathCompDef>();
        compDef->setName(name);
        compDef->setGroupName(group_name);
        compDef->setInputPortDescriptor(inputs);
        compDef->setOutputPortDescriptor(outputs);
        compDef->setPropDelay(prop_delay);
        compDef->setScalarFn(
            [scalar_function](Bess::TimeMs time,
                              const std::vector<double> &values) -> double {
                py::gil_scoped_acquire gil;
                return scalar_function(time, values).cast<double>();
            });
        return compDef;
    };

    py::class_<MathCompDef, PyMathCompDef, EvtBasedCompDef, py::smart_holder>(
        m, "MathCompDef")
        .def(py::init<>())
        .def_static("make_binary_op",
                    &MathCompDef::makeBinaryOp,
                    py::arg("name"),
                    py::arg("group_name"),
                    py::arg("op_kind"),
                    py::arg("prop_delay"))
        .def_static("from_scalar_fn",
                    from_scalar_fn,
                    py::arg("name"),
                    py::arg("group_name"),
                    py::arg("inputs"),
                    py::arg("outputs"),
                    py::arg("prop_delay"),
                    py::arg("scalar_function"))
        .def_property(
            "input_port_descriptor",
            [](const MathCompDef &self) {
                return self.getInputPortDescriptor();
            },
            &MathCompDef::setInputPortDescriptor)
        .def_property(
            "output_port_descriptor",
            [](const MathCompDef &self) {
                return self.getOutputPortDescriptor();
            },
            &MathCompDef::setOutputPortDescriptor)
        .def_property(
            "op_kind",
            py::overload_cast<>(&MathCompDef::getOpKind),
            py::overload_cast<const MathOpKind &>(&MathCompDef::setOpKind))
        .def_property(
            "auto_reschedule",
            py::overload_cast<>(&MathCompDef::getAutoReschedule),
            py::overload_cast<const bool &>(&MathCompDef::setAutoReschedule))
        .def_property(
            "prop_delay",
            py::overload_cast<>(&MathCompDef::getPropDelay),
            py::overload_cast<const Bess::TimeNs &>(&MathCompDef::setPropDelay))
        .def("set_scalar_fn",
             [](MathCompDef &self, const py::function &scalar_function) {
                 self.setScalarFn(
                     [scalar_function](
                         Bess::TimeMs time,
                         const std::vector<double> &values) -> double {
                         py::gil_scoped_acquire gil;
                         return scalar_function(values).cast<double>();
                     });
             })
        .def("compute_scalar_fn_if_needed",
             &MathCompDef::computeScalarFnIfNeeded)
        .def("clone", &MathCompDef::clone)
        .def("get_type_name", &MathCompDef::getTypeName);

    py::class_<MathSimComp, EvtBasedSimComp, std::shared_ptr<MathSimComp>>(
        m, "MathSimComp")
        .def(py::init<>())
        .def_static("from_def",
                    &MathSimComp::template fromDef<MathSimComp>,
                    py::arg("comp_def"),
                    py::arg("clone_def") = true)
        .def_property("input_states",
                      py::overload_cast<>(&MathSimComp::getInputStates),
                      py::overload_cast<const std::vector<PortState> &>(
                          &MathSimComp::setInputStates))
        .def_property("output_states",
                      py::overload_cast<>(&MathSimComp::getOutputStates),
                      py::overload_cast<const std::vector<PortState> &>(
                          &MathSimComp::setOutputStates))
        .def_property("input_connections",
                      py::overload_cast<>(&MathSimComp::getInputConnections),
                      py::overload_cast<const Connections &>(
                          &MathSimComp::setInputConnections))
        .def_property("output_connections",
                      py::overload_cast<>(&MathSimComp::getOutputConnections),
                      py::overload_cast<const Connections &>(
                          &MathSimComp::setOutputConnections))
        .def_property("input_connected",
                      py::overload_cast<>(&MathSimComp::getIsInputConnected),
                      py::overload_cast<const std::vector<bool> &>(
                          &MathSimComp::setIsInputConnected))
        .def_property("output_connected",
                      py::overload_cast<>(&MathSimComp::getIsOutputConnected),
                      py::overload_cast<const std::vector<bool> &>(
                          &MathSimComp::setIsOutputConnected))
        .def_property(
            "net_uuid",
            py::overload_cast<>(&MathSimComp::getNetUuid),
            py::overload_cast<const Bess::UUID &>(&MathSimComp::setNetUuid));

    py::class_<MathSimDriver,
               EvtBasedSimDriver,
               std::shared_ptr<MathSimDriver>>(m, "MathSimDriver")
        .def(py::init<>())
        .def_static("register_driver", &registerMathSimDriver)
        .def("supports_def", &MathSimDriver::supportsDef)
        .def("create_component", &MathSimDriver::createComp)
        .def("simulate", &MathSimDriver::simulate)
        .def("add_component",
             &MathSimDriver::addComponent,
             py::arg("component"),
             py::arg("schedule_sim") = true)
        .def("can_connect_ports",
             &MathSimDriver::canConnectPorts,
             py::arg("src"),
             py::arg("dst"))
        .def("connect_ports",
             &MathSimDriver::connectPorts,
             py::arg("src"),
             py::arg("dst"),
             py::arg("override_conn") = false)
        .def("delete_connection",
             &MathSimDriver::deleteConnection,
             py::arg("port_a"),
             py::arg("port_b"))
        .def("add_port",
             &MathSimDriver::addPort,
             py::arg("port"),
             py::arg("force") = false)
        .def("remove_port",
             &MathSimDriver::removePort,
             py::arg("port"),
             py::arg("force") = false)
        .def("collapse_inputs", &MathSimDriver::collapseInputs)
        .def("get_port_state", &MathSimDriver::getPortState)
        .def("get_input_port_states", &MathSimDriver::getInputPortStates);
}
