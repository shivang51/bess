#include <cstdint>
#include <iostream>
#include <pybind11/functional.h>
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>
#include <typeinfo>

#include "types.h"

namespace py = pybind11;

using namespace Bess::SimEngine;

namespace {
    struct OwnedPyObject {
        py::object object;
    };

    struct PySimulationFunctionWrapper {
        SimulationFunction fn;

        PySimulationFunctionWrapper() = default;

        explicit PySimulationFunctionWrapper(py::function callable) {
            set(std::move(callable));
        }

        void set(py::function callable) {
            py::function f = std::move(callable);
            fn = [f](const std::vector<PinState> &inputs, SimTime t, const ComponentState &prev) -> ComponentState {
                py::gil_scoped_acquire gil;
                std::cout << "[Bindings][PySimulationFunctionWrapper][set] simulate: GIL acquired, calling python (wrapper set)\n";
                py::object result = f(inputs, static_cast<long long>(t.count()), prev);
                return result.cast<ComponentState>();
            };
        }

        ComponentState call(const std::vector<PinState> &inputs, long long t_ns, const ComponentState &prev) const {
            if (!fn) {
                return prev;
            }
            return fn(inputs, SimTime(t_ns), prev);
        }

        bool valid() const noexcept { return static_cast<bool>(fn); }
    };
} // namespace

void bind_sim_engine_types(py::module_ &m) {
    py::enum_<LogicState>(m, "LogicState")
        .value("LOW", LogicState::low)
        .value("HIGH", LogicState::high)
        .value("UNKNOWN", LogicState::unknown)
        .value("HIGH_Z", LogicState::high_z)
        .export_values();

    py::class_<PinState>(m, "PinState")
        .def(py::init<>())
        .def(py::init<const PinState &>())
        .def(py::init<bool>(), py::arg("value"))
        .def(py::init([](LogicState state, long long last_change_time_ns) {
                 PinState p;
                 p.state = state;
                 p.lastChangeTime = SimTime(last_change_time_ns);
                 return p;
             }),
             py::arg("state"), py::arg("last_change_time_ns"))
        .def_readwrite("state", &PinState::state)
        .def_property(
            "last_change_time_ns",
            [](const PinState &self) { return static_cast<long long>(self.lastChangeTime.count()); },
            [](PinState &self, long long ns) { self.lastChangeTime = SimTime(ns); })
        .def("copy", [](const PinState &self) { return PinState(self); })
        .def("__repr__", [](const PinState &self) {
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
        .def_property(
            "input_states",
            [](ComponentState &self) -> std::vector<PinState> & { return self.inputStates; },
            [](ComponentState &self, const std::vector<PinState> &v) { self.inputStates = v; },
            py::return_value_policy::reference_internal)
        .def_property(
            "output_states",
            [](ComponentState &self) -> std::vector<PinState> & {
                return self.outputStates;
            },
            [](ComponentState &self, const std::vector<PinState> &v) {
                self.outputStates = v;
            },
            py::return_value_policy::reference_internal)
        .def("set_output_state", [](ComponentState &self, std::size_t idx, const PinState &value) {
                if (idx >= self.outputStates.size()) {
                    throw py::index_error("output index out of range");
                }
                self.outputStates[idx] = value;
            }, py::arg("idx"), py::arg("value"))
        .def_readwrite("is_changed", &ComponentState::isChanged)
        .def_property(
            "input_connected",
            [](const ComponentState &self) { return self.inputConnected; },
            [](ComponentState &self, const std::vector<bool> &v) { self.inputConnected = v; })
        .def_property(
            "output_connected",
            [](const ComponentState &self) { return self.outputConnected; },
            [](ComponentState &self, const std::vector<bool> &v) { self.outputConnected = v; })
        .def_property("aux_data_ptr", [](const ComponentState &self) { return static_cast<std::uintptr_t>(reinterpret_cast<std::uintptr_t>(self.auxData)); }, [](ComponentState &self, std::uintptr_t ptr_value) { self.auxData = reinterpret_cast<std::any *>(ptr_value); })
        .def("set_aux_pyobject", [](ComponentState &self, py::object obj) {
                if (self.auxData && self.auxData->type() == typeid(OwnedPyObject)) {
                    delete self.auxData;
                    self.auxData = nullptr;
                }
                self.auxData = new std::any(OwnedPyObject{obj});
                return static_cast<std::uintptr_t>(reinterpret_cast<std::uintptr_t>(self.auxData)); }, py::arg("obj"), "Attach a Python object as aux data. Returns the aux_data pointer value.")
        .def("get_aux_pyobject", [](const ComponentState &self) -> py::object {
                if (self.auxData && self.auxData->type() == typeid(OwnedPyObject)) {
                    const auto &owned = std::any_cast<const OwnedPyObject &>(*self.auxData);
                    return owned.object;
                }
                return py::none(); }, "Return the attached Python object if owned by Python, else None.")
        .def("clear_aux_data", [](ComponentState &self) {
                if (self.auxData && self.auxData->type() == typeid(OwnedPyObject)) {
                    delete self.auxData;
                    self.auxData = nullptr;
                } }, "Clear aux_data if it was set via set_aux_pyobject.");

    // SimulationFunction wrapper exposed to Python
    py::class_<PySimulationFunctionWrapper>(m, "SimulationFunction")
        .def(py::init<>())
        .def(py::init<py::function>(), py::arg("callable"), "Wrap a Python callable as a SimulationFunction.")
        .def("set", &PySimulationFunctionWrapper::set, py::arg("callable"), "Assign/replace the underlying callable.")
        .def("__call__", &PySimulationFunctionWrapper::call, py::arg("inputs"), py::arg("t_ns"), py::arg("prev"), "Invoke the simulation function.")
        .def_property_readonly("valid", &PySimulationFunctionWrapper::valid, "Whether a callable is set.");

    py::enum_<PinType>(m, "PinType")
        .value("INPUT", PinType::input)
        .value("OUTPUT", PinType::output)
        .export_values();

    py::enum_<ExtendedPinType>(m, "ExtendedPinType")
        .value("NONE", ExtendedPinType::none)
        .value("INPUT_CLOCK", ExtendedPinType::inputClock)
        .value("INPUT_CLEAR", ExtendedPinType::inputClear)
        .export_values();

    py::class_<PinDetails>(m, "PinDetails")
        .def(py::init<>())
        .def_readwrite("type", &PinDetails::type)
        .def_readwrite("name", &PinDetails::name)
        .def_readwrite("extended_type", &PinDetails::extendedType);
}
