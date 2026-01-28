#include "scene_viewport.h"
#include "imgui.h"
#include "imgui_internal.h"
#include "scene/camera.h"
#include "scene/scene.h"
#include "simulation_engine.h"
#include "ui/ui_main/component_explorer.h"

namespace Bess::UI {
    SceneViewport::SceneViewport(const std::string &viewportName)
        : m_viewportName(viewportName) {}

    void SceneViewport::draw() {

        const auto scene = Canvas::Scene::instance();

        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
        ImGui::SetNextWindowSizeConstraints({400.f, 400.f}, {-1.f, -1.f});

        ImGui::Begin(m_viewportName.c_str(), nullptr, NO_MOVE_FLAGS);

        const auto viewportPanelSize = ImGui::GetContentRegionAvail();
        m_viewportSize = {viewportPanelSize.x, viewportPanelSize.y};

        const auto offset = ImGui::GetCursorPos();
        if (m_viewportTexture) {
            ImGui::Image((ImTextureRef)m_viewportTexture,
                         ImVec2(viewportPanelSize.x, viewportPanelSize.y), ImVec2(0, 1),
                         ImVec2(1, 0));
        } else {
            ImGui::SetCursorPos({100, 100});
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.f, 0.f, 0.f, 1.f));
            ImGui::Text("No valid scene texture attached.");
            ImGui::PopStyleColor();
        }

        const auto gPos = ImGui::GetMainViewport()->Pos;

        m_localPos = ImGui::GetWindowPos();
        m_viewportPos = {m_localPos.x - gPos.x + offset.x,
                         m_localPos.y - gPos.y + offset.y};
        m_isHovered = ImGui::IsWindowHovered();

        ImGui::PopStyleVar();

        if (!scene->isHoveredEntityValid() && ImGui::BeginPopupContextWindow()) {
            if (ImGui::MenuItem("Add Component", "Shift-A")) {
                ComponentExplorer::isShown = true;
            }

            ImGui::EndPopup();
        }

        ImGui::End();

        drawTopLeftControls();
        drawBottomControls();
    }

    void SceneViewport::drawTopLeftControls() const {
        const ImGuiContext &g = *ImGui::GetCurrentContext();
        const auto colors = g.Style.Colors;
        auto &simEngine = SimEngine::SimulationEngine::instance();
        static float checkboxWidth = ImGui::CalcTextSize("W").x + g.Style.FramePadding.x + 2.f;
        const auto textSize = ImGui::CalcTextSize("Schematic Mode");
        static float size = textSize.x + checkboxWidth + 12.f;
        ImGui::SetNextWindowPos({m_localPos.x + g.Style.FramePadding.x,
                                 m_localPos.y + g.Style.FramePadding.y});
        ImGui::SetNextWindowSize({size, 0});
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(4, 4));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0);

        auto col = colors[ImGuiCol_ButtonActive];
        col.w = 0.2f;
        ImGui::PushStyleColor(ImGuiCol_WindowBg, col);
        ImGui::Begin("TopLeftViewportActions", nullptr, NO_MOVE_FLAGS);
        ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 2);
        ImGui::Text("Schematic Mode");
        ImGui::SameLine();
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(0, 0));
        ImGui::Checkbox("##CheckBoxSchematicMode", Canvas::Scene::instance()->getIsSchematicViewPtr());
        ImGui::PopStyleVar();
        ImGui::End();
        ImGui::PopStyleVar(2);
        ImGui::PopStyleColor(1);
    }

    void SceneViewport::drawBottomControls() const {
        const auto &mousePos = Canvas::Scene::instance()->getSceneMousePos();
        const auto posLabel = std::format("Pos: ({:.2f}, {:.2f})", mousePos.x, mousePos.y);
        const auto posLabelSize = ImGui::CalcTextSize(posLabel.c_str());

        float windowWidth = posLabelSize.x + ImGui::GetStyle().ItemSpacing.x + 150.0f + 20.f;

        ImGui::SetNextWindowPos({m_localPos.x + m_viewportSize.x - windowWidth - 10.f,
                                 m_localPos.y + m_viewportSize.y - 44.f});
        ImGui::SetNextWindowBgAlpha(0.f);

        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0);

        // Force height 34
        ImGui::SetNextWindowSize({0, 34});
        ImGui::Begin("SceneBottomRightControls", nullptr, NO_MOVE_FLAGS | ImGuiWindowFlags_NoScrollbar);

        const float windowHeight = 34.0f;
        const float sliderHeight = ImGui::GetFrameHeight();

        ImGui::SetCursorPosY((windowHeight - sliderHeight) * 0.5f);
        const auto &camera = Canvas::Scene::instance()->getCamera();

        // Mouse Pos Text
        {
            ImGui::AlignTextToFramePadding();
            ImGui::TextUnformatted(posLabel.c_str());

            // Recenter on click
            if (ImGui::IsItemClicked(ImGuiMouseButton_Left)) {
                camera->focusAtPoint({0.f, 0.f}, false);
            }
        }

        ImGui::SameLine();

        // Zoom Slider
        {
            ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 8);
            ImGui::PushStyleVar(ImGuiStyleVar_GrabRounding, 8);

            ImGui::SetNextItemWidth(150.0f);
            if (ImGui::SliderFloat("##Zoom", &camera->getZoomRef(), Camera::zoomMin,
                                   Camera::zoomMax, "%.1fx", ImGuiSliderFlags_AlwaysClamp)) {
                const float stepSize = 0.1f;
                const float val = roundf(camera->getZoom() / stepSize) * stepSize;
                camera->setZoom(val);
            }
            ImGui::PopStyleVar(2);
        }

        ImGui::End();
        ImGui::PopStyleVar(2);
    }

    void SceneViewport::firstTime() {
    }

    void SceneViewport::setViewportTexture(const uint64_t texture) {
        m_viewportTexture = texture;
    }

    bool SceneViewport::isHovered() const {
        return m_isHovered;
    }

    const glm::vec2 &SceneViewport::getViewportPos() const {
        return m_viewportPos;
    }

    const glm::vec2 &SceneViewport::getViewportSize() const {
        return m_viewportSize;
    }
} // namespace Bess::UI
