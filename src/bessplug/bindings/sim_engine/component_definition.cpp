#include <cstdio>
#include <pybind11/functional.h>
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

#include "component_definition.h"
#include "types.h"

namespace py = pybind11;

using namespace Bess::SimEngine;

namespace {
    static ComponentState convertResultToComponentState(const py::object &result, const ComponentState &prev) {
        if (result.is_none()) {
            std::fprintf(stderr, "[Bindings] simulate: got None, returning prev\n");
            std::fflush(stderr);
            return prev;
        }
        if (py::isinstance<ComponentState>(result)) {
            std::fprintf(stderr, "[Bindings] simulate: got native ComponentState\n");
            std::fflush(stderr);
            return result.cast<ComponentState>();
        }
        if (py::hasattr(result, "_native")) {
            py::object n = result.attr("_native");
            if (py::isinstance<ComponentState>(n)) {
                std::fprintf(stderr, "[Bindings] simulate: got wrapper with _native ComponentState\n");
                std::fflush(stderr);
                return n.cast<ComponentState>();
            }
        }
        std::fprintf(stderr, "[Bindings] simulate: invalid return type\n");
        std::fflush(stderr);
        throw py::type_error("Simulation function must return ComponentState (native), a wrapper with _native, or None");
    }

    static py::list to_py_list(const std::vector<PinState> &inputs) {
        py::list lst;
        for (const auto &p : inputs) {
            lst.append(py::cast(p));
        }
        return lst;
    }
} // namespace

void bind_sim_engine_component_definition(py::module_ &m) {
    py::class_<ComponentDefinition>(m, "ComponentDefinition")
        .def(py::init([]() {
                 SimulationFunction noop = [](const std::vector<PinState> & /*inputs*/, SimTime /*t*/, const ComponentState &prev) {
                     return prev;
                 };
                 return ComponentDefinition("", "", 0, 0, noop, SimDelayNanoSeconds(0), std::vector<std::string>{});
             }),
             "Create an empty, inert component definition.")
        .def(py::init([](const std::string &name,
                         const std::string &category,
                         int input_count,
                         int output_count,
                         const py::function &sim_func,
                         long long delay_ns,
                         const std::string &op_str) {
                 SimulationFunction fn = [sim_func](const std::vector<PinState> &inputs, SimTime t, const ComponentState &prev) -> ComponentState {
                     std::fprintf(stderr, "[Bindings] simulate: entering (op ctor)\n");
                     std::fflush(stderr);
                     py::gil_scoped_acquire gil;
                     std::fprintf(stderr, "[Bindings] simulate: GIL acquired, calling python (op ctor)\n");
                     std::fflush(stderr);
                     try {
                         py::list py_inputs = to_py_list(inputs);
                         py::object py_prev = py::cast(prev);
                         py::object result = sim_func(py_inputs, static_cast<long long>(t.count()), py_prev);
                         std::fprintf(stderr, "[Bindings] simulate: python returned, converting\n");
                         std::fflush(stderr);
                         auto out = convertResultToComponentState(result, prev);
                         std::fprintf(stderr, "[Bindings] simulate: converted, leaving\n");
                         std::fflush(stderr);
                         return out;
                     } catch (const std::exception &e) {
                         std::fprintf(stderr, "[Bindings] simulate: exception: %s\n", e.what());
                         std::fflush(stderr);
                         throw;
                     }
                 };
                 const char op = op_str.empty() ? '0' : op_str[0];
                 return ComponentDefinition(name, category, input_count, output_count, fn, SimDelayNanoSeconds(delay_ns), op);
             }),
             py::arg("name"), py::arg("category"), py::arg("input_count"), py::arg("output_count"),
             py::arg("simulate"), py::arg("delay_ns"), py::arg("op") = std::string(),
             "Create a definition using an operator to generate expressions.")
        .def(py::init([](const std::string &name,
                         const std::string &category,
                         int input_count,
                         int output_count,
                         const py::function &sim_func,
                         long long delay_ns,
                         const std::vector<std::string> &expressions) {
                 SimulationFunction fn = [sim_func](const std::vector<PinState> &inputs, SimTime t, const ComponentState &prev) -> ComponentState {
                     std::fprintf(stderr, "[Bindings] simulate: entering (expr ctor)\n");
                     std::fflush(stderr);
                     py::gil_scoped_acquire gil;
                     std::fprintf(stderr, "[Bindings] simulate: GIL acquired, calling python (expr ctor)\n");
                     std::fflush(stderr);
                     try {
                         py::list py_inputs = to_py_list(inputs);
                         py::object py_prev = py::cast(prev);
                         py::object result = sim_func(py_inputs, static_cast<long long>(t.count()), py_prev);
                         std::fprintf(stderr, "[Bindings] simulate: python returned, converting\n");
                         std::fflush(stderr);
                         auto out = convertResultToComponentState(result, prev);
                         std::fprintf(stderr, "[Bindings] simulate: converted, leaving\n");
                         std::fflush(stderr);
                         return out;
                     } catch (const std::exception &e) {
                         std::fprintf(stderr, "[Bindings] simulate: exception: %s\n", e.what());
                         std::fflush(stderr);
                         throw;
                     }
                 };
                 return ComponentDefinition(name, category, input_count, output_count, fn, SimDelayNanoSeconds(delay_ns), expressions);
             }),
             py::arg("name"), py::arg("category"), py::arg("input_count"), py::arg("output_count"),
             py::arg("simulate"), py::arg("delay_ns"), py::arg("expressions") = std::vector<std::string>{},
             "Create a definition with explicit expressions.")
        .def("get_expressions", &ComponentDefinition::getExpressions, py::arg("input_count") = -1)
        .def("get_pin_details", [](const ComponentDefinition &self) {
            auto [inSpan, outSpan] = self.getPinDetails();
            std::vector<PinDetail> in(inSpan.begin(), inSpan.end());
            std::vector<PinDetail> out(outSpan.begin(), outSpan.end());
            return std::make_pair(in, out);
        })
        .def("get_hash", &ComponentDefinition::getHash)

        .def_property("name", [](const ComponentDefinition &self) { return self.name; }, [](ComponentDefinition &self, const std::string &v) { self.name = v; self.invalidateHash(); })

        .def_property("category", [](const ComponentDefinition &self) { return self.category; }, [](ComponentDefinition &self, const std::string &v) { self.category = v; self.invalidateHash(); })

        .def_property("delay_ns", [](const ComponentDefinition &self) { return static_cast<long long>(self.delay.count()); }, [](ComponentDefinition &self, long long ns) { self.delay = SimDelayNanoSeconds(ns); self.invalidateHash(); })

        .def_property("setup_time_ns", [](const ComponentDefinition &self) { return static_cast<long long>(self.setupTime.count()); }, [](ComponentDefinition &self, long long ns) { self.setupTime = SimDelayNanoSeconds(ns); self.invalidateHash(); })

        .def_property("hold_time_ns", [](const ComponentDefinition &self) { return static_cast<long long>(self.holdTime.count()); }, [](ComponentDefinition &self, long long ns) { self.holdTime = SimDelayNanoSeconds(ns); self.invalidateHash(); })

        .def_property("expressions", [](const ComponentDefinition &self) { return self.expressions; }, [](ComponentDefinition &self, const std::vector<std::string> &expr) { self.expressions = expr; self.invalidateHash(); })

        .def_property("input_pin_details", [](const ComponentDefinition &self) { return self.inputPinDetails; }, [](ComponentDefinition &self, const std::vector<PinDetail> &pins) { 
						self.inputPinDetails = pins; 
						self.invalidateHash(); })

        .def_property("output_pin_details", [](const ComponentDefinition &self) { return self.outputPinDetails; }, [](ComponentDefinition &self, const std::vector<PinDetail> &pins) { self.outputPinDetails = pins; self.invalidateHash(); })

        .def_property("input_count", [](const ComponentDefinition &self) { return self.inputCount; }, [](ComponentDefinition &self, int v) { self.inputCount = v; self.invalidateHash(); })
        .def_property("output_count", [](const ComponentDefinition &self) { return self.outputCount; }, [](ComponentDefinition &self, int v) { self.outputCount = v; self.invalidateHash(); })
        .def_property("op", [](const ComponentDefinition &self) { return self.op; }, [](ComponentDefinition &self, char c) { self.op = c; self.invalidateHash(); })

        .def_property("negate", [](const ComponentDefinition &self) { return self.negate; }, [](ComponentDefinition &self, bool v) { self.negate = v; self.invalidateHash(); })

        .def_property("aux_data", [](ComponentDefinition &self) -> py::object {
                if (self.auxData.has_value() && self.auxData.type() == typeid(py::object)) {
                    return std::any_cast<py::object>(self.auxData);
                }
                return py::none(); }, [](ComponentDefinition &self, py::object obj) { self.auxData = obj; })
        .def_property("simulation_function", [](const ComponentDefinition &self) {
                SimulationFunction fn = self.simulationFunction;
                py::cpp_function wrapper([fn](const std::vector<PinState> &inputs, long long t_ns, const ComponentState &prev) {
                    if (!fn) return prev;
                    return fn(inputs, SimTime(t_ns), prev);
                });
                return py::function(wrapper); }, [](ComponentDefinition &self, py::object callable) {
                if (!PyCallable_Check(callable.ptr())) {
                    throw py::type_error("simulation_function expects a callable");
                }
                self.simulationFunction = [callable](const std::vector<PinState> &inputs, SimTime t, const ComponentState &prev) -> ComponentState {
                    std::fprintf(stderr, "[Bindings] simulate: entering (prop set)\n");
                    std::fflush(stderr);
                    py::gil_scoped_acquire gil;
                    std::fprintf(stderr, "[Bindings] simulate: GIL acquired, calling python (prop set)\n");
                    std::fflush(stderr);
                    py::list py_inputs = to_py_list(inputs);
                    py::object py_prev = py::cast(prev);
                    py::object result = callable(py_inputs, static_cast<long long>(t.count()), py_prev);
                    std::fprintf(stderr, "[Bindings] simulate: python returned, converting (prop set)\n");
                    std::fflush(stderr);
                    return convertResultToComponentState(result, prev);
                };
                self.invalidateHash(); })
        .def("set_simulation_function", [](ComponentDefinition &self, py::object callable) {
            if (!PyCallable_Check(callable.ptr())) {
                throw py::type_error("set_simulation_function expects a callable");
            }
            self.simulationFunction = [callable](const std::vector<PinState> &inputs, SimTime t, const ComponentState &prev) -> ComponentState {
                std::fprintf(stderr, "[Bindings] simulate: entering (method)\n");
                std::fflush(stderr);
                py::gil_scoped_acquire gil;
                std::fprintf(stderr, "[Bindings] simulate: GIL acquired, calling python (method)\n");
                std::fflush(stderr);
                py::list py_inputs = to_py_list(inputs);
                py::object py_prev = py::cast(prev);
                py::object result = callable(py_inputs, static_cast<long long>(t.count()), py_prev);
                std::fprintf(stderr, "[Bindings] simulate: python returned, converting (method)\n");
                std::fflush(stderr);
                return convertResultToComponentState(result, prev);
            };
            self.invalidateHash(); });
}
