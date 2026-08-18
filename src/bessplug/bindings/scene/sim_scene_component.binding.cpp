
#include "pages/main_page/scene_components/sim_scene_component.h"
#include "bess_core/scene_driver.h"
#include "common/bess_uuid.h"
#include "common/logger.h"

#include "bess_core/scene/scene_draw_context.h"
#include "bess_core/scene/scene_state/components/scene_component.h"
#include "bess_core/scene/scene_state/scene_state.h" // included for pybind11
#include "bess_core/settings/viewport_theme.h"
#include "pages/main_page/scene_components/slot_scene_component.h"
#include "ui/project_api.h"
#include "json/value.h"
#include <memory>
#include <pybind11/functional.h>
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>
#include <pybind11/stl_bind.h>

namespace py = pybind11;

namespace {
    void logPythonOverrideError(const char *method,
                                const py::error_already_set &error) {
        BESS_ERROR("Python SimulationSceneComponent.{} failed: {}",
                   method,
                   error.what());
    }
} // namespace

class PySimSceneComponent : public Bess::Canvas::SimulationSceneComponent,
                            public py::trampoline_self_life_support {
  public:
    PySimSceneComponent() = default;
    ~PySimSceneComponent() override = default;

    void draw(Bess::SceneDrawContext &context) override {
        try {
            PYBIND11_OVERRIDE(void,
                              Bess::Canvas::SimulationSceneComponent,
                              draw,
                              std::ref(context));
        } catch (const py::error_already_set &error) {
            logPythonOverrideError("draw", error);
        }
        Bess::Canvas::SimulationSceneComponent::draw(context);
    }

    std::vector<Bess::UUID> cleanup(Bess::Canvas::SceneState &state,
                                    Bess::UUID caller) override {
        try {
            PYBIND11_OVERRIDE(std::vector<Bess::UUID>,
                              Bess::Canvas::SimulationSceneComponent,
                              cleanup,
                              std::ref(state),
                              caller);
        } catch (const py::error_already_set &error) {
            logPythonOverrideError("cleanup", error);
        }
        return Bess::Canvas::SimulationSceneComponent::cleanup(state, caller);
    }

    std::vector<std::shared_ptr<SceneComponent>>
    clone(const Bess::Canvas::SceneState &sceneState) const override {
        auto c = copy();
        return cloneSimulationComponent(sceneState, c);
    }

    virtual std::shared_ptr<Bess::Canvas::SimulationSceneComponent>
    copy() const {
        try {
            PYBIND11_OVERRIDE_NAME(
                std::shared_ptr<Bess::Canvas::SimulationSceneComponent>,
                PySimSceneComponent,
                "copy",
                copy);
        } catch (const py::error_already_set &error) {
            logPythonOverrideError("copy", error);
        }
        return std::make_shared<Bess::Canvas::SimulationSceneComponent>(*this);
    }

    void drawSchematic(Bess::SceneDrawContext &context) override {
        try {
            PYBIND11_OVERRIDE_NAME(void,
                                   Bess::Canvas::SimulationSceneComponent,
                                   "draw_schematic",
                                   drawSchematic,
                                   std::ref(context));
        } catch (const py::error_already_set &error) {
            logPythonOverrideError("draw_schematic", error);
        }
        Bess::Canvas::SimulationSceneComponent::drawSchematic(context);
    }

    void update(Bess::TimeMs timeStep,
                Bess::Canvas::SceneState &state) override {
        try {
            PYBIND11_OVERRIDE(void,
                              Bess::Canvas::SimulationSceneComponent,
                              update,
                              timeStep,
                              std::ref(state));
        } catch (const py::error_already_set &error) {
            logPythonOverrideError("update", error);
        }
        Bess::Canvas::SimulationSceneComponent::update(timeStep, state);
    }

    void prepareUI(Bess::SceneUIPrepareCtx &ctx) override {
        try {
            PYBIND11_OVERRIDE_NAME(void,
                                   Bess::Canvas::SimulationSceneComponent,
                                   "prepare_ui",
                                   prepareUI,
                                   std::ref(ctx));
        } catch (const py::error_already_set &error) {
            logPythonOverrideError("prepare_ui", error);
        }
        Bess::Canvas::SimulationSceneComponent::prepareUI(ctx);
    }

    void onScaleChanged() override {
        try {
            PYBIND11_OVERRIDE_NAME(void,
                                   Bess::Canvas::SimulationSceneComponent,
                                   "on_scale_changed",
                                   onScaleChanged);
        } catch (const py::error_already_set &error) {
            logPythonOverrideError("on_scale_changed", error);
        }
        Bess::Canvas::SimulationSceneComponent::onScaleChanged();
    }

    std::string getTypeName() const override {
        try {
            PYBIND11_OVERRIDE_NAME(std::string,
                                   Bess::Canvas::SimulationSceneComponent,
                                   "get_type_name",
                                   getTypeName);
        } catch (const py::error_already_set &error) {
            logPythonOverrideError("get_type_name", error);
        }
        return Bess::Canvas::SimulationSceneComponent::getTypeName();
    }

    Json::Value toJson() const override {
        try {
            PYBIND11_OVERRIDE_NAME(Json::Value,
                                   Bess::Canvas::SimulationSceneComponent,
                                   "to_json",
                                   toJson);
        } catch (const py::error_already_set &error) {
            logPythonOverrideError("to_json", error);
        }
        return Bess::Canvas::SimulationSceneComponent::toJson();
    }

    void drawPropertiesUI(Bess::Canvas::SceneState &sceneState) override {
        try {
            PYBIND11_OVERRIDE_NAME(void,
                                   Bess::Canvas::SimulationSceneComponent,
                                   "draw_properties_ui",
                                   drawPropertiesUI,
                                   std::ref(sceneState));
        } catch (const py::error_already_set &error) {
            logPythonOverrideError("draw_properties_ui", error);
        }
        Bess::Canvas::SimulationSceneComponent::drawPropertiesUI(sceneState);
    }

    glm::vec2 calculateScale(const Bess::Canvas::SceneState &state) override {
        try {
            PYBIND11_OVERRIDE_NAME(glm::vec2,
                                   Bess::Canvas::SimulationSceneComponent,
                                   "calc_scale",
                                   calculateScale,
                                   std::ref(state));
        } catch (const py::error_already_set &error) {
            logPythonOverrideError("calc_scale", error);
        }
        return Bess::Canvas::SimulationSceneComponent::calculateScale(state);
    }
};

void bind_sim_scene_component(py::module_ &m) {
    typedef Bess::SimEngine::Drivers::CompDef TCompDef;
    typedef std::shared_ptr<TCompDef> TCompDefPtr;

    const auto setup = [](Bess::Canvas::SimulationSceneComponent &comp,
                          const TCompDefPtr &compDef) {
        comp.setCompDef(compDef);
        comp.setName(compDef->getName());

        // STYLE
        auto &style = comp.getStyle();
        const auto &colors = Bess::ViewportTheme::colors;
        style.color = colors.componentBG;
        style.headerColor =
            Bess::ViewportTheme::getCompHeaderColor(compDef->getGroupName());
        style.color = colors.componentBG;

        std::vector<std::shared_ptr<Bess::Canvas::SlotSceneComponent>>
            createdSlots;

        const auto inpDetails = compDef->getInputPortDescriptor();
        const auto outDetails = compDef->getOutputPortDescriptor();

        int inSlotIdx = 0, outSlotIdx = 0;
        char inpCh = 'A', outCh = 'a';

        const auto &slots = comp.createIOSlots(inpDetails, outDetails);

        for (const auto &slot : slots) {
            if (slot->isInputSlot()) {
                const auto name = inpDetails.nameAt(inSlotIdx++);
                if (!name.empty()) {
                    slot->setName(name);
                } else {
                    slot->setName(std::string(1, inpCh++));
                }
            } else {
                const auto name = outDetails.nameAt(outSlotIdx++);
                if (!name.empty()) {
                    slot->setName(name);
                } else {
                    slot->setName(std::string(1, outCh++));
                }
            }
            createdSlots.push_back(slot);
        }

        if (inpDetails.isResizeable) {
            auto slot = std::make_shared<Bess::Canvas::SlotSceneComponent>();
            slot->setPortDirection(Bess::SimEngine::PortDirection::input);
            slot->setSignalKind(inpDetails.resizePortSpec().signalKind);
            slot->setResizeTrigger(true);
            slot->setIndex(-1); // assign -1 for resize slots
            comp.addInputSlot(slot->getUuid(), false);
            createdSlots.push_back(slot);
        }

        if (outDetails.isResizeable) {
            auto slot = std::make_shared<Bess::Canvas::SlotSceneComponent>();
            slot->setPortDirection(Bess::SimEngine::PortDirection::output);
            slot->setSignalKind(outDetails.resizePortSpec().signalKind);
            slot->setResizeTrigger(true);
            slot->setIndex(-1); // assign -1 for resize slots
            comp.addOutputSlot(slot->getUuid(), false);
            createdSlots.push_back(slot);
        }

        auto &scenes = Bess::UI::Proj::scenes();
        auto &sceneState = scenes.getActiveScene()->getState();
        for (const auto &slot : createdSlots) {
            sceneState.addComponent(slot);
        }
    };

    auto simCompBinding =
        py::class_<Bess::Canvas::SimulationSceneComponent,
                   PySimSceneComponent,
                   Bess::Canvas::SceneComponent,
                   py::smart_holder>(m, "SimulationSceneComponent")
            .def(py::init<>())
            .def(py::pickle(
                [](const Bess::Canvas::SimulationSceneComponent &self) {
                    Json::StreamWriterBuilder builder;
                    builder["indentation"] = "";
                    const std::string output =
                        Json::writeString(builder, self.toJson());
                    return py::make_tuple(output);
                },
                [](py::tuple &t) {
                    const auto &self = t[0].cast<std::string>();

                    Json::CharReaderBuilder builder;
                    auto reader = std::shared_ptr<Json::CharReader>(
                        builder.newCharReader());

                    Json::Value json;
                    std::string errors;

                    bool parsingSuccessful =
                        reader->parse(self.c_str(),
                                      self.c_str() + self.size(),
                                      &json,
                                      &errors);

                    if (!parsingSuccessful) {
                        throw std::runtime_error(
                            "Failed to parse SimulationSceneComponent from "
                            "JSON: " +
                            errors);
                    }

                    auto newComp = PySimSceneComponent();
                    auto sharedComp =
                        std::shared_ptr<Bess::Canvas::SimulationSceneComponent>(
                            &newComp,
                            [](Bess::Canvas::SimulationSceneComponent *) {});
                    Bess::Canvas::SimulationSceneComponent::fromJson(
                        json, sharedComp);
                    return newComp;
                }))
            // Python scene components render on the UI thread. Release the
            // GIL around base operations that can wait for the scheduler so a
            // Python simulation callback can finish and release that barrier.
            .def("cleanup",
                 &Bess::Canvas::SimulationSceneComponent::cleanup,
                 py::arg("state"),
                 py::arg("caller") = 0,
                 py::call_guard<py::gil_scoped_release>())
            .def("draw",
                 &Bess::Canvas::SimulationSceneComponent::draw,
                 py::arg("context"),
                 py::call_guard<py::gil_scoped_release>())
            .def("draw_schematic",
                 &Bess::Canvas::SimulationSceneComponent::drawSchematic,
                 py::arg("context"),
                 py::call_guard<py::gil_scoped_release>())
            .def("update",
                 &Bess::Canvas::SimulationSceneComponent::update,
                 py::arg("time_step"),
                 py::arg("scene_state"),
                 py::call_guard<py::gil_scoped_release>())
            .def("setup", setup, py::arg("comp_def"))
            .def("get_input_states",
                 &Bess::Canvas::SimulationSceneComponent::getInputStates,
                 py::arg("scene_state"),
                 py::call_guard<py::gil_scoped_release>())
            .def("get_output_states",
                 &Bess::Canvas::SimulationSceneComponent::getOutputStates,
                 py::arg("scene_state"),
                 py::call_guard<py::gil_scoped_release>())
            .def("draw_slots",
                 &Bess::Canvas::SimulationSceneComponent::drawSlots,
                 py::arg("context"),
                 py::call_guard<py::gil_scoped_release>())
            .def("draw_background",
                 &Bess::Canvas::SimulationSceneComponent::drawBackground,
                 py::arg("context"))
            .def("on_scale_changed",
                 &Bess::Canvas::SimulationSceneComponent::onScaleChanged)
            .def("get_type_name",
                 &Bess::Canvas::SimulationSceneComponent::getTypeName)
            .def("set_scale_dirty",
                 &Bess::Canvas::SimulationSceneComponent::setScaleDirty,
                 py::arg("val") = true)
            .def(
                "set_schematic_scale_dirty",
                &Bess::Canvas::SimulationSceneComponent::setSchematicScaleDirty,
                py::arg("val") = true)
            .def_property_readonly(
                "comp_def",
                [](const Bess::Canvas::SimulationSceneComponent &self) {
                    return self.getCompDef();
                })
            .def_property(
                "transform", //\n
                [](const Bess::Canvas::SimulationSceneComponent &self)
                    -> const Bess::Canvas::Transform & {
                    return self.getTransform();
                }, // \n
                &Bess::Canvas::SimulationSceneComponent::setTransform,
                py::return_value_policy::reference_internal)
            .def_property(
                "schematic_transform", //\n
                [](const Bess::Canvas::SimulationSceneComponent &self)
                    -> const Bess::Canvas::Transform & {
                    return self.getSchematicTransform();
                }, // \n
                &Bess::Canvas::SimulationSceneComponent::setSchematicTransform,
                py::return_value_policy::reference_internal)
            .def_property(
                "name", //\n
                [](const Bess::Canvas::SimulationSceneComponent &self) {
                    return self.getName();
                }, // \n
                &Bess::Canvas::SimulationSceneComponent::setName)
            .def_property(
                "position", //\n
                [](const Bess::Canvas::SimulationSceneComponent &self) {
                    return self.getTransform().position;
                }, // \n
                &Bess::Canvas::SimulationSceneComponent::setPosition)
            .def_property(
                "scale", //\n
                [](const Bess::Canvas::SimulationSceneComponent &self) {
                    return self.getTransform().scale;
                }, // \n
                &Bess::Canvas::SimulationSceneComponent::setScale)
            .def_property(
                "schematic_scale", //\n
                [](const Bess::Canvas::SimulationSceneComponent &self) {
                    return self.getSchematicTransform().scale;
                }, // \n
                [](Bess::Canvas::SimulationSceneComponent &self,
                   const glm::vec2 &scale) {
                    self.getSchematicTransform().scale = scale;
                    self.setSchSlotsPosDirty();
                })
            .def_property_readonly(
                "runtime_id", //\n
                [](const Bess::Canvas::SimulationSceneComponent &self) {
                    return self.getRuntimeId();
                })

            .def_property_readonly(
                "inp_slots_container",
                [](const Bess::Canvas::SimulationSceneComponent &self) {
                    return self.getInputSlotsContainer();
                },
                py::return_value_policy::reference_internal)
            .def_property_readonly(
                "out_slots_container",
                [](const Bess::Canvas::SimulationSceneComponent &self) {
                    return self.getOutputSlotsContainer();
                },
                py::return_value_policy::reference_internal)

            .def("copy",
                 [&](const Bess::Canvas::SimulationSceneComponent &self) {
                     auto c = std::make_shared<
                         Bess::Canvas::SimulationSceneComponent>(self);
                     return c;
                 })
            .def("to_json", &Bess::Canvas::SimulationSceneComponent::toJson)
            .def("calc_scale",
                 &Bess::Canvas::SimulationSceneComponent::calculateScale,
                 py::arg("scene_state"))
            .def("get_slot_start_y",
                 &Bess::Canvas::SimulationSceneComponent::getSlotStartY)
            .def("draw_properties_ui",
                 &Bess::Canvas::SimulationSceneComponent::drawPropertiesUI,
                 py::arg("scene_state"),
                 py::call_guard<py::gil_scoped_release>())
            .def("prepare_ui",
                 &Bess::Canvas::SimulationSceneComponent::prepareUI,
                 py::arg("ctx"),
                 py::call_guard<py::gil_scoped_release>())

            .def_property("icon",
                          py::overload_cast<>(
                              &Bess::Canvas::SimulationSceneComponent::getIcon),
                          &Bess::Canvas::SimulationSceneComponent::setIcon);

    // decorators
    auto deserDecorator = [&](const py::function &fromJsonFunc) {
        return py::cpp_function([fromJsonFunc](const py::args &args,
                                               const py::kwargs &kwargs) {
            auto pyComp = fromJsonFunc(*args, **kwargs);

            if (pyComp.is_none()) {
                return std::shared_ptr<Bess::Canvas::SimulationSceneComponent>(
                    nullptr);
            }

            auto comp = pyComp.cast<
                std::shared_ptr<Bess::Canvas::SimulationSceneComponent>>();
            Bess::Canvas::SimulationSceneComponent::fromJson(
                args[0].cast<Json::Value>(), comp);
            return comp;
        });
    };
    simCompBinding.def_static(
        "deser",
        deserDecorator,
        py::arg("Use as a decorator over from_json static function"));
}
