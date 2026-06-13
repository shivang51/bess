#include "scene/widgets/scene_widgets.h"
#include "common/types.h"
#include "scene_draw_context.h"

#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

namespace py = pybind11;
using namespace Bess::Canvas::SceneWidgets;

namespace {
    void bind_options(py::module_ &m);

    bool draw_toggle_button(const Bess::PickingId &id, bool value,
                            const glm::vec3 &buttonPos,
                            const glm::vec2 &buttonSize,
                            Bess::SceneDrawContext &context) {
        return toggleButton(id, value, buttonPos, buttonSize, context);
    }

    std::tuple<TextBoxResult, std::string>
    draw_tb(const Bess::PickingId &id, std::string &value,
            const glm::vec3 &boxPos, const glm::vec2 &boxSize,
            Bess::SceneDrawContext &context,
            const TextBoxOptions &options = {}) {
        auto res = textBox(id, &value, boxPos, boxSize, context, options);
        return {res, value};
    }

    std::tuple<SliderResult, float> draw_slider_float(
        const Bess::PickingId &id, float value, float minValue, float maxValue,
        const glm::vec3 &sliderPos, const glm::vec2 &sliderSize,
        Bess::SceneDrawContext &context, const SliderOptions &options = {}) {
        auto res = sliderFloat(id, &value, minValue, maxValue, sliderPos,
                               sliderSize, context, options);
        return {res, value};
    }

    std::tuple<SliderResult, int> draw_slider_int(
        const Bess::PickingId &id, int value, int minValue, int maxValue,
        const glm::vec3 &sliderPos, const glm::vec2 &sliderSize,
        Bess::SceneDrawContext &context, const SliderOptions &options = {}) {
        auto res = sliderInt(id, &value, minValue, maxValue, sliderPos,
                             sliderSize, context, options);
        return {res, value};
    }

    std::tuple<DropdownResult, int>
    draw_dropdown(const Bess::PickingId &id, int selectedIndex,
                  const std::vector<std::string> &items,
                  const glm::vec3 &boxPos, const glm::vec2 &boxSize,
                  Bess::SceneDrawContext &context,
                  const DropdownOptions &options = {}) {
        auto res = dropdown(id, &selectedIndex, items, boxPos, boxSize, context,
                            options);
        return {res, selectedIndex};
    }
} // namespace

void bind_scene_widgets(py::module_ &m) {
    auto mSceneWidgets = m.def_submodule("widgets", "Scene Widgets bindings");

    bind_options(mSceneWidgets);

    mSceneWidgets.def("toggle_button", &draw_toggle_button, py::arg("id"),
                      py::arg("value"), py::arg("button_pos"),
                      py::arg("button_size"), py::arg("context"));

    mSceneWidgets.def("button", &button, py::arg("id"), py::arg("label"),
                      py::arg("button_pos"), py::arg("button_size"),
                      py::arg("label_color"), py::arg("context"));

    mSceneWidgets.def("text_box", &draw_tb, py::arg("id"), py::arg("value"),
                      py::arg("box_pos"), py::arg("box_size"),
                      py::arg("context"),
                      py::arg("options") = TextBoxOptions{});

    mSceneWidgets.def("slider_float", &draw_slider_float, py::arg("id"),
                      py::arg("value"), py::arg("min_value"),
                      py::arg("max_value"), py::arg("slider_pos"),
                      py::arg("slider_size"), py::arg("context"),
                      py::arg("options") = SliderOptions{});

    mSceneWidgets.def("slider_int", &draw_slider_int, py::arg("id"),
                      py::arg("value"), py::arg("min_value"),
                      py::arg("max_value"), py::arg("slider_pos"),
                      py::arg("slider_size"), py::arg("context"),
                      py::arg("options") = SliderOptions{});

    mSceneWidgets.def(
        "dropdown", &draw_dropdown, py::arg("id"), py::arg("selected_index"),
        py::arg("items"), py::arg("box_pos"), py::arg("box_size"),
        py::arg("context"), py::arg("options") = DropdownOptions{});
}

namespace {
    void bind_options(py::module_ &m) {
        py::class_<TextBoxOptions>(m, "TextBoxOptions")
            .def(py::init<>())
            .def_readwrite("placeholder", &TextBoxOptions::placeholder)
            .def_readwrite("max_len", &TextBoxOptions::maxLength)
            .def_readwrite("font_size", &TextBoxOptions::fontSize)
            .def_readwrite("padding", &TextBoxOptions::padding)
            .def_readwrite("bg_color", &TextBoxOptions::backgroundColor)
            .def_readwrite("hover_bg_color",
                           &TextBoxOptions::hoverBackgroundColor)
            .def_readwrite("focused_bg_color",
                           &TextBoxOptions::focusedBackgroundColor)
            .def_readwrite("border_color", &TextBoxOptions::borderColor)
            .def_readwrite("focused_border_color",
                           &TextBoxOptions::focusedBorderColor)
            .def_readwrite("text_color", &TextBoxOptions::textColor)
            .def_readwrite("placeholder_color",
                           &TextBoxOptions::placeholderColor)
            .def_readwrite("cursor_color", &TextBoxOptions::cursorColor)
            .def("__repr__", [](const TextBoxOptions &options) {
                return "bessplug.api.scene.widgets.TextBoxOptions()";
            });

        py::class_<DropdownOptions>(m, "DropdownOptions")
            .def(py::init<>())
            .def_readwrite("placeholder", &DropdownOptions::placeholder)
            .def_readwrite("font_size", &DropdownOptions::fontSize)
            .def_readwrite("option_height", &DropdownOptions::optionHeight)
            .def_readwrite("max_visible_options",
                           &DropdownOptions::maxVisibleOptions)
            .def_readwrite("padding", &DropdownOptions::padding)
            .def_readwrite("bg_color", &DropdownOptions::backgroundColor)
            .def_readwrite("hover_bg_color",
                           &DropdownOptions::hoverBackgroundColor)
            .def_readwrite("expanded_bg_color",
                           &DropdownOptions::expandedBackgroundColor)
            .def_readwrite("border_color", &DropdownOptions::borderColor)
            .def_readwrite("focused_border_color",
                           &DropdownOptions::focusedBorderColor)
            .def_readwrite("option_hover_color",
                           &DropdownOptions::optionHoverColor)
            .def_readwrite("option_selected_color",
                           &DropdownOptions::optionSelectedColor)
            .def_readwrite("text_color", &DropdownOptions::textColor)
            .def_readwrite("muted_text_color", &DropdownOptions::mutedTextColor)
            .def("__repr__", [](const DropdownOptions &options) {
                return "bessplug.api.scene.widgets.DropdownOptions()";
            });

        py::class_<TextBoxResult>(m, "TextBoxResult")
            .def(py::init<>())
            .def_readwrite("changed", &TextBoxResult::changed)
            .def_readwrite("submitted", &TextBoxResult::submitted)
            .def_readwrite("canceled", &TextBoxResult::canceled)
            .def_readwrite("focused", &TextBoxResult::focused);

        py::class_<SliderResult>(m, "SliderResult")
            .def(py::init<>())
            .def_readwrite("changed", &SliderResult::changed)
            .def_readwrite("editing", &SliderResult::editing)
            .def_readwrite("focused", &SliderResult::focused);

        py::class_<DropdownResult>(m, "DropdownResult")
            .def(py::init<>())
            .def_readwrite("changed", &DropdownResult::changed)
            .def_readwrite("opened", &DropdownResult::opened)
            .def_readwrite("closed", &DropdownResult::closed)
            .def_readwrite("expanded", &DropdownResult::expanded)
            .def_readwrite("selected_index", &DropdownResult::selectedIndex);

        py::class_<SliderOptions>(m, "SliderOptions")
            .def(py::init<>())
            .def_readwrite("step", &SliderOptions::step)
            .def_readwrite("precision", &SliderOptions::precision)
            .def_readwrite("show_value", &SliderOptions::showValue)
            .def_readwrite("font_size", &SliderOptions::fontSize)
            .def_readwrite("track_height", &SliderOptions::trackHeight)
            .def_readwrite("knob_radius", &SliderOptions::knobRadius)
            .def_readwrite("padding", &SliderOptions::padding)
            .def_readwrite("background_color", &SliderOptions::backgroundColor)
            .def_readwrite("hover_background_color",
                           &SliderOptions::hoverBackgroundColor)
            .def_readwrite("focused_border_color",
                           &SliderOptions::focusedBorderColor)
            .def_readwrite("track_color", &SliderOptions::trackColor)
            .def_readwrite("fill_color", &SliderOptions::fillColor)
            .def_readwrite("knob_color", &SliderOptions::knobColor)
            .def_readwrite("text_color", &SliderOptions::textColor)
            .def("__repr__", [](const SliderOptions &options) {
                return "bessplug.api.scene.widgets.SliderOptions()";
            });
    }
} // namespace
