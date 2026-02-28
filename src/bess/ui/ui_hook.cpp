#include "ui_hook.h"
#include "gtc/type_ptr.hpp"
#include "imgui.h"
#include "widgets/m_widgets.h"

namespace Bess::UI::Hook {
    void UIHook::setPropertyDescriptors(const std::vector<PropertyDesc> &descs) {
        m_propDescriptors = descs;
    }

    const std::vector<PropertyDesc> &UIHook::getPropertyDescriptors() const {
        return m_propDescriptors;
    }

    void UIHook::addPropertyDescriptor(const PropertyDesc &desc) {
        m_propDescriptors.push_back(desc);
    }

    void UIHook::draw() {
        for (const auto &desc : m_propDescriptors) {
            drawProperty(desc);
        }
    }

    void UIHook::drawProperty(const PropertyDesc &desc) {
        const std::string key = "##prop_" + desc.name;

        switch (desc.type) {
        case PropertyDescType::bool_t: {
            bool value = std::get<bool>(desc.binding.getter());
            if (ImGui::Checkbox(desc.name.c_str(), &value)) {
                desc.binding.setter(value);
            }
        } break;
        case PropertyDescType::int_t: {
            int64_t value = std::get<int64_t>(desc.binding.getter());
            if (ImGui::InputScalar(desc.name.c_str(), ImGuiDataType_S64, &value)) {
                desc.binding.setter(value);
            }
        } break;
        case PropertyDescType::uint_t: {
            uint64_t value = std::get<uint64_t>(desc.binding.getter());
            if (ImGui::InputScalar(desc.name.c_str(), ImGuiDataType_U64, &value)) {
                desc.binding.setter(value);
            }
        } break;
        case PropertyDescType::float_t: {
            double value = std::get<double>(desc.binding.getter());
            if (ImGui::InputDouble(desc.name.c_str(), &value)) {
                desc.binding.setter(value);
            }
        } break;
        case PropertyDescType::string_t: {
            if (!m_stringPool.contains(key)) {
                m_stringPool[key] = std::get<std::string>(desc.binding.getter());
            }
            if (Widgets::TextBox(desc.name, m_stringPool[key])) {
                desc.binding.setter(m_stringPool[key]);
            }
        } break;
        case PropertyDescType::enum_t: {
            if (!m_enumPool.contains(key)) {
                const auto &constraints = std::get<EnumConstraints>(desc.constraints);
                m_enumPool[key] = constraints.labels;
            }
            std::string currentValue = std::get<std::string>(desc.binding.getter());
            if (Widgets::ComboBox(desc.name, currentValue, m_enumPool[key])) {
                desc.binding.setter(currentValue);
            }
        } break;
        case PropertyDescType::color_t: {
            glm::vec4 value = std::get<glm::vec4>(desc.binding.getter());
            if (ImGui::ColorEdit4(desc.name.c_str(), glm::value_ptr(value))) {
                desc.binding.setter(value);
            }
        } break;
        default:
            throw std::runtime_error(std::format("Unsupported property type {} for draw",
                                                 static_cast<int>(desc.type)));
        }
    }
} // namespace Bess::UI::Hook
