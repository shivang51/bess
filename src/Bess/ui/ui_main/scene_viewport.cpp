#include "scene_viewport.h"
#include "imgui.h"
#include "imgui_internal.h"
#include "scene/camera.h"
#include "scene/scene.h"
#include "simulation_engine.h"
#include "ui/icons/FontAwesomeIcons.h"
#include "ui/icons/MaterialIcons.h"
#include "ui/ui_main/component_explorer.h"

namespace Bess::UI {
    SceneViewport::SceneViewport(const std::string &viewportName)
        : m_viewportName(viewportName) {}

    void SceneViewport::draw() {
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
        ImGui::SetNextWindowSizeConstraints({400.f, 400.f}, {-1.f, -1.f});

        ImGui::Begin(m_viewportName.c_str(), nullptr, NO_MOVE_FLAGS);

        auto viewportPanelSize = ImGui::GetContentRegionAvail();
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

        const auto pos = ImGui::GetWindowPos();
        const auto gPos = ImGui::GetMainViewport()->Pos;
        m_viewportPos = {pos.x - gPos.x + offset.x, pos.y - gPos.y + offset.y};
        m_isHovered = ImGui::IsWindowHovered();
        ImGui::PopStyleVar();

        if (ImGui::BeginPopupContextWindow()) {
            if (ImGui::MenuItem("Add Component", "Shift-A")) {
                ComponentExplorer::isShown = true;
            }

            ImGui::EndPopup();
        }

        ImGui::End();

        drawTopLeftControls();
        drawTopRightControls();
        drawBottomControls();
    }

    void SceneViewport::drawTopLeftControls() const {
        const ImGuiContext &g = *ImGui::GetCurrentContext();
        const auto colors = g.Style.Colors;
        auto &simEngine = SimEngine::SimulationEngine::instance();
        static float checkboxWidth = ImGui::CalcTextSize("W").x + g.Style.FramePadding.x + 2.f;
        static float size = ImGui::CalcTextSize("Schematic Mode").x + checkboxWidth + 12.f;
        ImGui::SetNextWindowPos({m_viewportPos.x + g.Style.FramePadding.x,
                                 m_viewportPos.y + g.Style.FramePadding.y});
        ImGui::SetNextWindowSize({size, 0});
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(4, 4));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0);

        auto col = colors[ImGuiCol_Header];
        col.w = 0.1f;
        ImGui::PushStyleColor(ImGuiCol_WindowBg, col);
        ImGui::Begin("TopLeftViewportActions", nullptr, NO_MOVE_FLAGS);
        ImGui::Text("Schematic Mode");
        ImGui::SameLine();
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(0, 0));
        ImGui::Checkbox("##CheckBoxSchematicMode", Canvas::Scene::instance()->getIsSchematicViewPtr());
        ImGui::PopStyleVar();
        ImGui::End();
        ImGui::PopStyleVar(2);
        ImGui::PopStyleColor(1);
    }

    void SceneViewport::drawTopRightControls() const {
        const ImGuiContext &g = *ImGui::GetCurrentContext();
        const auto colors = g.Style.Colors;
        auto &simEngine = SimEngine::SimulationEngine::instance();
        static int n = 4; // number of action buttons
        static float size = (float)(32 * n) - (float)(n - 1);
        ImGui::SetNextWindowPos(
            {m_viewportPos.x + m_viewportSize.x - size - g.Style.FramePadding.x,
             m_viewportPos.y + g.Style.FramePadding.y});
        ImGui::SetNextWindowSize({size, 0});
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(4, 4));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0);

        auto col = colors[ImGuiCol_Header];
        col.w = 0.5;
        ImGui::PushStyleColor(ImGuiCol_WindowBg, col);
        ImGui::Begin("TopRightViewportActions", nullptr, NO_MOVE_FLAGS);

        auto scene = Canvas::Scene::instance();

        // scene modes
        {

            const bool isGeneral = scene->getSceneMode() == Canvas::SceneMode::general;

            // general mode
            if (isGeneral) {
                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4{1.f, 0.667f, 0.f, 1.f});
            }

            auto icon = Icons::FontAwesomeIcons::FA_MOUSE_POINTER;
            if (ImGui::Button(icon)) {
                scene->setSceneMode(Canvas::SceneMode::general);
            }
            if (isGeneral) {
                ImGui::PopStyleColor();
            }

            if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) {
            }

            // move mode
            if (!isGeneral) {
                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4{1.f, 0.667f, 0.f, 1.f});
            }
            ImGui::SameLine();
            icon = Icons::FontAwesomeIcons::FA_ARROWS_ALT;
            if (ImGui::Button(icon)) {
                scene->setSceneMode(Canvas::SceneMode::move);
            }

            if (!isGeneral) {
                ImGui::PopStyleColor();
            }

            if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) {
                const auto msg = "Move Mode";
                ImGui::SetTooltip("%s", msg);
            }
        }

        const auto isSimPaused = simEngine.getSimulationState() == SimEngine::SimulationState::paused;

        // Play / Pause
        {

            auto icon = Icons::FontAwesomeIcons::FA_PAUSE;
            if (isSimPaused) {
                // #574735
                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4{1.f, 0.667f, 0.f, 1.f});
                icon = Icons::FontAwesomeIcons::FA_PLAY;
            }

            ImGui::SameLine();
            if (ImGui::Button(icon)) {
                simEngine.toggleSimState();
            }

            if (isSimPaused)
                ImGui::PopStyleColor();

            if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) {
                const auto msg = isSimPaused ? "Resume Simulation" : "Pause Simulation";
                ImGui::SetTooltip("%s", msg);
            }
        }

        ImGui::SameLine();

        // Step when paused
        {
            ImGui::BeginDisabled(!isSimPaused);

            if (ImGui::Button(Icons::MaterialIcons::REDO)) {
                simEngine.stepSimulation();
            }

            if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) {
                ImGui::SetTooltip("%s", "Step");
            }
            ImGui::EndDisabled();
        }

        ImGui::End();
        ImGui::PopStyleVar(2);
        ImGui::PopStyleColor(1);
    }

    void SceneViewport::drawBottomControls() const {
        // Camera controls (on bottom right)
        const auto &mousePos = Canvas::Scene::instance()->getSceneMousePos();
        const auto posLabel = std::format("X:{:.2f}, Y:{:.2f}", mousePos.x, mousePos.y);
        const auto posLabelCStr = posLabel.c_str();
        const auto posLabelSize = ImGui::CalcTextSize(posLabelCStr);

        ImGui::SetNextWindowPos(
            {m_viewportPos.x + m_viewportSize.x - 208 - posLabelSize.x,
             m_viewportPos.y + m_viewportSize.y - 40});
        ImGui::SetNextWindowBgAlpha(0.f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(4, 4));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0);

        ImGui::Begin("SceneBottomRightControls", nullptr, NO_MOVE_FLAGS);
        // Mouse Pos
        {
            ImGui::Text("%s", posLabelCStr);
            ImGui::SameLine();
        }

        ImGui::SameLine();

        // Zoom Slider
        {
            const auto &camera = Canvas::Scene::instance()->getCamera();
            ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 8);
            ImGui::PushStyleVar(ImGuiStyleVar_GrabRounding, 8);
            ImGui::SetNextItemWidth(150.0f);
            if (ImGui::SliderFloat("Zoom", &camera->getZoomRef(), Camera::zoomMin,
                                   Camera::zoomMax, nullptr,
                                   ImGuiSliderFlags_AlwaysClamp)) {
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
