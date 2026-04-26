#include "common/types.h"
#include "drivers/digital_sim_driver.h"
#include "drivers/event_based_sim_driver.h"
#include "drivers/sim_driver.h"
#include "expression_evalutator/expr_evaluator.h"

#include <pybind11/functional.h>
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

namespace py = pybind11;

void bind_dig_sim_driver(py::module_ &m) {
    using namespace Bess::SimEngine::Drivers;
    using namespace Bess::SimEngine;

    py::class_<Digital::DigCompState>(m, "DigCompState")
        .def(py::init<>())
        .def_readwrite("input_states", &Digital::DigCompState::inputStates)
        .def_readwrite("output_states", &Digital::DigCompState::outputStates);

    py::class_<Digital::DigCompSimData,
               SimFnDataBase,
               std::shared_ptr<Digital::DigCompSimData>>(m, "DigCompSimData")
        .def(py::init<>())
        .def_readwrite("input_states", &Digital::DigCompSimData::inputStates)
        .def_readwrite("output_states", &Digital::DigCompSimData::outputStates)
        .def_readwrite("prev_state", &Digital::DigCompSimData::prevState)
        .def_readwrite("sim_time", &Digital::DigCompSimData::simTime)
        .def_readwrite("expressions", &Digital::DigCompSimData::expressions);

    auto from_operator_info = [](const std::string &name,
                                 const std::string &group_name,
                                 const SlotsGroupInfo &inputs,
                                 const SlotsGroupInfo &outputs,
                                 Bess::TimeNs prop_delay,
                                 OperatorInfo info) -> std::shared_ptr<Digital::DigCompDef> {
        py::gil_scoped_acquire gil;
        auto comp_def = std::make_shared<Digital::DigCompDef>();
        comp_def->setName(name);
        comp_def->setGroupName(group_name);
        comp_def->setInputSlotsInfo(inputs);
        comp_def->setOutputSlotsInfo(outputs);
        comp_def->setPropDelay(prop_delay);
        comp_def->setOpInfo(info);
        comp_def->setSimFn(ExprEval::exprEvalSimFunc);
        return comp_def;
    };

    py::class_<Digital::DigCompDef,
               EvtBasedCompDef,
               std::shared_ptr<Digital::DigCompDef>>(m, "DigCompDef")
        .def(py::init<>())
        .def_static("from_operator", from_operator_info,
                    py::arg("name"),
                    py::arg("group_name"),
                    py::arg("inputs"),
                    py::arg("outputs"),
                    py::arg("prop_delay"),
                    py::arg("op_info"),
                    "Create a ComponentDefinition from operator info.")
        .def_property("input_slots_info",
                      py::overload_cast<>(&Digital::DigCompDef::getInputSlotsInfo),
                      py::overload_cast<const SlotsGroupInfo &>(
                          &Digital::DigCompDef::setInputSlotsInfo))
        .def_property("output_slots_info",
                      py::overload_cast<>(&Digital::DigCompDef::getOutputSlotsInfo),
                      py::overload_cast<const SlotsGroupInfo &>(
                          &Digital::DigCompDef::setOutputSlotsInfo))
        .def_property("op_info",
                      py::overload_cast<>(&Digital::DigCompDef::getOpInfo),
                      py::overload_cast<const OperatorInfo &>(
                          &Digital::DigCompDef::setOpInfo))
        .def_property("behavior_type",
                      py::overload_cast<>(&Digital::DigCompDef::getBehaviorType),
                      py::overload_cast<const ComponentBehaviorType &>(
                          &Digital::DigCompDef::setBehaviorType))
        .def_property("output_expressions",
                      py::overload_cast<>(&Digital::DigCompDef::getOutputExpressions),
                      py::overload_cast<const std::vector<std::string> &>(
                          &Digital::DigCompDef::setOutputExpressions))
        .def_property("auto_reschedule",
                      py::overload_cast<>(&Digital::DigCompDef::getAutoReschedule),
                      py::overload_cast<const bool &>(
                          &Digital::DigCompDef::setAutoReschedule))
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

    py::class_<Digital::DigitalSimDriver,
               EvtBasedSimDriver,
               std::shared_ptr<Digital::DigitalSimDriver>>(m, "DigitalSimDriver")
        .def(py::init<>())
        .def("supports_def",
             &Digital::DigitalSimDriver::supportsDef)
        .def("create_component",
             &Digital::DigitalSimDriver::createComp)
        .def("simulate",
             &Digital::DigitalSimDriver::simulate)
        .def("add_component",
             &Digital::DigitalSimDriver::addComponent,
             py::arg("component"),
             py::arg("schedule_sim") = true)
        .def("can_connect_components",
             &Digital::DigitalSimDriver::canConnectComponents,
             py::arg("src"), py::arg("src_slot_idx"), py::arg("src_type"),
             py::arg("dst"), py::arg("dst_slot_idx"), py::arg("dst_type"))
        .def("connect_component",
             &Digital::DigitalSimDriver::connectComponent,
             py::arg("src"), py::arg("src_slot_idx"), py::arg("src_type"),
             py::arg("dst"), py::arg("dst_slot_idx"), py::arg("dst_type"),
             py::arg("override_conn") = false)
        .def("delete_connection",
             &Digital::DigitalSimDriver::deleteConnection,
             py::arg("comp_a"), py::arg("pin_a_type"), py::arg("idx_a"),
             py::arg("comp_b"), py::arg("pin_b_type"), py::arg("idx_b"))
        .def("add_slot",
             &Digital::DigitalSimDriver::addSlot,
             py::arg("component_id"),
             py::arg("slot_type"),
             py::arg("index"))
        .def("remove_slot",
             &Digital::DigitalSimDriver::removeSlot,
             py::arg("component_id"),
             py::arg("slot_type"),
             py::arg("index"));
}
