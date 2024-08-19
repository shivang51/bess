#include "ui/m_widgets.h"
#include "imgui.h"
#include <algorithm>

namespace Bess::UI {
    static int InputTextCallback(ImGuiInputTextCallbackData* data)
    {
        if (data->EventFlag == ImGuiInputTextFlags_CallbackResize)
        {
            std::string* str = (std::string*)data->UserData;
            str->resize(data->BufTextLen);
            data->Buf = (char*)str->c_str();
        }
        return 0;
    }

    bool MWidgets::TextBox(const std::string& label, std::string& value, const std::string& hintText)
    {
        if (ImGui::InputTextWithHint(label.c_str(), hintText.c_str(), value.data(), value.capacity() + 1, ImGuiInputTextFlags_CallbackResize, InputTextCallback, (void*)&value)) {
            return true;
        }

        return false;
    }
}