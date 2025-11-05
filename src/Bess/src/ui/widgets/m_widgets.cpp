#include "ui/widgets/m_widgets.h"
#include "imgui.h"

namespace Bess::UI::Widgets {
    static int InputTextCallback(ImGuiInputTextCallbackData *data) {
        if (data->EventFlag == ImGuiInputTextFlags_CallbackResize) {
            std::string *str = (std::string *)data->UserData;
            str->resize(data->BufTextLen);
            data->Buf = (char *)str->c_str();
        }
        return 0;
    }

    bool TextBox(const std::string &label, std::string &value, const std::string &hintText) {
        ImGui::AlignTextToFramePadding();
        if (!label.empty() && label[0] != '#' && label[1] != '#') {
            ImGui::Text("%s", label.c_str());
            ImGui::SameLine();
        }
        if (ImGui::InputTextWithHint(("##Tb" + label).c_str(), hintText.c_str(),
                                     value.data(), value.capacity() + 1, ImGuiInputTextFlags_CallbackResize,
                                     InputTextCallback, (void *)&value)) {
            return true;
        }

        return false;
    }

    bool CheckboxWithLabel(const char *label, bool *value) {
        ImGui::Text("%s", label);
        auto style = ImGui::GetStyle();
        float availWidth = ImGui::GetContentRegionAvail().x;
        ImGui::SameLine();
        float checkboxWidth = ImGui::CalcTextSize("X").x + style.FramePadding.x;
        ImGui::SetCursorPosX(availWidth - checkboxWidth);
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(0, 0));
        bool changed = ImGui::Checkbox(("##" + std::string(label)).c_str(), value);
        ImGui::PopStyleVar();
        return changed;
    }
} // namespace Bess::UI::Widgets
