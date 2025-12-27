#include <pybind11/chrono.h>
#include <pybind11/functional.h>
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

#include "component_definition.h"
#include "internal_types.h"
#include "types.h"

#include <iostream>
#include <print>

namespace py = pybind11;

using namespace Bess::SimEngine;

static ComponentState convertResultToComponentState(const py::object &result,
                                                    const ComponentState &prev);

struct PyComponentDefinition : public ComponentDefinition {
    using ComponentDefinition::ComponentDefinition;

    PyComponentDefinition() {
        m_ownership = CompDefinitionOwnership::Python;
    }

    std::shared_ptr<ComponentDefinition> clone() const override {
        py::gil_scoped_acquire gil;
        return std::static_pointer_cast<PyComponentDefinition>(
            ComponentDefinition::clone());
    }

    std::shared_ptr<ComponentDefinition> cloneViaPythonImpl() const override {
        PYBIND11_OVERRIDE_PURE(
            std::shared_ptr<ComponentDefinition>,
            ComponentDefinition,
            cloneViaPythonImpl);
    }
};

template <typename T>
static py::list toPyList(const std::vector<T> &inputs);

#define DEF_PROP_GSET_T(type, prop_name, cpp_name)     \
    def_property(                                      \
        prop_name,                                     \
        [](const ComponentDefinition &self) {          \
            return self.get##cpp_name();               \
        },                                             \
        [](ComponentDefinition &self, const type &v) { \
            self.set##cpp_name(v);                     \
        })

#define DEF_PROP_STR_GSET(prop_name, cpp_name)                \
    def_property(                                             \
        prop_name,                                            \
        [](const ComponentDefinition &self) {                 \
            return self.get##cpp_name();                      \
        },                                                    \
        [](ComponentDefinition &self, const std::string &v) { \
            self.set##cpp_name(v);                            \
        })

void bind_sim_engine_component_definition(py::module_ &m) {
    auto setAuxData = [](ComponentDefinition &self, const py::object &obj) {
        self.setAuxData(std::any(Bess::Py::OwnedPyObject{obj}));
    };

    auto getAuxData = [](const ComponentDefinition &self) -> py::object {
        if (self.getAuxData().has_value() &&
            self.getAuxData().type() == typeid(Bess::Py::OwnedPyObject)) {
            const auto &owned = std::any_cast<const Bess::Py::OwnedPyObject &>(
                self.getAuxData());
            return owned.object;
        }
        return py::none();
    };

    auto getSimFn = [](const ComponentDefinition &self) -> py::object {
        SimulationFunction fn = self.getSimulationFunction();
        if (!fn)
            return py::none();

        return py::cpp_function(
            [fn](const std::vector<PinState> &inputs,
                 long long t_ns,
                 const ComponentState &prev) {
                py::gil_scoped_acquire gil;
                return fn(inputs, SimTime(t_ns), prev);
            });
    };

    auto setSimFn = [](ComponentDefinition &self, py::object callable) {
        if (!PyCallable_Check(callable.ptr())) {
            throw py::type_error("simulation_function expects a callable");
        }
        auto fn = [callable](const std::vector<PinState> &inputs,
                             SimTime t,
                             const ComponentState &prev) -> ComponentState {
            py::gil_scoped_acquire gil;
            py::list py_inputs = toPyList(inputs);
            py::object py_prev = py::cast(prev);
            py::object result = callable(py_inputs,
                                         static_cast<long long>(t.count()),
                                         py_prev);
            return convertResultToComponentState(result, prev);
        };
        self.setSimulationFunction(fn);
    };

    py::class_<ComponentDefinition,
               PyComponentDefinition,
               std::shared_ptr<ComponentDefinition>>(m, "ComponentDefinition")
        .def(py::init([]() {
                 return std::make_shared<PyComponentDefinition>();
             }),
             "Create an empty, inert component definition.")
        .def("get_hash", &ComponentDefinition::getHash)
        .def("clone", &ComponentDefinition::clone)
        .DEF_PROP_STR_GSET("name", Name)
        .DEF_PROP_STR_GSET("group_name", GroupName)
        .DEF_PROP_GSET_T(bool, "should_auto_reschedule", ShouldAutoReschedule)
        .DEF_PROP_GSET_T(ComponentBehaviorType, "behavior_type", BehaviorType)
        .DEF_PROP_GSET_T(SlotsGroupInfo, "input_slots_info", InputSlotsInfo)
        .DEF_PROP_GSET_T(SlotsGroupInfo, "output_slots_info", OutputSlotsInfo)
        .DEF_PROP_GSET_T(SimDelayNanoSeconds, "sim_delay", SimDelay)
        .DEF_PROP_GSET_T(OperatorInfo, "op_info", OpInfo)
        .DEF_PROP_GSET_T(std::vector<std::string>, "output_expressions", OutputExpressions)
        .def_property("aux_data", getAuxData, setAuxData, "Get Set Aux Data as a Python object.")
        .def_property("simulation_function", getSimFn, setSimFn, "Get or set the simulation function as a Python callable.");
}

template <typename T>
static py::list toPyList(const std::vector<T> &inputs) {
    py::list lst;
    for (const auto &p : inputs) {
        lst.append(py::cast(p));
    }
    return lst;
}

static ComponentState convertResultToComponentState(const py::object &result,
                                                    const ComponentState &prev) {
    if (result.is_none()) {
        std::cerr << "[Bindings] simulate: got None, returning prev\n";
        std::flush(std::cerr);
        return prev;
    }
    if (py::isinstance<ComponentState>(result)) {
        std::cerr << "[Bindings] simulate: got native ComponentState\n";
        std::flush(std::cerr);
        return result.cast<ComponentState>();
    }
    if (py::hasattr(result, "_native")) {
        py::object n = result.attr("_native");
        if (py::isinstance<ComponentState>(n)) {
            std::cerr << "[Bindings] simulate: got wrapper with _native ComponentState\n";
            std::flush(std::cerr);
            return n.cast<ComponentState>();
        }
    }
    std::cerr << "[Bindings] simulate: invalid return type\n";
    std::flush(std::cerr);
    throw py::type_error("Simulation function must return ComponentState (native), a wrapper with _native, or None");
}
