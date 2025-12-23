#include <cstdio>
#include <iostream>
#include <pybind11/chrono.h>
#include <pybind11/functional.h>
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

#include "component_definition.h"
#include "internal_types.h"
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

#define DEF_PROPERTY_STR(prop_name, cpp_name)                 \
    def_property(                                             \
        prop_name,                                            \
        [](const ComponentDefinition &self) {                 \
            return self.cpp_name;                             \
        },                                                    \
        [](ComponentDefinition &self, const std::string &v) { \
            self.cpp_name = v;                                \
            self.invalidateHash();                            \
        })

#define DEF_PROPERTY_VEC_STR(prop_name, cpp_name)                          \
    def_property(                                                          \
        prop_name,                                                         \
        [](const ComponentDefinition &self) {                              \
            return self.cpp_name;                                          \
        },                                                                 \
        [](ComponentDefinition &self, const std::vector<std::string> &v) { \
            self.cpp_name = v;                                             \
            self.invalidateHash();                                         \
        })

#define DEF_PROPERTY_VEC_PIN_DETAIL(prop_name, cpp_name)                 \
    def_property(                                                        \
        prop_name,                                                       \
        [](const ComponentDefinition &self) {                            \
            return self.cpp_name;                                        \
        },                                                               \
        [](ComponentDefinition &self, const std::vector<PinDetail> &v) { \
            self.cpp_name = v;                                           \
            self.invalidateHash();                                       \
        })

#define DEF_PROPERTY_TIME(prop_name, cpp_name)       \
    def_property(                                    \
        prop_name,                                   \
        [](const ComponentDefinition &self) {        \
            return (long long)self.cpp_name.count(); \
        },                                           \
        [](ComponentDefinition &self, long long v) { \
            self.cpp_name = SimDelayNanoSeconds(v);  \
            self.invalidateHash();                   \
        })

#define DEF_PROPERTY_INT(prop_name, cpp_name)  \
    def_property(                              \
        prop_name,                             \
        [](const ComponentDefinition &self) {  \
            return self.cpp_name;              \
        },                                     \
        [](ComponentDefinition &self, int v) { \
            self.cpp_name = v;                 \
            self.invalidateHash();             \
        })

void bind_sim_engine_component_definition(py::module_ &m) {
    // py::class_<ComponentDefinition>(m, "ComponentDefinition")
    //     .def(py::init([]() {
    //              SimulationFunction noop = [](const std::vector<PinState> &,
    //                                           SimTime, const ComponentState &prev) {
    //                  return prev;
    //              };
    //              return ComponentDefinition("", "", 0, 0, noop,
    //                                         SimDelayNanoSeconds(0),
    //                                         std::vector<std::string>{});
    //          }),
    //          "Create an empty, inert component definition.")
    //     .def(py::init([](const std::string &name,
    //                      const std::string &category,
    //                      int input_count,
    //                      int output_count,
    //                      const py::function &sim_func,
    //                      long long delay_ns,
    //                      const std::string &op_str) {
    //              SimulationFunction fn = [sim_func](const std::vector<PinState> &inputs, SimTime t,
    //                                                 const ComponentState &prev) -> ComponentState {
    //                  py::gil_scoped_acquire gil;
    //                  try {
    //                      py::list py_inputs = to_py_list(inputs);
    //                      py::object py_prev = py::cast(prev);
    //                      py::object result = sim_func(py_inputs, t.count(), py_prev);
    //                      auto out = convertResultToComponentState(result, prev);
    //                      return out;
    //                  } catch (const std::exception &e) {
    //                      std::cerr << "[Bindings] simulate: exception (op ctor): \n"
    //                                << e.what() << std::endl;
    //                      throw;
    //                  }
    //              };
    //              const char op = op_str.empty() ? '0' : op_str[0];
    //
    //              return ComponentDefinition();
    //              // return ComponentDefinition(name, category,
    //              //                            input_count, output_count,
    //              //                            fn, SimDelayNanoSeconds(delay_ns), op);
    //          }),
    //          py::arg("name"), py::arg("category"), py::arg("input_count"), py::arg("output_count"),
    //          py::arg("simulate"), py::arg("delay_ns"), py::arg("op") = std::string(),
    //          "Create a definition using an operator to generate expressions.")
    //     .def(py::init([](const std::string &name,
    //                      const std::string &category,
    //                      int input_count,
    //                      int output_count,
    //                      const py::function &sim_func,
    //                      long long delay_ns,
    //                      const std::vector<std::string> &expressions) {
    //              SimulationFunction fn = [sim_func](const std::vector<PinState> &inputs, SimTime t,
    //                                                 const ComponentState &prev) -> ComponentState {
    //                  std::fflush(stderr);
    //                  py::gil_scoped_acquire gil;
    //                  try {
    //                      py::list py_inputs = to_py_list(inputs);
    //                      py::object py_prev = py::cast(prev);
    //                      py::object result = sim_func(py_inputs, static_cast<long long>(t.count()), py_prev);
    //                      auto out = convertResultToComponentState(result, prev);
    //                      return out;
    //                  } catch (const std::exception &e) {
    //                      std::cerr << "[Bindings] simulate: exception: \n"
    //                                << e.what() << std::endl;
    //                      throw;
    //                  }
    //              };
    //              return ComponentDefinition();
    //          }),
    //          py::arg("name"), py::arg("category"), py::arg("input_count"), py::arg("output_count"),
    //          py::arg("simulate"), py::arg("delay_ns"), py::arg("expressions") = std::vector<std::string>{},
    //          "Create a definition with explicit expressions.")
    //     // .def("get_expressions", &ComponentDefinition::getExpressions, py::arg("input_count") = -1)
    //     // .def("get_pin_details", [](const ComponentDefinition &self) {
    //     //     auto [inSpan, outSpan] = self.getPinDetails();
    //     //     std::vector<PinDetail> in(inSpan.begin(), inSpan.end());
    //     //     std::vector<PinDetail> out(outSpan.begin(), outSpan.end());
    //     //     return std::make_pair(in, out);
    //     // })
    //     .def("get_hash", &ComponentDefinition::getHash)
    //     .DEF_PROPERTY_STR("name", name)
    //     .DEF_PROPERTY_STR("category", category)
    //     .DEF_PROPERTY_TIME("delay_ns", delay)
    //     .DEF_PROPERTY_TIME("setup_time_ns", setupTime)
    //     .DEF_PROPERTY_TIME("hold_time_ns", holdTime)
    //     .DEF_PROPERTY_VEC_STR("expressions", expressions)
    //     .DEF_PROPERTY_VEC_PIN_DETAIL("input_pin_details", inputPinDetails)
    //     .DEF_PROPERTY_VEC_PIN_DETAIL("output_pin_details", outputPinDetails)
    //     .DEF_PROPERTY_INT("input_count", inputCount)
    //     .DEF_PROPERTY_INT("output_count", outputCount)
    //
    //     .def_property("op", [](const ComponentDefinition &self) { return self.op;
    // 		; }, [](ComponentDefinition &self, char c) {
    //     self.op = c;
    //     self.invalidateHash(); })
    //
    //     .def_property("negate", [](const ComponentDefinition &self) {
    // 		return self.negate;
    // 	; }, [](ComponentDefinition &self, bool v) {
    //     self.negate = v;
    //     self.invalidateHash(); })
    //
    //     .def("set_aux_pyobject", [](ComponentDefinition &self, const py::object &obj) {
    // 		self.auxData = std::any(Bess::Py::OwnedPyObject{obj});
    // 		; }, py::arg("obj"), "Attach a Python object as aux data. Returns the aux_data pointer value.")
    //
    //     .def("get_aux_pyobject", [](const ComponentDefinition &self) -> py::object {
    //     if (self.auxData.has_value() && self.auxData.type() == typeid(Bess::Py::OwnedPyObject)) {
    //         const auto &owned = std::any_cast<const Bess::Py::OwnedPyObject &>(self.auxData);
    //         return owned.object;
    //     }
    //     return py::none(); }, "Return the attached Python object if owned by Python, else None.")
    //
    //     .def_property("simulation_function", [](const ComponentDefinition &self) {
    //     SimulationFunction fn = self.simulationFunction;
    //     py::cpp_function wrapper([fn](const std::vector<PinState> &inputs, long long t_ns, const ComponentState &prev) {
    //         if (!fn)
    //             return prev;
    //         return fn(inputs, SimTime(t_ns), prev);
    //     });
    //     return py::function(wrapper); }, [](ComponentDefinition &self, py::object callable) {
    //     if (!PyCallable_Check(callable.ptr())) {
    //         throw py::type_error("simulation_function expects a callable");
    //     }
    //     self.simulationFunction = [callable](const std::vector<PinState> &inputs,
    // 		SimTime t, const ComponentState &prev) -> ComponentState {
    //         py::gil_scoped_acquire gil;
    //         py::list py_inputs = to_py_list(inputs);
    //         py::object py_prev = py::cast(prev);
    //         py::object result = callable(py_inputs, static_cast<long long>(t.count()), py_prev);
    //         return convertResultToComponentState(result, prev);
    //     };
    //     self.invalidateHash(); })
    //     .def("set_alt_input_counts", &ComponentDefinition::setAltInputCounts)
    //     .def("get_alt_input_counts", &ComponentDefinition::getAltInputCounts)
    //     .def("set_simulation_function", [](ComponentDefinition &self, py::object callable) {
    //     if (!PyCallable_Check(callable.ptr())) {
    //         throw py::type_error("set_simulation_function expects a callable");
    //     }
    //     self.simulationFunction = [callable](const std::vector<PinState> &inputs,
    // 		SimTime t, const ComponentState &prev) -> ComponentState {
    //         py::gil_scoped_acquire gil;
    //         py::list py_inputs = to_py_list(inputs);
    //         py::object py_prev = py::cast(prev);
    //         py::object result = callable(py_inputs, static_cast<long long>(t.count()), py_prev);
    //         return convertResultToComponentState(result, prev);
    //     };
    //     self.invalidateHash(); });
}
