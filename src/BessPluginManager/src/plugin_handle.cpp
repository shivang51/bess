#include "plugin_handle.h"

#include "component_definition.h"
#include "spdlog/spdlog.h"
#include "types.h"
#include <exception>
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

namespace py = pybind11;

namespace Bess::Plugins {
    namespace {
        inline std::shared_ptr<py::object> makeGilSafe(py::object obj) {
            return std::shared_ptr<py::object>(new py::object(std::move(obj)), [](py::object *p) {
                py::gil_scoped_acquire gil;
                delete p;
            });
        }
    } // namespace

    PluginHandle::PluginHandle(const pybind11::object &pluginObj)
        : m_pluginObj(pluginObj) {}

    std::vector<SimEngine::ComponentDefinition> PluginHandle::onComponentsRegLoad() const {
        std::vector<SimEngine::ComponentDefinition> components;

        py::gil_scoped_acquire gil;
        if (py::hasattr(m_pluginObj, "on_components_reg_load")) {
            py::object compList = m_pluginObj.attr("on_components_reg_load")();

            auto convertResultToComponentState = [](const py::object &result, const SimEngine::ComponentState &prev) -> SimEngine::ComponentState {
                if (result.is_none()) {
                    return prev;
                }
                if (py::isinstance<SimEngine::ComponentState>(result)) {
                    return result.cast<SimEngine::ComponentState>();
                }
                if (py::hasattr(result, "_native")) {
                    py::object n = result.attr("_native");
                    if (py::isinstance<SimEngine::ComponentState>(n)) {
                        return n.cast<SimEngine::ComponentState>();
                    }
                }
                throw py::type_error("Simulation function must return ComponentState (native), a wrapper with _native, or None");
            };

            for (py::handle item : compList) {
                py::object pyComp = py::reinterpret_borrow<py::object>(item);
                std::string name = pyComp.attr("name").cast<std::string>();
                std::string category = pyComp.attr("category").cast<std::string>();
                int inputCount = pyComp.attr("input_count").cast<int>();
                int outputCount = pyComp.attr("output_count").cast<int>();
                long long delayNs = pyComp.attr("delay_ns").cast<long long>();

                std::vector<std::string> expressions;
                if (py::hasattr(pyComp, "expressions")) {
                    try {
                        expressions = pyComp.attr("expressions").cast<std::vector<std::string>>();
                    } catch (std::exception &e) {
                        spdlog::error("Plugin [{}] component [{}]: failed to cast expressions\n{}", name, category, e.what());
                    }
                }

                std::string opStr;
                if (py::hasattr(pyComp, "op")) {
                    opStr = pyComp.attr("op").cast<std::string>();
                }

                bool negate = false;
                if (py::hasattr(pyComp, "negate")) {
                    negate = pyComp.attr("negate").cast<bool>();
                }

                std::shared_ptr<py::object> callablePtr;
                std::shared_ptr<py::object> initialAuxPtr;
                if (py::hasattr(pyComp, "simulation_function")) {
                    callablePtr = makeGilSafe(pyComp.attr("simulation_function"));
                } else if (py::hasattr(pyComp, "simulate")) {
                    callablePtr = makeGilSafe(pyComp.attr("simulate"));
                }

                if (py::hasattr(pyComp, "state_aux")) {
                    py::object auxObj = pyComp.attr("state_aux");
                    if (!auxObj.is_none())
                        initialAuxPtr = makeGilSafe(auxObj);
                } else if (py::hasattr(pyComp, "aux")) {
                    py::object auxObj = pyComp.attr("aux");
                    if (!auxObj.is_none())
                        initialAuxPtr = makeGilSafe(auxObj);
                }

                SimEngine::SimulationFunction simFn;
                if (callablePtr) {
                    simFn = [callablePtr, initialAuxPtr, convertResultToComponentState](const std::vector<SimEngine::PinState> &inputs, SimEngine::SimTime t, const SimEngine::ComponentState &prev) -> SimEngine::ComponentState {
                        py::gil_scoped_acquire gilInner;
                        py::module_ sim_api = py::module_::import("bessplug.api.sim_engine");
                        py::object PyPinState = sim_api.attr("PinState");
                        py::object PyComponentState = sim_api.attr("ComponentState");

                        py::list py_inputs;
                        for (const auto &p : inputs) {
                            py_inputs.append(PyPinState(py::cast(p)));
                        }
                        py::object py_prev = PyComponentState(py::cast(prev));
                        if (initialAuxPtr && py_prev.attr("aux").is_none()) {
                            // Initialize aux on the engine's native state the first time
                            py::setattr(py_prev, "aux", *initialAuxPtr);
                        }
                        py::object result = (*callablePtr)(py_inputs, static_cast<long long>(t.count()), py_prev);
                        return convertResultToComponentState(result, prev);
                    };
                }

                SimEngine::ComponentDefinition def = [&]() {
                    if (!expressions.empty()) {
                        return SimEngine::ComponentDefinition(name, category, inputCount, outputCount, simFn, SimEngine::SimDelayNanoSeconds(delayNs), expressions);
                    } else {
                        char op = opStr.empty() ? '0' : opStr[0];
                        SimEngine::ComponentDefinition d(name, category, inputCount, outputCount, simFn, SimEngine::SimDelayNanoSeconds(delayNs), op);
                        d.negate = negate;
                        return d;
                    }
                }();

                if (py::hasattr(pyComp, "native_input_pin_details")) {
                    try {
                        auto pins = pyComp.attr("native_input_pin_details").cast<std::vector<SimEngine::PinDetail>>();
                        def.inputPinDetails = pins;
                    } catch (std::exception &e) {
                        spdlog::error("Plugin [{}] component [{}]: failed to cast input_pin_details\n{}", name, category, e.what());
                    }
                }

                if (py::hasattr(pyComp, "native_output_pin_details")) {
                    try {
                        auto pins = pyComp.attr("native_output_pin_details").cast<std::vector<SimEngine::PinDetail>>();
                        def.outputPinDetails = pins;
                    } catch (std::exception &e) {
                        spdlog::error("Plugin [{}] component [{}]: failed to cast output_pin_details\n{}", name, category, e.what());
                    }
                }

                components.push_back(std::move(def));
            }
        }

        py::gil_scoped_release release;
        return components;
    }

    const pybind11::object &PluginHandle::getPluginObject() const {
        return m_pluginObj;
    }
} // namespace Bess::Plugins
