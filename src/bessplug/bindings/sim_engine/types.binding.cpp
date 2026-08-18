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

    py::enum_<PortDirection>(m, "PortDirection")
        .value("NONE", PortDirection::none)
        .value("INPUT", PortDirection::input)
        .value("OUTPUT", PortDirection::output)
        .export_values();

    py::enum_<SignalKind>(m, "SignalKind")
        .value("NONE", SignalKind::none)
        .value("DIGITAL", SignalKind::digital)
        .value("SCALAR", SignalKind::scalar)
        .value("VECTOR", SignalKind::vector)
        .value("STRING", SignalKind::string)
        .export_values();

    py::enum_<QuantityKind>(m, "QuantityKind")
        .value("NONE", QuantityKind::none)
        .value("LOGIC", QuantityKind::logic)
        .value("DIMENSIONLESS", QuantityKind::dimensionless)
        .value("VOLTAGE", QuantityKind::voltage)
        .value("CURRENT", QuantityKind::current)
        .value("RESISTANCE", QuantityKind::resistance)
        .value("CONDUCTANCE", QuantityKind::conductance)
        .value("POWER", QuantityKind::power)
        .value("FREQUENCY", QuantityKind::frequency)
        .value("ANGLE", QuantityKind::angle)
        .value("TIME", QuantityKind::time)
        .value("TEMPERATURE", QuantityKind::temperature)
        .export_values();

    py::class_<PortState>(m, "PortState")
        .def(py::init<>())
        .def(py::init<const PortState &>())
        .def(py::init([](double scalar_value) {
                 return PortState::scalar(scalar_value);
             }),
             py::arg("scalar_value"))
        .def(py::init([](LogicState state) {
                 PortState p;
                 p = state;
                 return p;
             }),
             py::arg("state"))
        .def(py::init([](LogicState state, long long last_change_time_ns) {
                 PortState p;
                 p = state;
                 p.lastChangeTime = SimTime(last_change_time_ns);
                 return p;
             }),
             py::arg("state"),
             py::arg("last_change_time_ns"))
        .def_property(
            "state",
            [](PortState &self) { return self.getLogicState(); },
            [](PortState &self, LogicState state) { self = state; })
        .def_readwrite("signal_kind", &PortState::signalKind)
        .def_readwrite("scalar_value", &PortState::scalarValue)
        .def_readwrite("vector_value", &PortState::vectorValue)
        .def_readwrite("string_value", &PortState::stringValue)
        .def_readwrite("conn_state", &PortState::connState)
        .def_property(
            "last_change_time_ns",
            [](const PortState &self) {
                return static_cast<long long>(self.lastChangeTime.count());
            },
            [](PortState &self, long long ns) {
                self.lastChangeTime = SimTime(ns);
            })
        .def("copy", [](const PortState &self) { return PortState(self); })
        .def_static(
            "scalar",
            [](double value, long long last_change_time_ns) {
                return PortState::scalar(value, SimTime(last_change_time_ns));
            },
            py::arg("value"),
            py::arg("last_change_time_ns") = 0)
        .def_static(
            "digital",
            [](LogicState value, long long last_change_time_ns) {
                return PortState::digital(value, SimTime(last_change_time_ns));
            },
            py::arg("value"),
            py::arg("last_change_time_ns") = 0)
        .def_static(
            "vector",
            [](std::vector<double> value, long long last_change_time_ns) {
                return PortState::vector(std::move(value),
                                         SimTime(last_change_time_ns));
            },
            py::arg("value"),
            py::arg("last_change_time_ns") = 0)
        .def_static(
            "string",
            [](std::string value, long long last_change_time_ns) {
                return PortState::string(value, SimTime(last_change_time_ns));
            },
            py::arg("value"),
            py::arg("last_change_time_ns") = 0)
        .def("is_digital", &PortState::isDigital)
        .def("is_scalar", &PortState::isScalar)
        .def("is_vector", &PortState::isVector)
        .def("is_string", &PortState::isString)
        .def(
            "set_scalar_value",
            [](PortState &self, double value, long long last_change_time_ns) {
                self.setScalarValue(value, SimTime(last_change_time_ns));
            },
            py::arg("value"),
            py::arg("last_change_time_ns") = 0)
        .def(
            "set_vector_value",
            [](PortState &self,
               std::vector<double> value,
               long long last_change_time_ns) {
                self.setVectorValue(std::move(value),
                                    SimTime(last_change_time_ns));
            },
            py::arg("value"),
            py::arg("last_change_time_ns") = 0)
        .def(
            "set_string_value",
            [](PortState &self,
               std::string value,
               long long last_change_time_ns) {
                self.setStringValue(std::move(value),
                                    SimTime(last_change_time_ns));
            },
            py::arg("value"),
            py::arg("last_change_time_ns") = 0)
        .def("get_digital_voltage_value", &PortState::getDigitalVoltageValue)
        .def("get_numeric_value", &PortState::getNumericValue)
        .def("invert",
             [](PortState &self) {
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
        .def("__repr__", [](const PortState &self) {
            if (self.signalKind == SignalKind::scalar) {
                return std::string("<PortState scalar=") +
                       std::to_string(self.scalarValue) +
                       ", t_ns=" + std::to_string(self.lastChangeTime.count()) +
                       ">";
            }

            if (self.signalKind == SignalKind::vector) {
                return std::string("<PortState vector_size=") +
                       std::to_string(self.vectorValue.size()) +
                       ", t_ns=" + std::to_string(self.lastChangeTime.count()) +
                       ">";
            }

            if (self.signalKind == SignalKind::string) {
                return std::string("<PortState string='") + self.stringValue +
                       "', t_ns=" +
                       std::to_string(self.lastChangeTime.count()) + ">";
            }

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
            return std::string("<PortState state=") + s +
                   ", t_ns=" + std::to_string(self.lastChangeTime.count()) +
                   ">";
        });

    py::class_<PortRef>(m, "PortRef")
        .def(py::init<>())
        .def_readwrite("component_id", &PortRef::componentId)
        .def_readwrite("direction", &PortRef::direction)
        .def_readwrite("signal_kind", &PortRef::signalKind)
        .def_readwrite("index", &PortRef::index)
        .def("is_valid", &PortRef::isValid)
        .def("is_input", &PortRef::isInput)
        .def("is_output", &PortRef::isOutput);

    py::class_<PortSpec>(m, "PortSpec")
        .def(py::init<>())
        .def_readwrite("name", &PortSpec::name)
        .def_readwrite("signal_kind", &PortSpec::signalKind)
        .def_readwrite("quantity_kind", &PortSpec::quantityKind)
        .def_readwrite("unit", &PortSpec::unit)
        .def_readwrite("default_state", &PortSpec::defaultState);

    py::class_<PortDescriptor>(m, "PortDescriptor")
        .def(py::init<>())
        .def_readwrite("direction", &PortDescriptor::direction)
        .def_readwrite("signal_kind", &PortDescriptor::signalKind)
        .def_readwrite("quantity_kind", &PortDescriptor::quantityKind)
        .def_readwrite("unit", &PortDescriptor::unit)
        .def_readwrite("count", &PortDescriptor::count)
        .def_readwrite("names", &PortDescriptor::names)
        .def_readwrite("is_resizeable", &PortDescriptor::isResizeable)
        .def_readwrite("default_states", &PortDescriptor::defaultStates)
        .def_readwrite("ports", &PortDescriptor::ports)
        .def_readwrite("resize_spec", &PortDescriptor::resizeSpec)
        .def_property_readonly("port_count", &PortDescriptor::portCount)
        .def("port_spec", &PortDescriptor::portSpec)
        .def("signal_kind_at", &PortDescriptor::signalKindAt)
        .def("name_at", &PortDescriptor::nameAt);

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
