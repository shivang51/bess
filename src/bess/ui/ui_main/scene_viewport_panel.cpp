#include "scene_viewport_panel.h"
#include "common/logger.h"
#include "imgui.h"
#include "imgui_internal.h"
#include "pages/main_page/main_page.h"
#include "scene/camera.h"
#include "scene/scene_draw_context.h"
#include "ui/icons/FontAwesomeIcons.h"
#include "ui/ui_main/component_explorer.h"
#include "ui_main/ui_main.h"
#include "ui_panel.h"
#include "vulkan_core.h"

namespace Bess::UI {
    SceneViewportPanel::SceneViewportPanel(const std::string &viewportName)
        : Panel(viewportName), m_viewportName(viewportName) {}

    void SceneViewportPanel::init() {
        auto &vkCore = Vulkan::VulkanCore::instance();
        m_viewport = std::make_shared<Canvas::Viewport>(vkCore.getDevice(),
                                                        vkCore.getSwapchain()->imageFormat(),
                                                        vec2Extent2D(m_viewportSize));

        m_flags = NO_MOVE_FLAGS | ImGuiWindowFlags_NoFocusOnAppearing;
        m_defaultDock = Dock::main;
        m_showInMenuBar = false;
        m_visible = true;
    }

    void SceneViewportPanel::update(TimeMs ts, const std::vector<ApplicationEvent> &events) {
        if (m_isResized) {
            m_viewport->resize(vec2Extent2D(m_viewportSize));
            m_attachedScene->getCamera()->resize(m_viewportSize.x, m_viewportSize.y);
            m_isResized = false;
        }

        if (m_isHovered) {
            m_attachedScene->update(ts, events);
            updateScene(ts, events);
        } else {
            m_attachedScene->update(ts, {});
        }
    }

    void SceneViewportPanel::destroy() {
        if (m_viewport) {
            destroyViewport();
        }
    }

    void SceneViewportPanel::onBeforeDraw() {

        renderAttachedScene();

        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
        ImGui::SetNextWindowSizeConstraints({400.f, 400.f}, {-1.f, -1.f});
    }

    void SceneViewportPanel::onDraw() {
        auto &scene = Pages::MainPage::getInstance()->getState().getSceneDriver();

        const auto viewportPanelSize = ImGui::GetContentRegionAvail();
        if (viewportPanelSize.x != m_viewportSize.x ||
            viewportPanelSize.y != m_viewportSize.y) {
            m_isResized = true;
            m_viewportSize = {viewportPanelSize.x, viewportPanelSize.y};
        }

        const auto offset = ImGui::GetCursorPos();
        if (m_viewport->getViewportTexture() != 0) {
            ImGui::Image((ImTextureRef)m_viewport->getViewportTexture(),
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

        ImGui::PopStyleVar();

        if (!scene->isHoveredEntityValid() && ImGui::BeginPopupContextWindow()) {
            if (ImGui::MenuItem("Add Component", "Shift-A")) {
                UI::UIMain::getPanel<ComponentExplorer>()->show();
            }

            ImGui::EndPopup();
        }
    }

    void SceneViewportPanel::onAfterDraw() {
        drawTopLeftControls();
        drawBottomControls();
    }

    void SceneViewportPanel::drawTopLeftControls() const {
        constexpr float windowR = 16.f;

        const ImGuiContext &g = *ImGui::GetCurrentContext();

        static float checkboxWidth = ImGui::CalcTextSize("W").x + g.Style.FramePadding.x + 2.f;
        static const auto textSize = ImGui::CalcTextSize("Schematic Mode");
        static float size = textSize.x + checkboxWidth + (windowR * 2);

        const auto colors = g.Style.Colors;

        // Is schematic mode
        ImGui::SetNextWindowPos({m_localPos.x + g.Style.FramePadding.x,
                                 m_localPos.y + g.Style.FramePadding.y});
        ImGui::SetNextWindowSize({size, 0});
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(windowR, 4));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, windowR);

        auto col = colors[ImGuiCol_ButtonActive];
        col.w = 0.2f;
        ImGui::PushStyleColor(ImGuiCol_WindowBg, col);
        ImGui::Begin("TopLeftViewportActions", nullptr, NO_MOVE_FLAGS);
        ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 2);
        ImGui::Text("Schematic Mode");
        ImGui::SameLine();
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(0, 0));

        auto &scene = Pages::MainPage::getInstance()->getState().getSceneDriver();
        ImGui::Checkbox("##CheckBoxSchematicMode", scene->getIsSchematicViewPtr());
        ImGui::PopStyleVar();
        ImGui::End();
        ImGui::PopStyleColor(1);

        // Scene path (root > module ...)
        ImGui::SetNextWindowPos({m_localPos.x + g.Style.FramePadding.x + size + 8.f,
                                 m_localPos.y + g.Style.FramePadding.y});
        ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0, 0, 0, 0));
        ImGui::SetNextWindowSize({0, 0});
        ImGui::Begin("TopLeftViewportActions1", nullptr, NO_MOVE_FLAGS);
        if (m_attachedScene->getState().getIsRootScene()) {
            ImGui::AlignTextToFramePadding();
            ImGui::TextDisabled("Root");
        } else {
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));
            if (ImGui::Button("Root")) {
                Pages::MainPage::getInstance()->getState().getSceneDriver().makeRootSceneActive();
            }
            ImGui::PopStyleColor(1);
            ImGui::SameLine();
            ImGui::AlignTextToFramePadding();
            ImGui::TextDisabled(Icons::FontAwesomeIcons::FA_CHEVRON_RIGHT);
            ImGui::SameLine();
            ImGui::AlignTextToFramePadding();
            ImGui::TextDisabled(" Module");
        }
        ImGui::End();
        ImGui::PopStyleColor(1);

        ImGui::PopStyleVar(3);
    }

    void SceneViewportPanel::drawBottomControls() const {
        auto &scene = Pages::MainPage::getInstance()->getState().getSceneDriver();
        const auto &mousePos = scene->getSceneMousePos();
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
        const auto &camera = scene->getCamera();

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

    void SceneViewportPanel::firstTime() {
    }

    bool SceneViewportPanel::isHovered() const {
        return m_isHovered;
    }

    const glm::vec2 &SceneViewportPanel::getViewportPos() const {
        return m_viewportPos;
    }

    const glm::vec2 &SceneViewportPanel::getViewportSize() const {
        return m_viewportSize;
    }

    VkExtent2D SceneViewportPanel::vec2Extent2D(const glm::vec2 &vec) {
        return {(uint32_t)vec.x, (uint32_t)vec.y};
    }

    void SceneViewportPanel::destroyViewport() {
        m_viewport.reset();
    }

    void SceneViewportPanel::onSceneAttached() {
        BESS_DEBUG("[SceneVewportPanel] Scene {} attached to viewport panel '{}'",
                   (uint64_t)m_attachedScene->getState().getSceneId(), m_viewportName);

        m_attachedScene->getCamera()->resize(m_viewportSize.x, m_viewportSize.y);
        m_viewport->setCamera(m_attachedScene->getCamera());
    }
} // namespace Bess::UI
