#include "scene/widgets/scene_widgets.h"
#include "common/types.h"
#include "scene_draw_context.h"

#include <optional>
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>
#include <string>
#include <string_view>
#include <tuple>
#include <vector>

namespace py = pybind11;
using namespace Bess::Canvas::SceneWidgets;

namespace {
    void bind_options(py::module_ &m);

    struct PyTextBoxOptions {
        std::string placeholder;
        size_t maxLength = 256;
        float fontSize = 8.f;
        glm::vec2 padding{4.f, 2.f};
        std::optional<Bess::Core::Renderer::Color> backgroundColor;
        std::optional<Bess::Core::Renderer::Color> hoverBackgroundColor;
        std::optional<Bess::Core::Renderer::Color> focusedBackgroundColor;
        std::optional<Bess::Core::Renderer::Color> borderColor;
        std::optional<Bess::Core::Renderer::Color> focusedBorderColor;
        std::optional<Bess::Core::Renderer::Color> textColor;
        std::optional<Bess::Core::Renderer::Color> placeholderColor;
        std::optional<Bess::Core::Renderer::Color> cursorColor;
    };

    struct PyDropdownOptions {
        std::string placeholder = "Select";
        float fontSize = 8.f;
        float optionHeight = 18.f;
        size_t maxVisibleOptions = 8;
        glm::vec2 padding{5.f, 2.f};
        std::optional<Bess::Core::Renderer::Color> backgroundColor;
        std::optional<Bess::Core::Renderer::Color> hoverBackgroundColor;
        std::optional<Bess::Core::Renderer::Color> expandedBackgroundColor;
        std::optional<Bess::Core::Renderer::Color> borderColor;
        std::optional<Bess::Core::Renderer::Color> focusedBorderColor;
        std::optional<Bess::Core::Renderer::Color> optionHoverColor;
        std::optional<Bess::Core::Renderer::Color> optionSelectedColor;
        std::optional<Bess::Core::Renderer::Color> textColor;
        std::optional<Bess::Core::Renderer::Color> mutedTextColor;
    };

    TextBoxOptions toOptions(const PyTextBoxOptions &options) {
        TextBoxOptions out;
        out.placeholder = options.placeholder;
        out.maxLength = options.maxLength;
        out.fontSize = options.fontSize;
        out.padding = options.padding;
        out.backgroundColor = options.backgroundColor;
        out.hoverBackgroundColor = options.hoverBackgroundColor;
        out.focusedBackgroundColor = options.focusedBackgroundColor;
        out.borderColor = options.borderColor;
        out.focusedBorderColor = options.focusedBorderColor;
        out.textColor = options.textColor;
        out.placeholderColor = options.placeholderColor;
        out.cursorColor = options.cursorColor;
        return out;
    }

    DropdownOptions toOptions(const PyDropdownOptions &options) {
        DropdownOptions out;
        out.placeholder = options.placeholder;
        out.fontSize = options.fontSize;
        out.optionHeight = options.optionHeight;
        out.maxVisibleOptions = options.maxVisibleOptions;
        out.padding = options.padding;
        out.backgroundColor = options.backgroundColor;
        out.hoverBackgroundColor = options.hoverBackgroundColor;
        out.expandedBackgroundColor = options.expandedBackgroundColor;
        out.borderColor = options.borderColor;
        out.focusedBorderColor = options.focusedBorderColor;
        out.optionHoverColor = options.optionHoverColor;
        out.optionSelectedColor = options.optionSelectedColor;
        out.textColor = options.textColor;
        out.mutedTextColor = options.mutedTextColor;
        return out;
    }

    bool draw_toggle_button(const Bess::PickingId &id,
                            bool value,
                            const glm::vec3 &buttonPos,
                            const glm::vec2 &buttonSize,
                            Bess::SceneDrawContext &context) {
        return toggleButton(id, value, buttonPos, buttonSize, context);
    }

    bool draw_button(const Bess::PickingId &id,
                     const std::string &label,
                     const glm::vec3 &buttonPos,
                     Bess::SceneDrawContext &context,
                     const ButtonOptions &options = {}) {
        return button(id, label, buttonPos, context, options);
    }

    std::tuple<TextBoxResult, std::string>
    draw_tb(const Bess::PickingId &id,
            std::string &value,
            const glm::vec3 &boxPos,
            const glm::vec2 &boxSize,
            Bess::SceneDrawContext &context,
            const PyTextBoxOptions &options = {}) {
        const auto coreOptions = toOptions(options);
        auto res = textBox(id, &value, boxPos, boxSize, context, coreOptions);
        return {res, value};
    }

    std::tuple<SliderResult, float>
    draw_slider_float(const Bess::PickingId &id,
                      float value,
                      float minValue,
                      float maxValue,
                      const glm::vec3 &sliderPos,
                      const glm::vec2 &sliderSize,
                      Bess::SceneDrawContext &context,
                      const SliderOptions &options = {}) {
        auto res = sliderFloat(id,
                               &value,
                               minValue,
                               maxValue,
                               sliderPos,
                               sliderSize,
                               context,
                               options);
        return {res, value};
    }

    std::tuple<SliderResult, int>
    draw_slider_int(const Bess::PickingId &id,
                    int value,
                    int minValue,
                    int maxValue,
                    const glm::vec3 &sliderPos,
                    const glm::vec2 &sliderSize,
                    Bess::SceneDrawContext &context,
                    const SliderOptions &options = {}) {
        auto res = sliderInt(id,
                             &value,
                             minValue,
                             maxValue,
                             sliderPos,
                             sliderSize,
                             context,
                             options);
        return {res, value};
    }

    std::tuple<DropdownResult, int>
    draw_dropdown(const Bess::PickingId &id,
                  int selectedIndex,
                  const std::vector<std::string> &items,
                  const glm::vec3 &boxPos,
                  const glm::vec2 &boxSize,
                  Bess::SceneDrawContext &context,
                  const PyDropdownOptions &options = {}) {
        std::vector<std::string_view> itemViews;
        itemViews.reserve(items.size());
        for (const auto &item : items) {
            itemViews.emplace_back(item);
        }

        const auto coreOptions = toOptions(options);
        auto res = dropdown(id,
                            &selectedIndex,
                            itemViews,
                            boxPos,
                            boxSize,
                            context,
                            coreOptions);
        return {res, selectedIndex};
    }
} // namespace

void bind_scene_widgets(py::module_ &m) {
    auto mSceneWidgets = m.def_submodule("widgets", "Scene Widgets bindings");

    bind_options(mSceneWidgets);

    mSceneWidgets.def("toggle_button",
                      &draw_toggle_button,
                      py::arg("id"),
                      py::arg("value"),
                      py::arg("button_pos"),
                      py::arg("button_size"),
                      py::arg("context"));

    mSceneWidgets.def("button",
                      &draw_button,
                      py::arg("id"),
                      py::arg("label"),
                      py::arg("button_pos"),
                      py::arg("context"),
                      py::arg("options") = ButtonOptions{});

    mSceneWidgets.def("text_box",
                      &draw_tb,
                      py::arg("id"),
                      py::arg("value"),
                      py::arg("box_pos"),
                      py::arg("box_size"),
                      py::arg("context"),
                      py::arg("options") = PyTextBoxOptions{});

    mSceneWidgets.def("slider_float",
                      &draw_slider_float,
                      py::arg("id"),
                      py::arg("value"),
                      py::arg("min_value"),
                      py::arg("max_value"),
                      py::arg("slider_pos"),
                      py::arg("slider_size"),
                      py::arg("context"),
                      py::arg("options") = SliderOptions{});

    mSceneWidgets.def("slider_int",
                      &draw_slider_int,
                      py::arg("id"),
                      py::arg("value"),
                      py::arg("min_value"),
                      py::arg("max_value"),
                      py::arg("slider_pos"),
                      py::arg("slider_size"),
                      py::arg("context"),
                      py::arg("options") = SliderOptions{});

    mSceneWidgets.def("dropdown",
                      &draw_dropdown,
                      py::arg("id"),
                      py::arg("selected_index"),
                      py::arg("items"),
                      py::arg("box_pos"),
                      py::arg("box_size"),
                      py::arg("context"),
                      py::arg("options") = PyDropdownOptions{});
}

namespace {
    void bind_options(py::module_ &m) {
        py::class_<PyTextBoxOptions>(m, "TextBoxOptions")
            .def(py::init<>())
            .def_readwrite("placeholder", &PyTextBoxOptions::placeholder)
            .def_readwrite("max_len", &PyTextBoxOptions::maxLength)
            .def_readwrite("font_size", &PyTextBoxOptions::fontSize)
            .def_readwrite("padding", &PyTextBoxOptions::padding)
            .def_readwrite("bg_color", &PyTextBoxOptions::backgroundColor)
            .def_readwrite("hover_bg_color",
                           &PyTextBoxOptions::hoverBackgroundColor)
            .def_readwrite("focused_bg_color",
                           &PyTextBoxOptions::focusedBackgroundColor)
            .def_readwrite("border_color", &PyTextBoxOptions::borderColor)
            .def_readwrite("focused_border_color",
                           &PyTextBoxOptions::focusedBorderColor)
            .def_readwrite("text_color", &PyTextBoxOptions::textColor)
            .def_readwrite("placeholder_color",
                           &PyTextBoxOptions::placeholderColor)
            .def_readwrite("cursor_color", &PyTextBoxOptions::cursorColor)
            .def("__repr__", [](const PyTextBoxOptions &options) {
                return "bessplug.api.scene.widgets.TextBoxOptions()";
            });

        py::class_<PyDropdownOptions>(m, "DropdownOptions")
            .def(py::init<>())
            .def_readwrite("placeholder", &PyDropdownOptions::placeholder)
            .def_readwrite("font_size", &PyDropdownOptions::fontSize)
            .def_readwrite("option_height", &PyDropdownOptions::optionHeight)
            .def_readwrite("max_visible_options",
                           &PyDropdownOptions::maxVisibleOptions)
            .def_readwrite("padding", &PyDropdownOptions::padding)
            .def_readwrite("bg_color", &PyDropdownOptions::backgroundColor)
            .def_readwrite("hover_bg_color",
                           &PyDropdownOptions::hoverBackgroundColor)
            .def_readwrite("expanded_bg_color",
                           &PyDropdownOptions::expandedBackgroundColor)
            .def_readwrite("border_color", &PyDropdownOptions::borderColor)
            .def_readwrite("focused_border_color",
                           &PyDropdownOptions::focusedBorderColor)
            .def_readwrite("option_hover_color",
                           &PyDropdownOptions::optionHoverColor)
            .def_readwrite("option_selected_color",
                           &PyDropdownOptions::optionSelectedColor)
            .def_readwrite("text_color", &PyDropdownOptions::textColor)
            .def_readwrite("muted_text_color",
                           &PyDropdownOptions::mutedTextColor)
            .def("__repr__", [](const PyDropdownOptions &options) {
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

        py::class_<ButtonOptions>(m, "ButtonOptions")
            .def(py::init<>())
            .def_readwrite("text_size", &ButtonOptions::textSize)
            .def_readwrite("button_size", &ButtonOptions::buttonSize)
            .def_readwrite("padding", &ButtonOptions::padding)
            .def_readwrite("border_thickness", &ButtonOptions::borderThickness)
            .def_readwrite("border_radius", &ButtonOptions::borderRadius)
            .def_readwrite("background_color", &ButtonOptions::backgroundColor)
            .def_readwrite("hover_background_color",
                           &ButtonOptions::hoverBackgroundColor)
            .def_readwrite("pressed_background_color",
                           &ButtonOptions::pressedBackgroundColor)
            .def_readwrite("border_color", &ButtonOptions::borderColor)
            .def_readwrite("text_color", &ButtonOptions::textColor)
            .def("__repr__", [](const ButtonOptions &options) {
                return "bessplug.api.scene.widgets.ButtonOptions()";
            });
    }
} // namespace
