#include "ui/m_widgets.h"
#include "imgui.h"
#include "imgui_internal.h"

namespace Bess::UI {
    static int InputTextCallback(ImGuiInputTextCallbackData *data) {
        if (data->EventFlag == ImGuiInputTextFlags_CallbackResize) {
            std::string *str = (std::string *)data->UserData;
            str->resize(data->BufTextLen);
            data->Buf = (char *)str->c_str();
        }
        return 0;
    }

    bool MWidgets::TextBox(const std::string &label, std::string &value, const std::string &hintText) {
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
} // namespace Bess::UI
