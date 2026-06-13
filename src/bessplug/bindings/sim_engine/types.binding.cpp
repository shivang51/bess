#include "common/types.h"

#include <pybind11/functional.h>
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>
#include <pybind11/stl_bind.h>

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

    py::enum_<ConnectionState>(m, "ConnectionState")
        .value("DRIVEN", ConnectionState::driven)
        .value("HIGH_Z", ConnectionState::high_z)
        .value("UNKNOWN", ConnectionState::unknown)
        .export_values();

    py::class_<SlotState>(m, "SlotState")
        .def(py::init<>())
        .def(py::init<const SlotState &>())
        .def(py::init([](LogicState state) {
                 SlotState p;
                 p = state;
                 return p;
             }),
             py::arg("state"))
        .def(py::init([](LogicState state, long long last_change_time_ns) {
                 SlotState p;
                 p = state;
                 p.lastChangeTime = SimTime(last_change_time_ns);
                 return p;
             }),
             py::arg("state"),
             py::arg("last_change_time_ns"))
        .def_property(
            "state",
            [](SlotState &self) { return self.getLogicState(); },
            [](SlotState &self, LogicState state) { self = state; })
        .def_readwrite("voltage", &SlotState::voltage)
        .def_readwrite("conn_state", &SlotState::connState)
        .def_property(
            "last_change_time_ns",
            [](const SlotState &self) {
                return static_cast<long long>(self.lastChangeTime.count());
            },
            [](SlotState &self, long long ns) {
                self.lastChangeTime = SimTime(ns);
            })
        .def("copy", [](const SlotState &self) { return SlotState(self); })
        .def("invert",
             [](SlotState &self) {
                 switch (self.getLogicState()) {
                 case LogicState::low:
                     self = LogicState::high;
                     break;
                 case LogicState::high:
                     self = LogicState::low;
                     break;
                 case LogicState::unknown:
                 case LogicState::high_z:
                     // leave unchanged
                     break;
                 }
             })
        .def("__repr__", [](const SlotState &self) {
            const char *s = "UNKNOWN";
            switch (self.getLogicState()) {
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
            return std::string("<PinState state=") + s +
                   ", t_ns=" + std::to_string(self.lastChangeTime.count()) +
                   ">";
        });

    py::enum_<SlotType>(m, "PinType")
        .value("INPUT", SlotType::digitalInput)
        .value("OUTPUT", SlotType::digitalOutput)
        .export_values();

    py::enum_<SlotsGroupType>(m, "SlotsGroupType")
        .value("NONE", SlotsGroupType::none)
        .value("INPUT", SlotsGroupType::input)
        .value("OUTPUT", SlotsGroupType::output)
        .export_values();

    py::enum_<SlotCatergory>(m, "SlotCategory")
        .value("NONE", SlotCatergory::none)
        .value("CLOCK", SlotCatergory::clock)
        .value("CLEAR", SlotCatergory::clear)
        .value("ENABLE", SlotCatergory::enable)
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
        .def_readwrite("should_negate_output",
                       &OperatorInfo::shouldNegateOutput);

    py::enum_<ComponentBehaviorType>(m, "ComponentBehaviorType")
        .value("NONE", ComponentBehaviorType::none)
        .value("INPUT", ComponentBehaviorType::input)
        .value("OUTPUT", ComponentBehaviorType::output)
        .export_values();
}
