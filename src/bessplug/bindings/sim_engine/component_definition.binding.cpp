#include <pybind11/chrono.h>
#include <pybind11/functional.h>
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>
#define PYBIND11_DEBUG

#include "component_definition.h"
#include "internal_types.h"
#include "types.h"

#include <iostream>
#include <pystate.h>

namespace py = pybind11;

using namespace Bess::SimEngine;

static ComponentState convertResultToComponentState(const py::object &result,
                                                    const ComponentState &prev);

class PyComponentDefinition : public ComponentDefinition,
                              public py::trampoline_self_life_support {
  public:
    using ComponentDefinition::ComponentDefinition;

    PyComponentDefinition() {
        m_ownership = CompDefinitionOwnership::Python;
    }

    ~PyComponentDefinition() override {
        py::gil_scoped_acquire gil;
        m_auxData.reset();
        m_simulationFunction = nullptr;
    }

    std::shared_ptr<ComponentDefinition> clone() const override {
        PYBIND11_OVERRIDE(
            std::shared_ptr<ComponentDefinition>,
            ComponentDefinition,
            clone);
    }

    void setAuxData(const std::any &data) override {
        py::gil_scoped_acquire gil;
        ComponentDefinition::setAuxData(data);
    }

    void onExpressionsChange() override {
        py::gil_scoped_acquire gil;
        ComponentDefinition::onExpressionsChange();
    }

    SimulationFunction &getSimulationFunction() override {
        py::gil_scoped_acquire gil;
        return ComponentDefinition::getSimulationFunction();
    }

    const SimulationFunction &getSimulationFunction() const override {
        py::gil_scoped_acquire gil;
        return ComponentDefinition::getSimulationFunction();
    }

    SimulationFunction getSimFunctionCopy() const override {
        py::gil_scoped_acquire gil;
        return ComponentDefinition::getSimFunctionCopy();
    }

    SimTime getRescheduleTime(SimTime currentTime) const override {
        PYBIND11_OVERRIDE_NAME(
            SimTime,
            ComponentDefinition,
            "get_reschedule_time", // very important to match the Python name
            getRescheduleTime,
            currentTime // in nano seconds
        );
    }

    void onStateChange(const ComponentState &oldState,
                       const ComponentState &newState) override {
        PYBIND11_OVERRIDE_NAME(
            void,
            ComponentDefinition,
            "on_state_change",
            onStateChange,
            oldState,
            newState);
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
            [fn](const std::vector<SlotState> &inputs,
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
        auto fn = [callable](const std::vector<SlotState> &inputs,
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
               py::smart_holder>(m, "ComponentDefinition")
        .def(py::init<>(), "Create an empty, inert component definition.")
        .def("get_hash", &ComponentDefinition::getHash)
        .def("clone", &ComponentDefinition::clone)
        .def("compute_hash", &ComponentDefinition::computeHash)
        .def("get_reschedule_time", &ComponentDefinition::getRescheduleTime,
             py::arg("current_time_ns"),
             "Get the next reschedule time given the current time in nanoseconds.")
        .def("on_state_change", &ComponentDefinition::onStateChange,
             py::arg("old_state"), py::arg("new_state"),
             "Callback invoked when the component's state changes.")
        .DEF_PROP_STR_GSET("name", Name)
        .DEF_PROP_STR_GSET("group_name", GroupName)
        .DEF_PROP_GSET_T(bool, "should_auto_reschedule", ShouldAutoReschedule)
        .DEF_PROP_GSET_T(ComponentBehaviorType, "behavior_type", BehaviorType)
        .DEF_PROP_GSET_T(SlotsGroupInfo, "input_slots_info", InputSlotsInfo)
        .DEF_PROP_GSET_T(SlotsGroupInfo, "output_slots_info", OutputSlotsInfo)
        .DEF_PROP_GSET_T(SimDelayNanoSeconds, "sim_delay", SimDelay)
        .DEF_PROP_GSET_T(OperatorInfo, "op_info", OpInfo)
        .DEF_PROP_GSET_T(CompDefIOGrowthPolicy, "io_growth_policy", IOGrowthPolicy)
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
