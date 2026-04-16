#include "imgui.h"
#include "ui/widgets/m_widgets.h"
#include <pybind11/functional.h>
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>
#include <tuple>

namespace py = pybind11;

void bind_bess_ui(py::module &m) {
    m.def("begin_panel", [](const std::string &name,
                            const glm::vec2 &initSize = glm::vec2(200.f, 200.f),
                            bool open = true) {
        ImGui::SetNextWindowSize(ImVec2(initSize.x, initSize.y), ImGuiCond_FirstUseEver);
        ImGui::Begin(name.c_str(), &open, ImGuiWindowFlags_NoFocusOnAppearing);
        return open;
    });

    m.def("end_panel", []() {
        ImGui::End();
    });

    m.def("text", [](const std::string &text) {
        ImGui::Text("%s", text.c_str());
    });

    auto textMultilineFn = [](const std::string &id, const std::string &text, const glm::vec2 &size) {
        Bess::UI::Widgets::SelectableText(id, text, size);
    };

    m.def("text_multiline", textMultilineFn,
          py::arg("id"),
          py::arg("text"),
          py::arg("size") = glm::vec2(0.f, 300.f));

    m.def("same_line", []() {
        ImGui::SameLine();
    });

    m.def("separator", []() {
        ImGui::Separator();
    });

    m.def("align_text_to_frame_padding", []() {
        ImGui::AlignTextToFramePadding();
    });

    /// inputs

    m.def("slider_float", [](const std::string &label, float value, float min, float max) {
        bool changed = ImGui::SliderFloat(label.c_str(), &value, min, max);
        return std::make_tuple(changed, value);
    });

    m.def("checkbox", [](const std::string &label, bool &value) {
        return ImGui::Checkbox(label.c_str(), &value);
    });

    m.def("button", [](const std::string &label) {
        return ImGui::Button(label.c_str());
    });

    auto inputTextFn = [](const std::string &label,
                          std::string &value,
                          const std::string &hint) {
        bool changed = Bess::UI::Widgets::TextBox(label, value, hint);
        return std::make_tuple(changed, value);
    };

    m.def("input_text",
          inputTextFn,
          py::arg("label"),
          py::arg("value"),
          py::arg("hint") = "");

    auto inputTextMultilineFn = [](const std::string &label,
                                   std::string &value,
                                   const std::string &hint = "",
                                   const glm::vec2 &size = glm::vec2(0.f, 400.f)) {
        bool changed = Bess::UI::Widgets::TextBoxMultiline(label, value, hint, size);
        return std::make_tuple(changed, value);
    };

    m.def("input_text_multiline",
          inputTextMultilineFn,
          py::arg("label"),
          py::arg("value"),
          py::arg("hint") = "",
          py::arg("size") = glm::vec2(0.f, 400.f));
}
