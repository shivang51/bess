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
#include "common/logger.h"
#include "imgui.h"
#include "pages/main_page/main_page.h"
#include "sub_systems/renderer_context.h"
#include "ui/ui_main/component_explorer.h"
#include "ui/ui_main/ui_main.h"
#include "ui/ui_panel.h"
#include <cstdint>

namespace Bess::UI {
    SceneViewportPanel::SceneViewportPanel(const std::string &viewportName)
        : Panel(viewportName),
          m_viewportName(viewportName) {
        static size_t viewportCounter = 0;
        m_viewportCtx.viewportId = viewportCounter++;
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
        if (!m_attachedScene || !m_camera) {
            return;
        }

        if (m_updateAttachedScene) {
            setAttachedScene(m_updateAttachedScene);
            m_updateAttachedScene = nullptr;
        }

        if (m_isResized) {
            m_camera->resize(m_viewportSize.x, m_viewportSize.y);
            if (m_sceneTexture) {
                m_sceneTexture->setSize(m_viewportSize);
                m_sceneTexture->destroy();
                m_sceneTexture->init();
            }
            if (m_pickingTexture) {
                m_pickingTexture->setSize(m_viewportSize);
                m_pickingTexture->destroy();
                m_pickingTexture->init();
            }
            m_pendingSelectionReadback.clear();
            m_isResized = false;
        }

        m_camera->update(ts);

        auto sceneDriver = GAppContext::getInstance()
                               .getSubSystem<Bess::ProjectContext>()
                               ->getSubSystem<SceneDriver>();

        Canvas::ViewportUpdateContext ctx{
            .isFocused = m_isHovered,
            .camera = m_camera,
            .viewportTransform =
                {
                    .pos = m_viewportPos,
                    .size = m_viewportSize,
                },
            .inputState = m_inputState,
            .pickingId = m_pickingId,
            .viewportId = m_viewportCtx.viewportId,
            .isSchematicMode =
                m_viewportCtx.mode == Core::Viewport::ViewportMode::schematic,
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
        m_rootToSceneStatePtrs.clear();
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
        if (viewportPanelSize.x != m_viewportSize.x ||
            viewportPanelSize.y != m_viewportSize.y) {
            m_isResized = true;
            m_viewportSize = {viewportPanelSize.x, viewportPanelSize.y};
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
        m_viewportPos = {m_localPos.x + gPos.x + offset.x,
                         m_localPos.y + gPos.y + offset.y};

        ImGui::PopStyleVar();

        if (!m_pickingId.isValid() && ImGui::BeginPopupContextWindow()) {
            if (ImGui::MenuItem("Add Component", "Shift-A")) {
                UI::UIMain::getPanel<ComponentExplorer>()->show();
            }

            ImGui::EndPopup();
        }
    }

    void SceneViewportPanel::onAfterDraw() {
        // auto sceneDriver = GAppContext::getInstance()
        //                        .getSubSystem<Bess::ProjectContext>()
        //                        ->getSubSystem<SceneDriver>();
        //
        // if (sceneDriver->getIsPaused()) {
        //     return;
        // }

        // drawTopLeftControls();
        // drawBottomControls();
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

    void SceneViewportPanel::onSceneAttached() {
        m_inputState.reset();
        m_pickingId = PickingId::invalid();
        m_pendingSelectionReadback.clear();
        if (m_camera) {
            m_camera->resize(m_viewportSize.x, m_viewportSize.y);
        }

        m_rootToSceneStatePtrs.clear();
        if (!m_attachedScene) {
            return;
        }

        // Very Important: to avoid circular intialization of mainpage,
        // we do this, do not remove this
        if (m_attachedScene->getState().getIsRootScene()) {
            m_rootToSceneStatePtrs.push_back(&m_attachedScene->getState());
            BESS_DEBUG(
                "[SceneVewportPanel] Scene {} attached to viewport panel '{}'",
                (uint64_t)m_attachedScene->getState().getSceneId(),
                m_viewportName);
            return;
        }

        const auto &mainPageState = Pages::MainPage::getInstance()->getState();
        auto sceneDriver = GAppContext::getInstance()
                               .getSubSystem<Bess::ProjectContext>()
                               ->getSubSystem<SceneDriver>();

        UUID sceneId = m_attachedScene->getSceneId();

        while (sceneId != UUID::null) {
            const auto scene = sceneDriver->getSceneWithId(sceneId);
            if (!scene) {
                BESS_ERROR(
                    "[SceneVewportPanel] Scene with id {} not found while "
                    "traversing parent scenes during scene attach.",
                    (uint64_t)sceneId);
                break;
            }
            m_rootToSceneStatePtrs.push_back(&scene->getState());
            sceneId = scene->getState().getParentSceneId();
            if (sceneDriver->getRootSceneId() != scene->getSceneId()) {
                BESS_ASSERT(sceneId != UUID::null,
                            "Non-root scene has null parent scene id.");
            }
        }

        std::ranges::reverse(m_rootToSceneStatePtrs);

        BESS_DEBUG(
            "[SceneVewportPanel] Scene {} attached to viewport panel '{}'",
            (uint64_t)m_attachedScene->getState().getSceneId(),
            m_viewportName);
    }

    glm::vec2 SceneViewportPanel::getSceneMousePos() {
        return m_camera->toWorldPos(m_inputState.mousePos);
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

} // namespace Bess::UI
