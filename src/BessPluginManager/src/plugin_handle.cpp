#include "plugin_handle.h"
#include "component_definition.h"
#include "scene/schematic_diagram.h"
#include "spdlog/spdlog.h"
#include "types.h"
#include <exception>
#include <memory>
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

namespace py = pybind11;

namespace Bess::Plugins {

    PluginHandle::PluginHandle(const pybind11::object &pluginObj)
        : m_pluginObj(pluginObj) {}

    std::vector<std::shared_ptr<SimEngine::ComponentDefinition>> PluginHandle::onComponentsRegLoad() const {
        std::vector<std::shared_ptr<SimEngine::ComponentDefinition>> components;

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
                auto d = pyComp.attr("_native").cast<std::shared_ptr<SimEngine::ComponentDefinition>>();
                components.emplace_back(std::move(d));
                // std::string name = pyComp.attr("name").cast<std::string>();
                // std::string category = pyComp.attr("groupName").cast<std::string>();
                // bool shouldAutoReschedule = pyComp.attr("should_auto_reschedule").cast<bool>();
                // SimEngine::ComponentBehaviorType behaviorType = pyComp.attr("behavior_type").cast<SimEngine::ComponentBehaviorType>();
                // SimEngine::SlotsGroupInfo inputSlotsInfo = pyComp.attr("input_slots_info").cast<SimEngine::SlotsGroupInfo>();
                // SimEngine::SlotsGroupInfo outputSlotsInfo = pyComp.attr("output_slots_info").cast<SimEngine::SlotsGroupInfo>();
                //
                // SimEngine::SimDelayNanoSeconds delayNs = pyComp.attr("sim_delay").cast<SimEngine::SimDelayNanoSeconds>();
                //
                // SimEngine::OperatorInfo opInfo = pyComp.attr("op_info").cast<SimEngine::OperatorInfo>();
                //
                // std::vector<std::string> expressions;
                // if (py::hasattr(pyComp, "expressions")) {
                //     try {
                //         expressions = pyComp.attr("expressions").cast<std::vector<std::string>>();
                //     } catch (std::exception &e) {
                //         spdlog::error("Plugin [{}] component [{}]: failed to cast expressions\n{}", name, category, e.what());
                //     }
                // }
                //
                // std::shared_ptr<py::object> callablePtr;
                // std::shared_ptr<py::object> initialAuxPtr;
                // py::object auxObj;
                // if (py::hasattr(pyComp, "simulation_function")) {
                //     callablePtr = std::make_shared<py::object>(pyComp.attr("simulation_function"));
                // }
                //
                // if (py::hasattr(pyComp, "aux_data")) {
                //     auxObj = pyComp.attr("aux_data");
                //     initialAuxPtr = std::make_shared<py::object>(auxObj);
                // }
                //
                // SimEngine::SimulationFunction simFn;
                // if (callablePtr) {
                //     simFn = [callablePtr, initialAuxPtr, convertResultToComponentState](const std::vector<SimEngine::PinState> &inputs, SimEngine::SimTime t, const SimEngine::ComponentState &prev) -> SimEngine::ComponentState {
                //         py::gil_scoped_acquire gilInner;
                //         py::module_ sim_api = py::module_::import("bessplug.api.sim_engine");
                //         py::object PyPinState = sim_api.attr("PinState");
                //         py::object PyComponentState = sim_api.attr("ComponentState");
                //
                //         py::list py_inputs;
                //         for (const auto &p : inputs) {
                //             py_inputs.append(PyPinState(py::cast(p)));
                //         }
                //         py::object py_prev = PyComponentState(py::cast(prev));
                //         if (initialAuxPtr && py_prev.attr("aux_data").is_none()) {
                //             py::setattr(py_prev, "aux_data", *initialAuxPtr);
                //         }
                //         py::object result;
                //         try {
                //             result = (*callablePtr)(py_inputs, static_cast<long long>(t.count()), py_prev);
                //         } catch (const std::exception &e) {
                //             result = (*callablePtr)(py_inputs, static_cast<long long>(t.count()), py::cast(prev));
                //         }
                //         return convertResultToComponentState(result, prev);
                //     };
                // }
                //
                // SimEngine::ComponentDefinition d;
                // d.setName(name);
                // d.setGroupName(category);
                // d.setShouldAutoReschedule(shouldAutoReschedule);
                // d.setBehaviorType(behaviorType);
                // d.setInputSlotsInfo(inputSlotsInfo);
                // d.setOutputSlotsInfo(outputSlotsInfo);
                // d.setSimDelay(delayNs);
                // d.setOpInfo(opInfo);
                // d.setOutputExpressions(expressions);
                // d.setSimulationFunction(simFn);
                // if (auxObj && !auxObj.is_none()) {
                //     d.setAuxData(auxObj);
                // }
                //
                // components.emplace_back(std::move(d));
            }
        }

        py::gil_scoped_release release;
        return components;
    }

    std::unordered_map<uint64_t, Canvas::SchematicDiagram> PluginHandle::onSchematicSymbolsLoad() const {
        std::unordered_map<uint64_t, Canvas::SchematicDiagram> symbols;

        py::gil_scoped_acquire gil;
        if (py::hasattr(m_pluginObj, "on_schematic_symbols_load")) {
            py::object symDict = m_pluginObj.attr("on_schematic_symbols_load")();

            for (auto item : symDict.cast<py::dict>()) {
                uint64_t key = item.first.cast<uint64_t>();
                auto symbol = item.second.attr("_native").cast<Canvas::SchematicDiagram>();
                symbols.emplace(key, std::move(symbol));
            }
        }

        py::gil_scoped_release release;
        return symbols;
    }

    const pybind11::object &PluginHandle::getPluginObject() const {
        return m_pluginObj;
    }
} // namespace Bess::Plugins
