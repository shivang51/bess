#include "scene_viewport_panel.h"
#include "bess_core/g_app_context.h"
#include "bess_core/renderer/renderer_2d.h"
#include "bess_core/scene/camera.h"
#include "bess_core/scene/scene.h"
#include "bess_core/scene/scene_draw_context.h"
#include "bess_core/scene_driver.h"
#include "bess_core/sub_systems/input_sub_system.h"
#include "bess_wgpu/wgpu_texture.h"
#include "common/bess_uuid.h"
#include "common/helpers.h"
#include "imgui.h"
#include "imgui_internal.h"
#include "pages/main_page/module_edit.h"
#include "pages/main_page/scene_components/sim_scene_component.h"
#include "project_session/project_session.h"
#include "sub_systems/renderer_context.h"
#include "ui/icons/CodIcons_Remapped.h"
#include "ui/icons/FontAwesomeIcons_Remapped.h"
#include "ui/project_api.h"
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

        m_flags |=
            ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse;
    }

    void SceneViewportPanel::update(TimeMs ts) {
        if (!m_wasRendered)
            return;

        auto sceneDriver = GAppContext::getInstance()
                               .getSubSystem<Bess::ProjectSession>()
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
        m_viewportCtx->isFocused = m_isViewportHovered;

        if (m_viewportCtx->isFocused && m_attachedScene &&
            m_attachedScene != sceneDriver->getActiveScene()) {
            sceneDriver->setActiveScene(m_attachedScene->getSceneId());
            m_viewportCtx->inputCtx.pickingId = PickingId::invalid();
        }

        Canvas::ViewportUpdateContext ctx{
            .isFocused = m_isViewportHovered && m_wasRendered && !m_isResized,
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
            auto scene = GAppContext::getInstance()
                             .getSubSystem<Bess::ProjectSession>()
                             ->getSubSystem<SceneDriver>()
                             ->setActiveScene(m_nextSceneId);
            setAttachedScene(scene);
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
                               .getSubSystem<Bess::ProjectSession>()
                               ->getSubSystem<SceneDriver>();
        if (!sceneDriver->getIsPaused()) {
            renderAttachedScene();
        }

        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
        ImGui::SetNextWindowSizeConstraints({400.f, 400.f}, {-1.f, -1.f});
        ImGui::PushItemFlag(ImGuiItemFlags_NoTabStop, true);
        ImGui::PushItemFlag(ImGuiItemFlags_NoNav, true);
    }

    void SceneViewportPanel::onDraw() {
        if (!m_wasRendered)
            return;

        auto sceneDriver = GAppContext::getInstance()
                               .getSubSystem<Bess::ProjectSession>()
                               ->getSubSystem<SceneDriver>();

        const auto scene = sceneDriver->getActiveScene();

        const auto viewportPanelSizeRaw = ImGui::GetContentRegionAvail();
        const glm::vec2 viewportPanelSize = {
            std::max(1.0f, viewportPanelSizeRaw.x),
            std::max(1.0f, viewportPanelSizeRaw.y)};
        if (viewportPanelSize.x != m_viewportCtx->transform.size.x ||
            viewportPanelSize.y != m_viewportCtx->transform.size.y) {
            m_isResized = true;
            m_viewportCtx->isResized = true;
            m_viewportCtx->transform.size = {viewportPanelSize.x,
                                             viewportPanelSize.y};
        }

        const auto offset = ImGui::GetCursorPos();
        if (m_sceneTexture) {
            ImGui::Image((ImTextureRef)m_sceneTexture->getView(),
                         ImVec2(viewportPanelSize.x, viewportPanelSize.y));
            m_isViewportHovered = ImGui::IsItemHovered();
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

        const auto &pickingId = m_viewportCtx->inputCtx.pickingId;

        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(4, 4));
        if (!pickingId.isValid() && ImGui::BeginPopupContextWindow()) {
            if (ImGui::MenuItemEx("Add Component",
                                  Icons::FontAwesomeIcons::FA_PLUS,
                                  "Shift-A")) {
                UI::UIMain::getPanel<ComponentExplorer>()->show();
            }

            ImGui::Separator();

            const auto selItems =
                m_attachedScene->getState().getSelectedComponents().size();

            if (ImGui::MenuItem("Select Net", "", false, selItems != 0)) {
                selectNets();
            } // Select Net

            if (ImGui::MenuItemEx("Delete",
                                  Icons::FontAwesomeIcons::FA_TRASH,
                                  "Del",
                                  false,
                                  selItems != 0)) {
                deleteSelComps();
            }

            if (ImGui::MenuItemEx("Delete Net",
                                  Icons::FontAwesomeIcons::FA_TRASH,
                                  "Ctrl-Del",
                                  false,
                                  selItems != 0)) {
                selectNets();
                deleteSelComps();
            }

            ImGui::EndPopup();
        }
        ImGui::PopStyleVar();

        drawTopLeftControls();
        drawBottomControls();
    }

    void SceneViewportPanel::onAfterDraw() {
        ImGui::PopStyleVar();
        ImGui::PopItemFlag();
        ImGui::PopItemFlag();
    }

    void SceneViewportPanel::drawTopLeftControls() {
        constexpr float windowR = 8.f;

        const auto *g = ImGui::GetCurrentContext();

        static float checkboxWidth =
            ImGui::CalcTextSize("W").x + g->Style.FramePadding.x + 2.f;
        static const auto textSize = ImGui::CalcTextSize("   Schematic Mode");
        static float size = textSize.x + checkboxWidth + (windowR * 2) +
                            (g->Style.FramePadding.x);

        const auto colors = g->Style.Colors;

        const ImVec2 windowPos = {
            m_viewportCtx->transform.pos.x + g->Style.FramePadding.x,
            m_viewportCtx->transform.pos.y + g->Style.FramePadding.y,
        };

        // Is schematic mode
        ImGui::SetNextWindowPos(windowPos);

        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(windowR, 2));
        ImGui::PushStyleVar(ImGuiStyleVar_ChildBorderSize, 0);
        ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, windowR);

        auto col = colors[ImGuiCol_ButtonActive];
        col.w = 0.2f;
        ImGui::PushStyleColor(ImGuiCol_WindowBg, col);
        ImGui::BeginChild(
            std::format("TopLeftViewportActions{}", m_viewportCtx->viewportId)
                .c_str(),
            ImVec2(size, 0),
            ImGuiChildFlags_AlwaysUseWindowPadding,
            NO_MOVE_FLAGS | ImGuiWindowFlags_NoDocking |
                ImGuiWindowFlags_NoNav);

        ImGui::AlignTextToFramePadding();
        ImGui::Text("%s Schematic Mode",
                    Icons::FontAwesomeIcons::FA_WAVE_SQUARE);
        ImGui::SameLine();
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(0, 0));

        const auto &sceneDriver = Proj::scenes();

        const auto &rootScene =
            sceneDriver.getSceneWithId(sceneDriver.getRootSceneId());

        bool isSchematicMode = m_viewportCtx->isSchematicMode();
        ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 4.f);
        if (ImGui::Checkbox("##CheckBoxSchematicMode", &isSchematicMode)) {
            m_viewportCtx->mode = isSchematicMode
                                      ? Core::Viewport::ViewportMode::schematic
                                      : Core::Viewport::ViewportMode::normal;
        }

        ImGui::PopStyleVar();
        ImGui::EndChild();
        ImGui::PopStyleColor(1);

        // Scene path (root > module ...)
        ImGui::SetNextWindowPos({windowPos.x + size + 8.f, windowPos.y});
        ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0, 0, 0, 0));
        ImGui::BeginChild(
            std::format("TopLeftViewportActions{}1", m_viewportCtx->viewportId)
                .c_str(),
            ImVec2(0, 0),
            ImGuiChildFlags_AlwaysUseWindowPadding |
                ImGuiChildFlags_AlwaysAutoResize | ImGuiChildFlags_AutoResizeY,
            NO_MOVE_FLAGS | ImGuiWindowFlags_NoDocking |
                ImGuiWindowFlags_NoNav);

        constexpr auto rootIcon =
            Common::Helpers::concat(Icons::CodIcons::RECORD, " Root");

        if (m_attachedScene->getState().getIsRootScene()) {
            ImGui::AlignTextToFramePadding();
            ImGui::TextDisabled("%s", rootIcon.data());
        } else {
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));

            for (int i = 0; i < m_rootToSceneStatePtrs.size(); i++) {
                if (i == 0) {
                    if (ImGui::Button(rootIcon.data())) {
                        m_nextSceneId = sceneDriver.getRootSceneId();
                    }
                    continue;
                }

                const auto &sceneStatePtr = m_rootToSceneStatePtrs[i];
                const auto &parentStatePtr = m_rootToSceneStatePtrs[i - 1];
                const auto &module = parentStatePtr->getComponentByUuid(
                    sceneStatePtr->getModuleId());

                ImGui::SameLine();
                ImGui::AlignTextToFramePadding();
                ImGui::TextDisabled(Icons::FontAwesomeIcons::FA_CHEVRON_RIGHT);
                ImGui::SameLine();

                if (i == m_rootToSceneStatePtrs.size() - 1) {
                    ImGui::AlignTextToFramePadding();
                    if (module) {
                        ImGui::TextDisabled(" %s", module->getName().c_str());
                    } else {
                        ImGui::TextDisabled(" Unknown Module");
                    }
                } else {
                    ImGui::PushID(i);
                    if (ImGui::Button(module ? module->getName().c_str()
                                             : " Unknown Module")) {
                        m_nextSceneId = sceneStatePtr->getSceneId();
                    }
                    ImGui::PopID();
                }
            }
            ImGui::PopStyleColor(1);
        }
        ImGui::EndChild();
        ImGui::PopStyleColor(1);

        ImGui::PopStyleVar(3);
    }

    bool SceneViewportPanel::isHovered() const {
        return m_isViewportHovered;
    }

    const glm::vec2 &SceneViewportPanel::getViewportPos() const {
        return m_viewportCtx->transform.pos;
    }

    const glm::vec2 &SceneViewportPanel::getViewportSize() const {
        return m_viewportCtx->transform.size;
    }

    namespace {
        std::string fmtPosX(float x) {
            return std::format(
                "{} {:>12.2f} ", Icons::FontAwesomeIcons::FA_X, x);
        }

        std::string fmtPosY(float x) {
            return std::format(
                "{} {:>12.2f} ", Icons::FontAwesomeIcons::FA_Y, x);
        }

        std::string fmtPos(float x, float y) {
            return std::format("{} {:>12.2f}   {} {:>12.2f}",
                               Icons::FontAwesomeIcons::FA_X,
                               x,
                               Icons::FontAwesomeIcons::FA_Y,
                               y);
        }
    } // namespace

    void SceneViewportPanel::drawBottomControls() const {
        const auto &vpMousePos = m_viewportCtx->inputCtx.mousePos;
        const auto mousePos = m_camera->toWorldPos(vpMousePos);

        static const auto fixedPosLabelSize =
            ImGui::CalcTextSize(fmtPos(99999.f, 99999.f).c_str());
        static const float fixedWinWidth = fixedPosLabelSize.x +
                                           ImGui::GetStyle().ItemSpacing.x +
                                           150.0f + 50.f;

        const ImVec2 windowPos = {
            m_viewportCtx->transform.pos.x + m_viewportCtx->transform.size.x -
                fixedWinWidth - 10.f,
            m_viewportCtx->transform.pos.y + m_viewportCtx->transform.size.y -
                44.f,
        };

        ImGui::SetNextWindowPos(windowPos);

        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(4, 0));
        ImGui::PushStyleVar(ImGuiStyleVar_ChildBorderSize, 0);
        ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 8);

        // Force height 34
        ImGui::BeginChild("SceneBottomRightControls",
                          ImVec2(0, 34),
                          ImGuiChildFlags_AlwaysUseWindowPadding |
                              ImGuiChildFlags_AlwaysAutoResize |
                              ImGuiChildFlags_AutoResizeX,
                          NO_MOVE_FLAGS | ImGuiWindowFlags_NoScrollbar |
                              ImGuiWindowFlags_NoNav);

        const float windowHeight = 34.0f;
        const float sliderHeight = ImGui::GetFrameHeight();

        ImGui::SetCursorPosY((windowHeight - sliderHeight) * 0.5f);

        // Camera Icon
        {
            ImGui::AlignTextToFramePadding();
            ImGui::Text(" %s", Icons::FontAwesomeIcons::FA_CAMERA);
            ImGui::SameLine();
        }

        ImGui::SameLine();

        const auto zoomSliderX = ImGui::GetCursorPosX() + fixedPosLabelSize.x +
                                 ImGui::GetStyle().ItemSpacing.x;

        // Mouse Pos Text
        {
            ImGui::AlignTextToFramePadding();
            const auto posLabelX = fmtPosX(mousePos.x);
            ImGui::Text(" %s", posLabelX.c_str());
            // Recenter on click
            if (ImGui::IsItemClicked(ImGuiMouseButton_Left)) {
                m_camera->focusAtPoint({0.f, 0.f}, false);
            }

            ImGui::SameLine();

            ImGui::SeparatorEx(ImGuiSeparatorFlags_Vertical);
            // Recenter on click
            if (ImGui::IsItemClicked(ImGuiMouseButton_Left)) {
                m_camera->focusAtPoint({0.f, 0.f}, false);
            }

            ImGui::SameLine();

            ImGui::AlignTextToFramePadding();
            const auto posLabelY = fmtPosY(mousePos.y);
            ImGui::Text(" %s", posLabelY.c_str());

            // Recenter on click
            if (ImGui::IsItemClicked(ImGuiMouseButton_Left)) {
                m_camera->focusAtPoint({0.f, 0.f}, false);
            }
        }

        ImGui::SameLine();

        ImGui::SetCursorPosX(zoomSliderX);

        // Zoom Slider
        {
            ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 8);
            ImGui::PushStyleVar(ImGuiStyleVar_GrabRounding, 8);

            ImGui::SetNextItemWidth(150.0f);
            if (ImGui::SliderFloat("##Zoom",
                                   &m_camera->getZoomRef(),
                                   Camera::zoomMin,
                                   Camera::zoomMax,
                                   "%.1fx",
                                   ImGuiSliderFlags_AlwaysClamp)) {
                const float stepSize = 0.1f;
                const float val =
                    roundf(m_camera->getZoom() / stepSize) * stepSize;
                m_camera->setZoom(val);
            }
            ImGui::PopStyleVar(2);
        }

        ImGui::EndChild();
        ImGui::PopStyleVar(3);
    }

    void SceneViewportPanel::deleteSelComps() {
        const auto targetScene = m_attachedScene;
        auto &appCtx = GAppContext::getInstance();
        auto sess = appCtx.getSubSystem<ProjectSession>();

        const auto &sceneState = targetScene->getState();
        auto selectedIds = sceneState.getSelectedComponents() |
                           std::ranges::views::keys |
                           std::ranges::to<std::vector<UUID>>();
        std::erase_if(selectedIds, [&](const UUID &id) {
            const auto component = sceneState.getComponentByUuid(id);
            return !component ||
                   component->getType() == Canvas::SceneComponentType::ui;
        });

        std::unordered_set<UUID> moduleCoveredIds;
        std::vector<UUID> moduleIds;
        std::vector<UUID> regularIds;

        std::function<void(const UUID &)> collectCoveredIds =
            [&](const UUID &uuid) {
                if (moduleCoveredIds.contains(uuid)) {
                    return;
                }

                const auto component = sceneState.getComponentByUuid(uuid);
                if (!component) {
                    return;
                }

                moduleCoveredIds.insert(uuid);
                for (const auto &dependantUuid :
                     component->getDependants(sceneState)) {
                    collectCoveredIds(dependantUuid);
                }
            };

        for (const auto &id : selectedIds) {
            const auto component = sceneState.getComponentByUuid(id);
            if (!component) {
                continue;
            }

            if (component->getType() == Canvas::SceneComponentType::module) {
                moduleIds.push_back(id);
                collectCoveredIds(id);
            }
        }

        for (const auto &id : selectedIds) {
            if (!moduleCoveredIds.contains(id)) {
                regularIds.push_back(id);
            }
        }

        if (moduleIds.empty() && regularIds.empty()) {
            return;
        }

        auto tx = sess->tx("Delete selection");
        Status status = Status::ok();
        for (const auto &moduleId : moduleIds) {
            status = Edit::rmModule(tx, targetScene, moduleId);
            if (!status) {
                break;
            }
        }

        if (status && !regularIds.empty()) {
            status = tx.rmComp(regularIds, targetScene->getSceneId());
        }

        if (status) {
            const auto result = tx.commit();
            if (!result) {
                BESS_WARN("Could not delete selection: {}",
                          result.status.msg());
            }
        } else {
            tx.cancel();
            BESS_WARN("Could not prepare selection delete: {}", status.msg());
        }
    }

    void SceneViewportPanel::selectNets() {
        auto &sceneState = m_attachedScene->getState();
        const auto selectedIds = sceneState.getSelectedComponents() |
                                 std::views::keys |
                                 std::ranges::to<std::vector<UUID>>();

        std::unordered_set<UUID> processedNetIds;

        // Sync nets
        updateNets();

        // Collect the nets
        for (const auto &compId : selectedIds) {
            const auto &comp = sceneState.getComponentByUuid(compId);
            if (!comp ||
                comp->getType() != Canvas::SceneComponentType::simulation)
                continue;

            const auto netId =
                comp->cast<Canvas::SimulationSceneComponent>()->getNetId();
            if (netId == UUID::null || processedNetIds.contains(netId)) {
                continue;
            }

            processedNetIds.insert(netId);
        }

        // Select the components belonging to those nets
        for (const auto &[id, comp] : sceneState.getAllComponents()) {
            if (!comp ||
                comp->getType() != Canvas::SceneComponentType::simulation)
                continue;

            const auto netId =
                comp->cast<Canvas::SimulationSceneComponent>()->getNetId();

            if (!processedNetIds.contains(netId)) {
                continue;
            }

            sceneState.addSelectedComponent(comp->getUuid());
        }
    }

    void SceneViewportPanel::onSceneAttached() {
        m_viewportCtx->reset();
        m_pendingSelectionReadback.clear();
        m_rootToSceneStatePtrs.clear();

        const auto &sceneDriver = Proj::scenes();

        UUID sceneId = m_attachedScene->getSceneId();

        while (sceneId != UUID::null) {
            const auto &scene = sceneDriver.getSceneWithId(sceneId);
            if (!scene) {
                BESS_ERROR(
                    "[SceneVewportPanel] Scene with id {} not found while "
                    "traversing parent scenes during scene attach.",
                    (uint64_t)sceneId);
                break;
            }
            m_rootToSceneStatePtrs.push_back(&scene->getState());
            sceneId = scene->getState().getParentSceneId();
            if (sceneDriver.getRootSceneId() != scene->getSceneId()) {
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

    void SceneViewportPanel::updateNets() {
        auto &simEngine = Proj::sim();

        if (!simEngine.isNetUpdated())
            return;

        auto &sceneState = m_attachedScene->getState();

        std::unordered_map<UUID,
                           std::shared_ptr<Canvas::SimulationSceneComponent>>
            simIdToComp;

        for (const auto &[compId, comp] : sceneState.getAllComponents()) {
            if (comp->getType() == Canvas::SceneComponentType::group ||
                comp->getType() == Canvas::SceneComponentType::simulation) {
                const auto simComp =
                    comp->cast<Canvas::SimulationSceneComponent>();
                simIdToComp[simComp->getSimEngineId()] = simComp;
            }
        }

        const auto &nets = simEngine.getNetsMap();
        for (const auto &[netId, net] : nets) {
            for (const auto &simId : net.getComponents()) {
                if (simIdToComp.contains(simId)) {
                    const auto &comp = simIdToComp[simId];
                    comp->setNetId(netId);
                }
            }
        }
    }

    glm::vec2 SceneViewportPanel::getSceneMousePos() {
        return m_camera->toWorldPos(m_viewportCtx->inputCtx.mousePos);
    }

    bool SceneViewportPanel::isFocused() const {
        return m_isViewportHovered;
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
