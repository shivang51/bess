#include "ui/ui_main/scene_export_window.h"
#include "common/bess_assert.h"
#include "common/logger.h"
#include "imgui.h"
#include "pages/main_page/main_page.h"
#include "pages/main_page/main_page_state.h"
#include "scene/scene.h"
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
#include <vulkan/vulkan_core.h>

namespace Bess::UI {
    bool SceneExportWindow::m_shown = false;
    bool SceneExportWindow::m_firstDraw = false;

    // JUST TEMP CODE, WILL CHANGE LATER

    struct SceneBounds {
        glm::vec2 min, max;
    };

    struct SceneSnapsInfo {
        glm::ivec2 count;
        glm::vec2 span;
        glm::vec2 size;
    };

    struct SceneExportInfo {
        SceneBounds sceneBounds;
        SceneSnapsInfo snapsInfo;
        float scale;
        glm::ivec2 imgSize;
        std::filesystem::path path;
    };

    SceneBounds computeSceneBounds() {
        glm::vec2 min, max;
        bool first = true;
        auto &scene = Pages::MainPage::getInstance()->getState().getSceneDriver();
        const auto &state = scene->getState();
        for (const auto &compId : state.getRootComponents()) {
            const auto comp = state.getComponentByUuid<Canvas::SceneComponent>(compId)->getTransform();
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

        return {min, max};
    }

    SceneExportInfo getSceneExportInfo(const SceneBounds &bounds, float zoom) {
        auto &scene = Pages::MainPage::getInstance()->getState().getSceneDriver();
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

        return info;
    }

    /// will move this to separate thread when vulkan is integrated
    void exportScene(const SceneExportInfo &info) {
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
        auto pos = min + snapSpan / 2.f;
        std::shared_ptr<Camera> camera = viewport->getCamera();
        camera->setPos(pos);
        camera->setZoom(info.scale);

        int frameIdx = 0;
        for (int i = 0; i < snaps.y; i++) {
            pos.x = min.x + (snapSpan.x / 2.f);
            std::vector<std::vector<unsigned char>> snapsData;
            snapsData.reserve(snaps.x);
            for (int j = 0; j < snaps.x; j++) {
                camera->setPos(pos);

                viewport->begin(frameIdx, ViewportTheme::colors.background, {0, 0});
                auto &scene = Pages::MainPage::getInstance()->getState().getSceneDriver();
                scene->getViewportDrawFn()(viewport);
                viewport->end();
                viewport->submit();
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

    SceneBounds sceneBounds;
    glm::vec2 imgSize;
    void SceneExportWindow::draw() {
        if (!m_shown)
            return;

#ifdef _WIN32
        static std::filsystem::path homeDir = std::getenv("USERPROFILE");
#else
        static std::filesystem::path homeDir = std::getenv("HOME");
#endif

        static std::string fileName = "bess_scene_export";
        static std::string exportPath = homeDir / "Pictures" / "bess";
        static int zoom = 4;

        if (m_firstDraw) {
            exportPath = std::filesystem::absolute(exportPath);
            if (!std::filesystem::exists(exportPath))
                std::filesystem::create_directories(exportPath);

            const auto &mainPage = Pages::MainPage::getInstance()->getState();

            const auto now = std::chrono::system_clock::now();
            const std::chrono::zoned_time localTime{std::chrono::current_zone(), now};

            fileName = std::format("{}_{:%Y-%m-%d_%H:%M:%S}", mainPage.getCurrentProjectFile()->getName(), localTime);

            sceneBounds = computeSceneBounds();
            imgSize = getSceneExportInfo(sceneBounds, (float)zoom).imgSize;
        }

        ImGui::SetNextWindowSize(ImVec2(600, 400), ImGuiCond_FirstUseEver);
        ImGui::Begin("Export scene as PNG", &m_shown);

        {
            const float buttonHeight = ImGui::GetFrameHeight();
            const float textHeight = ImGui::CalcTextSize("ajP").y;
            const float verticalOffset = (buttonHeight - textHeight) / 2.0f;
            ImGui::SetCursorPosY(ImGui::GetCursorPosY() + verticalOffset);
            ImGui::Text("File Name");
            ImGui::SameLine();
            ImGui::SetCursorPosY(ImGui::GetCursorPosY() - verticalOffset);
            Widgets::TextBox("##File Name", fileName);
            ImGui::SameLine();
            ImGui::TextDisabled(".png");
        }

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
            imgSize = getSceneExportInfo(sceneBounds, (float)zoom).imgSize;
        }

        ImGui::TextDisabled("Image Size %lux%lu px.", (uint64_t)imgSize.x, (uint64_t)imgSize.y);

        ImGui::Spacing();
        ImGui::Spacing();

        if (ImGui::Button("Start Export")) {
            auto info = getSceneExportInfo(sceneBounds, (float)zoom);
            info.path = exportPath;
            info.path /= fileName + ".png";
            exportScene(info);
        }

        ImGui::End();

        m_firstDraw = false;
    }

    void SceneExportWindow::show() {
        m_shown = true;
        m_firstDraw = true;
    }

    bool SceneExportWindow::isShown() {
        return m_shown;
    }

} // namespace Bess::UI
