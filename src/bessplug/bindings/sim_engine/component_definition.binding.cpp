#include "expression_evalutator/expr_evaluator.h"
#include <pybind11/functional.h>
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

#ifdef DEBUG
    #define PYBIND11_DEBUG
#endif

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
        py::gil_scoped_acquire gil;
        auto ret = std::make_shared<PyComponentDefinition>();
        ret->setName(this->getName());
        ret->setGroupName(this->getGroupName());
        ret->setInputSlotsInfo(this->getInputSlotsInfo());
        ret->setOutputSlotsInfo(this->getOutputSlotsInfo());
        ret->setSimDelay(this->getSimDelay());
        ret->setOpInfo(this->getOpInfo());
        ret->setIOGrowthPolicy(this->getIOGrowthPolicy());
        ret->setOutputExpressions(this->getOutputExpressions());
        ret->setBehaviorType(this->getBehaviorType());
        ret->setSimulationFunction(this->getSimulationFunction());
        ret->setAuxData(this->getAuxData());
        ret->setShouldAutoReschedule(this->getShouldAutoReschedule());
        ret->setOwnership(this->getOwnership());
        return ret;
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

    auto from_sim_fn = [](const std::string &name,
                          const std::string &group_name,
                          const SlotsGroupInfo &inputs,
                          const SlotsGroupInfo &outputs,
                          SimDelayNanoSeconds sim_delay,
                          const py::function &sim_function) -> std::shared_ptr<ComponentDefinition> {
        py::gil_scoped_acquire gil;
        auto comp_def = std::make_shared<PyComponentDefinition>();
        comp_def->setName(name);
        comp_def->setGroupName(group_name);
        comp_def->setInputSlotsInfo(inputs);
        comp_def->setOutputSlotsInfo(outputs);
        comp_def->setSimDelay(sim_delay);
        comp_def->setSimulationFunction(sim_function.cast<SimulationFunction>());
        return comp_def;
    };

    auto from_operator_info = [](const std::string &name,
                                 const std::string &group_name,
                                 const SlotsGroupInfo &inputs,
                                 const SlotsGroupInfo &outputs,
                                 SimDelayNanoSeconds sim_delay,
                                 OperatorInfo info) -> std::shared_ptr<ComponentDefinition> {
        py::gil_scoped_acquire gil;
        auto comp_def = std::make_shared<PyComponentDefinition>();
        comp_def->setName(name);
        comp_def->setGroupName(group_name);
        comp_def->setInputSlotsInfo(inputs);
        comp_def->setOutputSlotsInfo(outputs);
        comp_def->setSimDelay(sim_delay);
        comp_def->setOpInfo(info);
        comp_def->setSimulationFunction(ExprEval::exprEvalSimFunc);
        return comp_def;
    };

    auto from_output_expressions = [](const std::string &name,
                                      const std::string &group_name,
                                      const SlotsGroupInfo &inputs,
                                      const SlotsGroupInfo &outputs,
                                      SimDelayNanoSeconds sim_delay,
                                      const std::vector<std::string> &output_expressions) -> std::shared_ptr<ComponentDefinition> {
        py::gil_scoped_acquire gil;
        auto comp_def = std::make_shared<PyComponentDefinition>();
        comp_def->setName(name);
        comp_def->setGroupName(group_name);
        comp_def->setInputSlotsInfo(inputs);
        comp_def->setOutputSlotsInfo(outputs);
        comp_def->setSimDelay(sim_delay);
        comp_def->setOutputExpressions(output_expressions);
        comp_def->setSimulationFunction(ExprEval::exprEvalSimFunc);
        return comp_def;
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
        .DEF_PROP_GSET_T(SimulationFunction, "simulation_function", SimulationFunction)
        .def_static("from_expressions", from_output_expressions,
                    py::arg("name"),
                    py::arg("group_name"),
                    py::arg("inputs"),
                    py::arg("outputs"),
                    py::arg("sim_delay"),
                    py::arg("expressions"),
                    "Create a ComponentDefinition from output expressions.")
        .def_static("from_operator", from_operator_info,
                    py::arg("name"),
                    py::arg("group_name"),
                    py::arg("inputs"),
                    py::arg("outputs"),
                    py::arg("sim_delay"),
                    py::arg("op_info"),
                    "Create a ComponentDefinition from operator info.")
        .def_static("from_sim_fn", from_sim_fn,
                    py::arg("name"),
                    py::arg("group_name"),
                    py::arg("inputs"),
                    py::arg("outputs"),
                    py::arg("sim_delay"),
                    py::arg("sim_function"),
                    "Create a ComponentDefinition from a simulation function.");
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
