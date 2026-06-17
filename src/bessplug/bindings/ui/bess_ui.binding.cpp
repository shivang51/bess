#include "dock_ids.h"
#include "gtc/type_ptr.hpp"
#include "imgui.h"
#include "imgui_internal.h"
#include "ui/widgets/m_widgets.h"
#include "ui/ui_main/ui_main.h"
#include <pybind11/functional.h>
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>
#include <tuple>

namespace py = pybind11;

void bind_bess_ui(py::module &m) {

    m.def("set_next_window_size", [](const glm::vec2 &size) {
        ImGui::SetNextWindowSize(ImVec2(size.x, size.y));
    });

    m.def("begin_panel",
          [](const std::string &name,
             const glm::vec2 &initSize = glm::vec2(200.f, 200.f),
             bool open = true) {
              ImGui::SetNextWindowSize(ImVec2(initSize.x, initSize.y),
                                       ImGuiCond_FirstUseEver);
              ImGui::Begin(
                  name.c_str(), &open, ImGuiWindowFlags_NoFocusOnAppearing);
              return open;
          });

    m.def("end_panel", []() { ImGui::End(); });

    m.def("text",
          [](const std::string &text) { ImGui::Text("%s", text.c_str()); });

    auto textMultilineFn = [](const std::string &id,
                              const std::string &text,
                              const glm::vec2 &size) {
        Bess::UI::Widgets::SelectableText(id, text, size);
    };

    m.def("text_multiline",
          textMultilineFn,
          py::arg("id"),
          py::arg("text"),
          py::arg("size") = glm::vec2(0.f, 300.f));

    m.def("same_line", []() { ImGui::SameLine(); });

    m.def(
        "separator",
        [](bool vertical = false) {
            ImGui::SeparatorEx(vertical ? ImGuiSeparatorFlags_Vertical
                                        : ImGuiSeparatorFlags_Horizontal);
        },
        py::arg("vertical") = false,
        "Create a separator line (horizontal or vertical)");

    m.def("align_text_to_frame_padding",
          []() { ImGui::AlignTextToFramePadding(); });

    /// inputs

    m.def("slider_float",
          [](const std::string &label, float value, float min, float max) {
              bool changed =
                  ImGui::SliderFloat(label.c_str(), &value, min, max);
              return std::make_tuple(changed, value);
          });

    m.def(
        "checkbox",
        [](const std::string &label,
           bool &value,
           bool expand = true,
           bool alignToFramePadding = false) {
            const auto changed = Bess::UI::Widgets::CheckboxWithLabel(
                label.c_str(), &value, expand, alignToFramePadding);

            return std::make_tuple(changed, value);
        },
        py::arg("label"),
        py::arg("value"),
        py::arg("expand") = true,
        py::arg("align_to_frame_padding") = false);

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
                                   const glm::vec2 &size =
                                       glm::vec2(0.f, 400.f)) {
        bool changed = Bess::UI::Widgets::TextBoxMultiline(label, value, size);
        return std::make_tuple(changed, value);
    };

    m.def("input_text_multiline",
          inputTextMultilineFn,
          py::arg("label"),
          py::arg("value"),
          py::arg("size") = glm::vec2(0.f, 400.f));

    m.def(
        "combo_box",
        [](const std::string &label,
           std::string &currentItem,
           const std::vector<std::string> &items) {
            bool changed =
                Bess::UI::Widgets::ComboBox(label, currentItem, items);
            return std::make_tuple(changed, currentItem);
        },
        py::arg("label"),
        py::arg("currentItem"),
        py::arg("items"));

    m.def("color_edit4", [](const std::string &label, glm::vec4 &color) {
        bool changed = ImGui::ColorEdit4(label.c_str(), glm::value_ptr(color));
        return std::make_tuple(changed, color);
    });

    m.def("collapsing_node", [](int key, const std::string &label) {
        return Bess::UI::Widgets::TreeNode(key, label);
    });

    m.def("tree_pop", []() { ImGui::TreePop(); });

    py::enum_<Bess::UI::Dock>(m, "Dock")
        .value("left", Bess::UI::Dock::left)
        .value("right", Bess::UI::Dock::right)
        .value("top", Bess::UI::Dock::top)
        .value("bottom", Bess::UI::Dock::bottom);

    m.def("try_reg_dock",
          [](const std::string &panelName, Bess::UI::Dock dock) {
              Bess::UI::UIMain::regExtPanelDock(panelName, dock);
          });

    // TABLE

    m.def("begin_table", [](const std::string &id, int columnCount) {
        return ImGui::BeginTable(
            id.c_str(),
            columnCount,
            ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg |
                ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_Resizable |
                ImGuiTableFlags_Reorderable);
    });

    m.def("end_table", []() { ImGui::EndTable(); });

    m.def("table_setup_column", [](const std::string &label) {
        ImGui::TableSetupColumn(label.c_str());
    });

    m.def("table_headers_row", []() { ImGui::TableHeadersRow(); });

    m.def("table_next_column", []() { ImGui::TableNextColumn(); });

    // TABLE END

    // MENUBAR

    m.def("begin_menu_bar", []() {
        ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.f);
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(6.f, 6.f));
        return ImGui::BeginMainMenuBar();
    });

    m.def("end_menu_bar", []() {
        ImGui::EndMainMenuBar();
        ImGui::PopStyleVar(2);
    });

    m.def("begin_menu", [](const std::string &label) {
        return ImGui::BeginMenu(label.c_str());
    });

    m.def(
        "menu_item",
        [](const std::string &label,
           const std::string &shortcut,
           bool selected = false,
           bool enabled = true) {
            return ImGui::MenuItem(
                label.c_str(), shortcut.c_str(), selected, enabled);
        },
        py::arg("label"),
        py::arg("shortcut") = "",
        py::arg("selected") = false,
        py::arg("enabled") = true);

    m.def("end_menu", []() { ImGui::EndMenu(); });

    // MENUBAR END
}
