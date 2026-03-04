#include "glm/ext/vector_float2.hpp"
#include "imgui.h"
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

    m.def("button", [](const std::string &label) {
        return ImGui::Button(label.c_str());
    });

    m.def("same_line", []() {
        ImGui::SameLine();
    });

    m.def("separator", []() {
        ImGui::Separator();
    });

    m.def("align_text_to_frame_padding", []() {
        ImGui::AlignTextToFramePadding();
    });

    m.def("slider_float", [](const std::string &label, float value, float min, float max) {
        bool changed = ImGui::SliderFloat(label.c_str(), &value, min, max);
        return std::make_tuple(changed, value);
    });

    m.def("checkbox", [](const std::string &label, bool &value) {
        return ImGui::Checkbox(label.c_str(), &value);
    });
}
