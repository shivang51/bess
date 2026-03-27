#include "ui/ui_main/scene_export_window.h"
#include "common/logger.h"
#include "imgui.h"
#include "pages/main_page/main_page.h"
#include "pages/main_page/main_page_state.h"
#include "pages/main_page/scene_components/slot_scene_component.h"
#include "scene/scene_draw_context.h"
#include "scene/viewport.h"
#include "settings/viewport_theme.h"
#include "ui/icons/FontAwesomeIcons.h"
#include "ui/ui_main/dialogs.h"
#include "ui/widgets/m_widgets.h"
#include "vulkan_core.h"

#include "png.h"
#include "stb_image_write.h"
#include <algorithm>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <memory>
#include <vulkan/vulkan_core.h>

namespace Bess::UI {
    namespace {
        void drawExportGrid(SceneDrawContext &context) {
            context.materialRenderer->drawGrid(
                glm::vec3(0.f, 0.f, 0.1f),
                context.camera->getSpan(),
                Canvas::PickingId::invalid(),
                {
                    .minorColor = ViewportTheme::colors.gridMinorColor,
                    .majorColor = ViewportTheme::colors.gridMajorColor,
                    .axisXColor = ViewportTheme::colors.gridAxisXColor,
                    .axisYColor = ViewportTheme::colors.gridAxisYColor,
                },
                context.camera);
        }

        void drawExportComponents(SceneDrawContext &context) {
            auto &sceneState = *context.sceneState;
            const auto &cam = context.camera;
            const auto &span = (cam->getSpan() / 2.f) + 200.f;
            const auto &camPos = cam->getPos();

            for (const auto &compId : sceneState.getRootComponents()) {
                const auto comp = sceneState.getComponentByUuid(compId);
                if (!comp) {
                    continue;
                }

                const auto &pos = comp->getAbsolutePosition(sceneState);
                const auto x = pos.x - camPos.x;
                const auto y = pos.y - camPos.y;

                if (x < -span.x || x > span.x || y < -span.y || y > span.y) {
                    continue;
                }

                if (sceneState.getIsSchematicView()) {
                    comp->drawSchematic(context);
                } else {
                    comp->draw(context);
                }
            }
        }

        void renderSceneToViewport(const std::shared_ptr<Canvas::Scene> &scene,
                                   const std::shared_ptr<Canvas::Viewport> &viewport,
                                   int frameIdx) {
            BESS_ASSERT(scene, "[SceneExportWindow] Scene must be valid for export");
            BESS_ASSERT(viewport, "[SceneExportWindow] Viewport must be valid for export");
            BESS_ASSERT(viewport->getCamera(), "[SceneExportWindow] Export viewport camera must be valid");

            SceneDrawContext context;
            context.materialRenderer = viewport->getRenderers().materialRenderer;
            context.pathRenderer = viewport->getRenderers().pathRenderer;
            context.camera = viewport->getCamera();
            context.sceneState = &scene->getState();

            viewport->begin(frameIdx,
                            ViewportTheme::colors.background,
                            {0, Canvas::PickingId::invalid().runtimeId});
            drawExportGrid(context);
            drawExportComponents(context);
            viewport->end();
            viewport->submit();
        }
    } // namespace


    SceneExportWindow::SceneExportWindow()
        : Panel("Scene Export Window"), defaultExportPath("Pictures") {
#ifdef _WIN32
        const std::filsystem::path homeDir = std::getenv("USERPROFILE");
#else
        const std::filesystem::path homeDir = std::getenv("HOME");
#endif

        defaultExportPath /= "bess";
        exportPath = homeDir / defaultExportPath;
        m_showInMenuBar = false;
        m_defaultDock = Dock::none;
    }

    void SceneExportWindow::onDraw() {
        const float buttonHeight = ImGui::GetFrameHeight();
        const float textHeight = ImGui::CalcTextSize("ajP").y;
        const float verticalOffset = (buttonHeight - textHeight) / 2.0f;
        refreshSelectedScene();
        const auto selectedScene = getSelectedScene();

        if (ImGui::BeginCombo("Scene", selectedScene ? getSceneLabel(selectedScene).c_str() : "No Scene")) {
            auto &sceneDriver = Pages::MainPage::getInstance()->getState().getSceneDriver();
            for (size_t i = 0; i < sceneDriver.getSceneCount(); ++i) {
                const auto scene = sceneDriver.getSceneAtIdx(i);
                if (!scene) {
                    continue;
                }

                const bool isSelected = scene->getSceneId() == m_selectedSceneId;
                const auto label = getSceneLabel(scene);
                if (ImGui::Selectable(label.c_str(), isSelected)) {
                    m_selectedSceneId = scene->getSceneId();
                    sceneBounds = computeSceneBounds(scene);
                    imgSize = getSceneExportInfo(scene, sceneBounds, static_cast<float>(zoom)).imgSize;
                }
                if (isSelected) {
                    ImGui::SetItemDefaultFocus();
                }
            }
            ImGui::EndCombo();
        }

        ImGui::SetCursorPosY(ImGui::GetCursorPosY() + verticalOffset);
        ImGui::Text("File Name");
        ImGui::SameLine();
        ImGui::SetCursorPosY(ImGui::GetCursorPosY() - verticalOffset);
        Widgets::TextBox("##File Name", fileName);
        ImGui::SameLine();
        ImGui::TextDisabled(".png");

        ImGui::Spacing();
        {
            Widgets::TextBox("##Export Path", exportPath);
            ImGui::SameLine();
            if (ImGui::SmallButton(UI::Icons::FontAwesomeIcons::FA_FOLDER_OPEN)) {
                const auto sel = Dialogs::showSelectPathDialog("Path to save");
                if (sel.size() > 0)
                    exportPath = sel;
            }
        }

        ImGui::Spacing();
        if (ImGui::SliderInt("Scale", &zoom, 1, 4)) {
            if (selectedScene) {
                imgSize = getSceneExportInfo(selectedScene, sceneBounds, static_cast<float>(zoom)).imgSize;
            }
        }

        ImGui::TextDisabled("Image Size %lux%lu px.", (uint64_t)imgSize.x, (uint64_t)imgSize.y);

        ImGui::Spacing();
        ImGui::Spacing();

        if (ImGui::Button("Start Export")) {
            if (!selectedScene) {
                BESS_ERROR("[SceneExportWindow] No scene selected for export");
                return;
            }

            auto info = getSceneExportInfo(selectedScene, sceneBounds, static_cast<float>(zoom));
            info.path = exportPath;
            info.path /= fileName + ".png";
            exportScene(selectedScene, info);
        }
    }

    void SceneExportWindow::onShow() {
        exportPath = std::filesystem::absolute(exportPath);
        if (!std::filesystem::exists(exportPath))
            std::filesystem::create_directories(exportPath);

        const auto &mainPage = Pages::MainPage::getInstance()->getState();

        const auto now = std::chrono::system_clock::now();
        const std::chrono::zoned_time localTime{std::chrono::current_zone(), now};

        fileName = std::format("{}_{:%Y-%m-%d_%H:%M:%S}", mainPage.getCurrentProjectFile()->getName(), localTime);

        refreshSelectedScene();
        if (const auto scene = getSelectedScene()) {
            sceneBounds = computeSceneBounds(scene);
            imgSize = getSceneExportInfo(scene, sceneBounds, static_cast<float>(zoom)).imgSize;
        } else {
            sceneBounds = {};
            imgSize = {};
        }
    }

    void SceneExportWindow::onBeforeDraw() {
        ImGui::SetNextWindowSize(ImVec2(600, 400), ImGuiCond_FirstUseEver);
    }

    void SceneExportWindow::refreshSelectedScene() {
        auto &sceneDriver = Pages::MainPage::getInstance()->getState().getSceneDriver();
        if (m_selectedSceneId != UUID::null &&
            sceneDriver.getSceneWithId(m_selectedSceneId)) {
            return;
        }

        const auto activeScene = sceneDriver.getActiveScene();
        m_selectedSceneId = activeScene ? activeScene->getSceneId() : UUID::null;
    }

    std::shared_ptr<Canvas::Scene> SceneExportWindow::getSelectedScene() const {
        auto &sceneDriver = Pages::MainPage::getInstance()->getState().getSceneDriver();
        return m_selectedSceneId == UUID::null ? nullptr : sceneDriver.getSceneWithId(m_selectedSceneId);
    }

    std::string SceneExportWindow::getSceneLabel(const std::shared_ptr<Canvas::Scene> &scene) const {
        if (!scene) {
            return "No Scene";
        }

        if (scene->getState().getIsRootScene()) {
            return "Root";
        }

        auto &sceneDriver = Pages::MainPage::getInstance()->getState().getSceneDriver();
        const auto parentScene = sceneDriver.getSceneWithId(scene->getState().getParentSceneId());
        if (!parentScene) {
            return std::format("Scene {}", static_cast<uint64_t>(scene->getSceneId()));
        }

        const auto module = parentScene->getState().getComponentByUuid(scene->getState().getModuleId());
        if (!module) {
            return std::format("Scene {}", static_cast<uint64_t>(scene->getSceneId()));
        }

        return module->getName();
    }

    SceneBounds SceneExportWindow::computeSceneBounds(const std::shared_ptr<Canvas::Scene> &scene) {
        BESS_ASSERT(scene, "[SceneExportWindow] Scene must be valid while computing bounds");

        glm::vec2 min, max;
        bool first = true;
        const auto &state = scene->getState();
        for (const auto &compId : state.getRootComponents()) {
            const auto component = state.getComponentByUuid<Canvas::SceneComponent>(compId);
            if (!component) {
                continue;
            }

            const auto comp = component->getTransform();
            if (first) {
                min = glm::vec2(comp.position) - glm::vec2(comp.scale);
                max = glm::vec2(comp.position) + glm::vec2(comp.scale);
                first = false;
                continue;
            }
            min.x = std::min(comp.position.x - comp.scale.x, min.x);
            max.x = std::max(comp.position.x + comp.scale.x, max.x);
            max.y = std::max(comp.position.y + comp.scale.y, max.y);
            min.y = std::min(comp.position.y - comp.scale.y, min.y);
        }

        if (first) {
            min = {-400.f, -300.f};
            max = {400.f, 300.f};
        }

        return {min, max};
    }

    SceneExportInfo SceneExportWindow::getSceneExportInfo(const std::shared_ptr<Canvas::Scene> &scene,
                                                          const SceneBounds &bounds,
                                                          float zoom) {
        BESS_ASSERT(scene, "[SceneExportWindow] Scene must be valid while preparing export info");

        auto size = scene->getSize();
        std::shared_ptr<Camera> camera = std::make_shared<Camera>(size.x, size.y);
        camera->setPos(bounds.min);
        camera->setZoom(zoom);
        auto snapSize = camera->getSpan();

        auto min = bounds.min;
        auto max = bounds.max;

        glm::vec2 dist = max - min;

        glm::vec2 rem = {(int)dist.x % (int)snapSize.x, (int)dist.y % (int)snapSize.y};
        rem = snapSize - rem;
        rem /= 2;
        min -= rem;
        max += rem;

        dist = max - min;

        glm::ivec2 snaps = glm::round(dist / snapSize);

        SceneExportInfo info;
        info.sceneBounds = {min, max};
        info.snapsInfo = {.count = snaps, .span = snapSize, .size = size};
        info.imgSize = {snaps.x * size.x, snaps.y * size.y};
        info.scale = zoom;
        info.sceneId = scene->getSceneId();

        return info;
    }

    /// will move this to separate thread when vulkan is integrated
    void SceneExportWindow::exportScene(const std::shared_ptr<Canvas::Scene> &scene,
                                        const SceneExportInfo &info) {
        BESS_ASSERT(scene, "[SceneExportWindow] Scene must be valid while exporting");

        const auto &size = info.snapsInfo.size;
        const auto &sceneBounds = info.sceneBounds;
        const auto &min = sceneBounds.min;

        const auto &snapSpan = info.snapsInfo.span;
        const glm::ivec2 &snaps = info.snapsInfo.count;
        const int finalWidth = info.imgSize.x;
        const int finalHeight = info.imgSize.y;

        BESS_INFO("[ExportSceneView] Snaps = {}, {} with size per snap {}, {}", snaps.x, snaps.y, size.x, size.y);
        BESS_INFO("[ExportSceneView] Generating image of size {}x{}", finalWidth, finalHeight);

        const auto &path = info.path;
        std::ofstream imgFile = std::ofstream(path, std::ios::binary);
        if (!imgFile.is_open()) {
            BESS_ERROR("[ExportSceneView] Failed to open file for writing: {}", path.string());
            return;
        }

        png_structp pngPtr = png_create_write_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
        if (!pngPtr) {
            BESS_ERROR("[ExportSceneView] png_create_write_struct failed.");
            return;
        }

        png_infop pngInfoPtr = png_create_info_struct(pngPtr);
        if (!pngInfoPtr) {
            BESS_ERROR("[ExportSceneView] png_create_info_struct failed.");
            png_destroy_write_struct(&pngPtr, NULL);
            return;
        }

        if (setjmp(png_jmpbuf(pngPtr))) {
            BESS_ERROR("[ExportSceneView] Error during png creation.");
            png_destroy_write_struct(&pngPtr, &pngInfoPtr);
            return;
        }

        png_set_write_fn(pngPtr, &imgFile, [](const png_structp png_ptr, const png_bytep data, const png_size_t length) {
							auto& stream = *static_cast<std::ostream*>(png_get_io_ptr(png_ptr));
							stream.write(reinterpret_cast<const char*>(data), length); }, [](const png_structp png_ptr) {
							auto& stream = *static_cast<std::ostream*>(png_get_io_ptr(png_ptr));
            stream.flush(); });

        png_set_IHDR(pngPtr, pngInfoPtr, finalWidth, finalHeight, 8, PNG_COLOR_TYPE_RGBA,
                     PNG_INTERLACE_NONE, PNG_COMPRESSION_TYPE_DEFAULT, PNG_FILTER_TYPE_DEFAULT);

        png_write_info(pngPtr, pngInfoPtr);

        std::vector<unsigned char> imgRowBuffer(finalWidth * 4);
        const size_t snapRowSize = size.x * 4;
        VkExtent2D extent = {(uint32_t)size.x, (uint32_t)size.y};

        auto &vkCore = Bess::Vulkan::VulkanCore::instance();
        auto viewport = std::make_shared<Canvas::Viewport>(vkCore.getDevice(), vkCore.getSwapchain()->imageFormat(), extent);
        auto camera = std::make_shared<Camera>(size.x, size.y);
        viewport->setCamera(camera);
        auto pos = min + snapSpan / 2.f;
        camera->setPos(pos);
        camera->setZoom(info.scale);

        int frameIdx = 0;
        for (int i = 0; i < snaps.y; i++) {
            pos.x = min.x + (snapSpan.x / 2.f);
            std::vector<std::vector<unsigned char>> snapsData;
            snapsData.reserve(snaps.x);
            for (int j = 0; j < snaps.x; j++) {
                camera->setPos(pos);
                renderSceneToViewport(scene, viewport, frameIdx);
                frameIdx = (frameIdx + 1) % 2;

                snapsData.emplace_back(viewport->getPixelData());

                pos.x += snapSpan.x;
            }

            for (int imgRow = size.y - 1; imgRow >= 0; imgRow--) {
                for (int j = 0; j < snaps.x; j++) {
                    const auto &snapData = snapsData[j];
                    const unsigned char *srcPtr = snapData.data() + imgRow * snapRowSize;
                    unsigned char *destPtr = imgRowBuffer.data() + j * snapRowSize;
                    memcpy(destPtr, srcPtr, snapRowSize);
                }
                png_write_row(pngPtr, imgRowBuffer.data());
            }
            pos.y += snapSpan.y;
        }

        png_write_end(pngPtr, NULL);
        png_destroy_write_struct(&pngPtr, &pngInfoPtr);

        BESS_INFO("[ExportSceneView] Successfully saved file to {}", path.string());
    }
} // namespace Bess::UI
