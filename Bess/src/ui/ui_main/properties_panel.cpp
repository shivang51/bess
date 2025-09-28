#include "ui/ui_main/properties_panel.h"
#include "common/helpers.h"
#include "gtc/type_ptr.hpp"
#include "imgui_internal.h"
#include "scene/components/components.h"
#include "scene/components/non_sim_comp.h"
#include "scene/scene.h"
#include "scene/artist/base_artist.h"
#include "ui/icons/FontAwesomeIcons.h"
#include "ui/m_widgets.h"
#include <imgui.h>

using namespace Bess::Canvas::Components;

namespace Bess::UI {
    bool PropertiesPanel::isShown = true;

    bool drawTagComponent(TagComponent &comp) {
        std::string icon;
        if (comp.isSimComponent) {
            icon = Common::Helpers::getComponentIcon(comp.type.simCompType);
        } else {
            icon = Common::Helpers::getComponentIcon(comp.type.nsCompType);
        }
        return MWidgets::TextBox("Name", comp.name);
    }

    bool MyCollapsingHeader(const char *label) {
        ImGuiContext &g = *ImGui::GetCurrentContext();
        ImGuiWindow *window = g.CurrentWindow;

        ImGuiID id = window->GetID(label);
        ImVec2 pos = window->DC.CursorPos;
        ImRect bb(pos, ImVec2(pos.x + ImGui::GetContentRegionAvail().x, pos.y + g.FontSize + g.Style.FramePadding.y * 2));
        bool opened = ImGui::TreeNodeBehaviorIsOpen(id, ImGuiTreeNodeFlags_DefaultOpen);
        bool hovered, held;

        auto style = ImGui::GetStyle();
        auto rounding = style.FrameRounding;

        if (ImGui::ButtonBehavior(bb, id, &hovered, &held, ImGuiButtonFlags_PressedOnClick))
            window->DC.StateStorage->SetInt(id, opened ? 0 : 1);
        if (hovered || held)
            window->DrawList->AddRectFilled(bb.Min, bb.Max, ImGui::GetColorU32(held ? ImGuiCol_HeaderActive : ImGuiCol_HeaderHovered), rounding);

        // Icon, text
        float button_sz = g.FontSize;
        pos.x += rounding / 2.f;
        auto icon = opened ? Icons::FontAwesomeIcons::FA_CARET_DOWN : Icons::FontAwesomeIcons::FA_CARET_RIGHT;
        ImGui::RenderText(ImVec2(pos.x + g.Style.ItemInnerSpacing.x, pos.y + g.Style.FramePadding.y), icon);
        ImGui::RenderText(ImVec2(pos.x + button_sz + g.Style.ItemInnerSpacing.x, pos.y + g.Style.FramePadding.y), label);

        ImGui::ItemSize(bb, g.Style.FramePadding.y);
        ImGui::ItemAdd(bb, id);

        if (opened) {
            ImGui::Indent();
        }
        return opened;
    }

    void drawSimulationOutputComponent(SimulationOutputComponent &comp) {
        ImGui::Spacing();
        MWidgets::CheckboxWithLabel("Record Output", &comp.recordOutput);
    }

    void drawSimulationInputComponent(SimulationInputComponent &comp, const UUID &uuid) {
        if (MyCollapsingHeader("Input Behaviour")) {
            if (MWidgets::CheckboxWithLabel("Clocked", &comp.clockBhaviour)) {
                comp.updateClock(uuid);
            }
            if (comp.clockBhaviour) {

                if (ImGui::SliderFloat("Frequency", &comp.frequency, 0.1f, 3.0f, "%.1f Hz", ImGuiSliderFlags_AlwaysClamp)) {
                    constexpr float stepSize = 0.1f;
                    comp.frequency = roundf(comp.frequency / stepSize) * stepSize; // Force step increments
                    comp.updateClock(uuid);
                }

                static std::vector<std::string> frequencies = {"Hz", "kHz", "MHz"};
                std::string currFreq = frequencies[(int)comp.frequencyUnit];
                if (UI::MWidgets::ComboBox("Unit", currFreq, frequencies)) {
                    auto idx = std::distance(frequencies.begin(), std::find(frequencies.begin(), frequencies.end(), currFreq));
                    comp.frequencyUnit = static_cast<SimEngine::FrequencyUnit>(idx);
                    comp.updateClock(uuid);
                }
            }
            ImGui::Unindent();
        }
    }

    void drawSpriteComponent(SpriteComponent &comp) {
        ImGui::Indent();
        ImGui::ColorEdit4("Color", glm::value_ptr(comp.color));
        ImGui::Unindent();
    }

    void drawTextNodeComponent(TextNodeComponent &comp) {
        ImGui::Indent();
        MWidgets::TextBox("Text", comp.text);
        ImGui::SliderFloat("Font Size", &comp.fontSize, 1.0f, 100.0f, "%.1f");
        ImGui::ColorEdit4("Color", glm::value_ptr(comp.color));
        ImGui::Unindent();
    }

    void PropertiesPanel::draw() {
        if (!isShown)
            return;

        ImGui::Begin(windowName.data(), nullptr, ImGuiWindowFlags_NoFocusOnAppearing);

        auto &registry = Canvas::Scene::instance().getEnttRegistry();
        const auto view = registry.view<SelectedComponent>();

        if (view.size() == 0) {
            ImGui::Text("No Component Selected");
            ImGui::End();
            return;
        }

        const auto entt = view.front();
        if (!registry.valid(entt)) {
            ImGui::End();
            return;
        }

        if (registry.all_of<TagComponent>(entt)) {
            if(drawTagComponent(registry.get<TagComponent>(entt)) && registry.all_of<SimulationComponent>(entt)) {
                auto& transform = registry.get<TransformComponent>(entt);
                transform.scale = Canvas::BaseArtist::calcCompSize(entt,
                    registry.get<SimulationComponent>(entt),
                    registry.get<TagComponent>(entt).name);
            }
        }

        if (registry.all_of<SimulationOutputComponent>(entt)) {
            drawSimulationOutputComponent(registry.get<SimulationOutputComponent>(entt));
        }

        if (registry.all_of<SimulationInputComponent>(entt)) {
            const auto simulationComp = registry.get<SimulationComponent>(entt);
            drawSimulationInputComponent(registry.get<SimulationInputComponent>(entt), simulationComp.simEngineEntity);
        }

        if (registry.all_of<ConnectionComponent>(entt)) {
            auto &connectionComponent = registry.get<ConnectionComponent>(entt);
            MWidgets::CheckboxWithLabel("Use Custom Color", &connectionComponent.useCustomColor);
            if (connectionComponent.useCustomColor) {
                drawSpriteComponent(registry.get<SpriteComponent>(entt));
            }
        }

        if (registry.all_of<TextNodeComponent>(entt)) {
            drawTextNodeComponent(registry.get<TextNodeComponent>(entt));
        }

        ImGui::End();
    }
} // namespace Bess::UI
