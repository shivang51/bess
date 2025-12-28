#include "types.h"
#include "component_definition.h"
#include "internal_types.h"

#include <cstdint>
#include <iostream>
#include <pybind11/functional.h>
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>
#include <pybind11/stl_bind.h>
#include <typeinfo>

namespace py = pybind11;

using namespace Bess::SimEngine;

void bindLogicState(py::module_ &m);
void bindPinState(py::module_ &m);
void bindComponentState(py::module_ &m);
void bindSlotsGroupType(py::module_ &m);
void bindSlotsGroupInfo(py::module_ &m);
void bindOperatorInfo(py::module_ &m);
void bindSlotCategory(py::module_ &m);
void bindComponentBehaviorType(py::module_ &m);

void bind_sim_engine_types(py::module_ &m) {
    py::enum_<LogicState>(m, "LogicState")
        .value("LOW", LogicState::low)
        .value("HIGH", LogicState::high)
        .value("UNKNOWN", LogicState::unknown)
        .value("HIGH_Z", LogicState::high_z)
        .export_values();

    py::class_<SlotState>(m, "PinState")
        .def(py::init<>())
        .def(py::init<const SlotState &>())
        .def(py::init<bool>(), py::arg("value"))
        .def(py::init([](LogicState state, long long last_change_time_ns) {
                 SlotState p;
                 p.state = state;
                 p.lastChangeTime = SimTime(last_change_time_ns);
                 return p;
             }),
             py::arg("state"), py::arg("last_change_time_ns"))
        .def_readwrite("state", &SlotState::state)
        .def_property(
            "last_change_time_ns",
            [](const SlotState &self) { return static_cast<long long>(self.lastChangeTime.count()); },
            [](SlotState &self, long long ns) { self.lastChangeTime = SimTime(ns); })
        .def("copy", [](const SlotState &self) { return SlotState(self); })
        .def("invert", [](SlotState &self) {
            switch (self.state) {
            case LogicState::low:
                self.state = LogicState::high;
                break;
            case LogicState::high:
                self.state = LogicState::low;
                break;
            case LogicState::unknown:
            case LogicState::high_z:
                // leave unchanged
                break;
            }
        })
        .def("__repr__", [](const SlotState &self) {
            const char *s = "UNKNOWN";
            switch (self.state) {
            case LogicState::low:
                s = "LOW";
                break;
            case LogicState::high:
                s = "HIGH";
                break;
            case LogicState::unknown:
                s = "UNKNOWN";
                break;
            case LogicState::high_z:
                s = "HIGH_Z";
                break;
            }
            return std::string("<PinState state=") + s + ", t_ns=" + std::to_string(self.lastChangeTime.count()) + ">";
        });

    py::class_<ComponentState>(m, "ComponentState")
        .def(py::init<>())
        .def(py::init<const ComponentState &>())
        .def_property(
            "input_states",
            [](ComponentState &self) -> std::vector<SlotState> & { return self.inputStates; },
            [](ComponentState &self, const std::vector<SlotState> &v) { self.inputStates = v; },
            py::return_value_policy::reference_internal)
        .def_property(
            "output_states",
            [](ComponentState &self) -> std::vector<SlotState> & {
                return self.outputStates;
            },
            [](ComponentState &self, const std::vector<SlotState> &v) {
                self.outputStates = v;
            },
            py::return_value_policy::reference_internal)
        .def("set_output_state", [](ComponentState &self, std::size_t idx, const SlotState &value) {
                if (idx >= self.outputStates.size()) {
                    throw py::index_error("output index out of range");
                }
                self.outputStates[idx] = value; }, py::arg("idx"), py::arg("value"))
        .def_readwrite("is_changed", &ComponentState::isChanged)
        .def("copy", [](const ComponentState &self) {
                ComponentState cpy = self;
                return cpy; })
        .def_property("input_connected", [](const ComponentState &self) { return self.inputConnected; }, [](ComponentState &self, const std::vector<bool> &v) { self.inputConnected = v; })
        .def_property("output_connected", [](const ComponentState &self) { return self.outputConnected; }, [](ComponentState &self, const std::vector<bool> &v) { self.outputConnected = v; })
        .def_property("aux_data", [](const ComponentState &self) -> py::object {
								if (self.auxData && self.auxData->type() == typeid(Bess::Py::OwnedPyObject)) {
										const auto &owned = std::any_cast<const Bess::Py::OwnedPyObject &>(*self.auxData);
										return owned.object;
								}
								return py::none(); }, [](ComponentState &self, py::object obj) {
								if (self.auxData && self.auxData->type() == typeid(Bess::Py::OwnedPyObject)) {
										delete self.auxData;
										self.auxData = nullptr;
								}
								self.auxData = new std::any(Bess::Py::OwnedPyObject{obj}); }, "Get or set the aux_data as a Python object if it was set via set_aux_pyobject.")
        .def("clear_aux_data", [](ComponentState &self) {
                if (self.auxData && self.auxData->type() == typeid(Bess::Py::OwnedPyObject)) {
                    delete self.auxData;
                    self.auxData = nullptr;
                } }, "Clear aux_data if it was set via set_aux_pyobject.");

    py::enum_<SlotType>(m, "PinType")
        .value("INPUT", SlotType::digitalInput)
        .value("OUTPUT", SlotType::digitalOutput)
        .export_values();

    py::enum_<SlotsGroupType>(m, "SlotsGroupType")
        .value("NONE", SlotsGroupType::none)
        .value("INPUT", SlotsGroupType::input)
        .value("OUTPUT", SlotsGroupType::output)
        .export_values();

    py::class_<SlotsGroupInfo>(m, "SlotsGroupInfo")
        .def(py::init<>())
        .def_readwrite("type", &SlotsGroupInfo::type)
        .def_readwrite("is_resizeable", &SlotsGroupInfo::isResizeable)
        .def_readwrite("count", &SlotsGroupInfo::count)
        .def_readwrite("names", &SlotsGroupInfo::names)
        .def_readwrite("categories", &SlotsGroupInfo::categories);

    py::class_<OperatorInfo>(m, "OperatorInfo")
        .def(py::init<>())
        .def_readwrite("op", &OperatorInfo::op)
        .def_readwrite("should_negate_output", &OperatorInfo::shouldNegateOutput);

    py::enum_<SlotCatergory>(m, "SlotCatergory")
        .value("NONE", SlotCatergory::none)
        .value("CLOCK", SlotCatergory::clock)
        .value("CLEAR", SlotCatergory::clear)
        .value("ENABLE", SlotCatergory::enable)
        .export_values();

    py::enum_<ComponentBehaviorType>(m, "ComponentBehaviorType")
        .value("NONE", ComponentBehaviorType::none)
        .value("INPUT", ComponentBehaviorType::input)
        .value("OUTPUT", ComponentBehaviorType::output)
        .export_values();
}
