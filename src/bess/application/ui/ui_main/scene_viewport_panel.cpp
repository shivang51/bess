#include "scene_viewport_panel.h"
#include "bess_core/g_app_context.h"
#include "bess_core/project_context.h"
#include "bess_core/renderer/renderer_2d.h"
#include "bess_core/scene/camera.h"
#include "bess_core/scene/scene.h"
#include "bess_core/scene/scene_draw_context.h"
#include "bess_core/scene_driver.h"
#include "bess_wgpu/wgpu_texture.h"
#include "common/bess_uuid.h"
#include "imgui.h"
#include "sub_systems/renderer_context.h"
#include "ui/ui_main/component_explorer.h"
#include "ui/ui_main/ui_main.h"
#include "ui/ui_panel.h"

namespace Bess::UI {
    SceneViewportPanel::SceneViewportPanel(const std::string &viewportName)
        : Panel(viewportName),
          m_viewportName(viewportName) {
        static size_t viewportCounter = 0;
        m_viewportCtx = std::make_shared<Core::Viewport::ViewportContext>();
        m_viewportCtx->viewportId = viewportCounter++;
    }

    void SceneViewportPanel::init() {
        m_defaultDock = Dock::top;
        m_showInMenuBar = false;
        m_visible = true;

        m_sceneTexture = std::make_shared<Wgpu::WgpuTexture>(
            Core::Renderer::TextureCreateInfo{});
        m_sceneTexture->setSize({800.f, 600.f});
        m_sceneTexture->init();

        m_pickingTexture = std::make_shared<Wgpu::WgpuTexture>(
            Core::Renderer::TextureCreateInfo{
                .format = Core::Renderer::Renderer2DTargetFormat::RG32Uint});
        m_pickingTexture->setSize({800.f, 600.f});
        m_pickingTexture->init();

        m_camera = std::make_shared<Camera>(800.f, 600.f);
    }

    void SceneViewportPanel::update(TimeMs ts) {
        auto sceneDriver = GAppContext::getInstance()
                               .getSubSystem<Bess::ProjectContext>()
                               ->getSubSystem<SceneDriver>();
        const auto activeScene = sceneDriver->getActiveScene();
        if (!m_attachedScene ||
            sceneDriver->getSceneWithId(m_attachedScene->getSceneId()) !=
                m_attachedScene) {
            setAttachedScene(activeScene);
        }

        if (!m_camera) {
            m_camera =
                std::make_shared<Camera>(m_viewportCtx->transform.size.x,
                                         m_viewportCtx->transform.size.y);
        }

        if (m_viewportCtx->updateSceneId != UUID::null) {
            setAttachedScene(
                sceneDriver->getSceneWithId(m_viewportCtx->updateSceneId));
            m_viewportCtx->updateSceneId = UUID::null;
        }

        if (!m_attachedScene) {
            setAttachedScene(activeScene);
        }

        if (!m_attachedScene) {
            return;
        }

        if (m_isResized) {
            m_camera->resize(m_viewportCtx->transform.size.x,
                             m_viewportCtx->transform.size.y);
            if (m_sceneTexture) {
                m_sceneTexture->setSize(m_viewportCtx->transform.size);
                m_sceneTexture->destroy();
                m_sceneTexture->init();
            }
            if (m_pickingTexture) {
                m_pickingTexture->setSize(m_viewportCtx->transform.size);
                m_pickingTexture->destroy();
                m_pickingTexture->init();
            }
            m_pendingSelectionReadback.clear();
            m_isResized = false;
        }

        m_camera->update(ts);
        m_viewportCtx->isFocused = m_isHovered;

        if (m_viewportCtx->isFocused && m_attachedScene &&
            m_attachedScene != sceneDriver->getActiveScene()) {
            sceneDriver->setActiveScene(m_attachedScene->getSceneId());
            m_viewportCtx->inputCtx.pickingId = PickingId::invalid();
        }

        Canvas::ViewportUpdateContext ctx{
            .isFocused = m_isHovered,
            .camera = m_camera,
            .viewportCtx = m_viewportCtx,
            .renderer = GAppContext::getInstance()
                            .getSubSystem<RendererContext>()
                            ->getRenderer(),
        };

        if (!sceneDriver->getIsPaused()) {
            m_attachedScene->viewportUpdate(ts, ctx);
            updateScene(ts);
        }

        if (m_nextSceneId != UUID::null) {
            GAppContext::getInstance()
                .getSubSystem<Bess::ProjectContext>()
                ->getSubSystem<SceneDriver>()
                ->setActiveScene(m_nextSceneId);
            m_nextSceneId = UUID::null;
        }
    }

    void SceneViewportPanel::destroy() {
        if (m_gridShader != 0) {
            const auto renderer = GAppContext::getInstance()
                                      .getSubSystem<RendererContext>()
                                      ->getRenderer();
            if (renderer != nullptr) {
                if (m_gridShader != 0) {
                    renderer->destroyCustomQuadShader(m_gridShader);
                }
            }
            m_gridShader = 0;
        }
        if (m_sceneTexture != nullptr) {
            m_sceneTexture->destroy();
            m_sceneTexture = nullptr;
        }
        if (m_pickingTexture != nullptr) {
            m_pickingTexture->destroy();
            m_pickingTexture = nullptr;
        }

        m_viewportCtx->reset();
    }

    void SceneViewportPanel::onBeforeDraw() {

        auto sceneDriver = GAppContext::getInstance()
                               .getSubSystem<Bess::ProjectContext>()
                               ->getSubSystem<SceneDriver>();
        if (!sceneDriver->getIsPaused()) {
            renderAttachedScene();
        }

        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
        ImGui::SetNextWindowSizeConstraints({400.f, 400.f}, {-1.f, -1.f});
    }

    void SceneViewportPanel::onDraw() {
        auto sceneDriver = GAppContext::getInstance()
                               .getSubSystem<Bess::ProjectContext>()
                               ->getSubSystem<SceneDriver>();

        const auto scene = sceneDriver->getActiveScene();

        const auto viewportPanelSize = ImGui::GetContentRegionAvail();
        if (viewportPanelSize.x != m_viewportCtx->transform.size.x ||
            viewportPanelSize.y != m_viewportCtx->transform.size.y) {
            m_isResized = true;
            m_viewportCtx->transform.size = {viewportPanelSize.x,
                                             viewportPanelSize.y};
        }

        const auto offset = ImGui::GetCursorPos();
        if (m_sceneTexture) {
            ImGui::Image((ImTextureRef)m_sceneTexture->getView(),
                         ImVec2(viewportPanelSize.x, viewportPanelSize.y));
        } else {
            ImGui::SetCursorPos({100, 100});
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.f, 0.f, 0.f, 1.f));
            ImGui::Text("No valid scene texture attached.");
            ImGui::PopStyleColor();
        }

        const auto gPos = ImGui::GetMainViewport()->Pos;
        m_localPos = ImGui::GetWindowPos();
        m_viewportCtx->transform.pos = {m_localPos.x + gPos.x + offset.x,
                                        m_localPos.y + gPos.y + offset.y};

        ImGui::PopStyleVar();

        const auto &pickingId = m_viewportCtx->inputCtx.pickingId;
        if (!pickingId.isValid() && ImGui::BeginPopupContextWindow()) {
            if (ImGui::MenuItem("Add Component", "Shift-A")) {
                UI::UIMain::getPanel<ComponentExplorer>()->show();
            }

            ImGui::EndPopup();
        }
    }

    bool SceneViewportPanel::isHovered() const {
        return m_isHovered;
    }

    const glm::vec2 &SceneViewportPanel::getViewportPos() const {
        return m_viewportCtx->transform.pos;
    }

    const glm::vec2 &SceneViewportPanel::getViewportSize() const {
        return m_viewportCtx->transform.size;
    }

    void SceneViewportPanel::onSceneAttached() {
        m_viewportCtx->reset();
        m_pendingSelectionReadback.clear();
    }

    glm::vec2 SceneViewportPanel::getSceneMousePos() {
        return m_camera->toWorldPos(m_viewportCtx->inputCtx.mousePos);
    }

    bool SceneViewportPanel::isFocused() const {
        return m_isHovered;
    }

    bool SceneViewportPanel::isAttachedToScene(
        const std::shared_ptr<Canvas::Scene> &scene) const {
        return scene && m_attachedScene &&
               scene->getSceneId() == m_attachedScene->getSceneId();
    }

    void SceneViewportPanel::focusCameraOnSelected() {
        if (!m_attachedScene || !m_camera) {
            return;
        }

        m_attachedScene->focusCameraOnSelected(m_camera);
    }

    std::shared_ptr<Camera> SceneViewportPanel::getCamera() const {
        return m_camera;
    }

    void SceneViewportPanel::updateAttachedSceneId(const UUID &sceneId) {
        m_viewportCtx->updateSceneId = sceneId;
    }

} // namespace Bess::UI
