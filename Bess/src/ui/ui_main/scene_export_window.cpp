#include "ui/ui_main/scene_export_window.h"
#include "common/log.h"
#include "imgui.h"
#include "pages/main_page/main_page_state.h"
#include "scene/scene.h"
#include "ui/icons/FontAwesomeIcons.h"
#include "ui/m_widgets.h"
#include "ui/ui_main/dialogs.h"

#include "png.h"
#include "stb_image_write.h"
#include <cstdint>
#include <filesystem>
#include <fstream>

namespace Bess::UI {
    bool SceneExportWindow::m_shown = false;
    bool SceneExportWindow::m_firstDraw = false;

    int zoom = 4;

#ifdef _WIN32
    std::filsystem::path homeDir = std::getenv("USERPROFILE");
#else
    std::filesystem::path homeDir = std::getenv("HOME");
#endif

    std::string fileName = "bess_scene_export";
    std::string exportPath = homeDir / "Pictures" / "bess";

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
        const auto &reg = Canvas::Scene::instance().getEnttRegistry();
        const auto view = reg.view<Canvas::Components::TransformComponent>();

        glm::vec2 min, max;
        bool first = true;
        for (const auto &ent : view) {
            const auto &comp = view.get<Canvas::Components::TransformComponent>(ent);
            if (first) {
                min = glm::vec2(comp.position) - glm::vec2(comp.scale);
                max = glm::vec2(comp.position) + glm::vec2(comp.scale);
                first = false;
                continue;
            }
            if (comp.position.x - comp.scale.x < min.x)
                min.x = comp.position.x - comp.scale.x;
            if (comp.position.x + comp.scale.x > max.x)
                max.x = comp.position.x + comp.scale.x;

            if (comp.position.y + comp.scale.y > max.y)
                max.y = comp.position.y + comp.scale.y;
            if (comp.position.y - comp.scale.y < min.y)
                min.y = comp.position.y - comp.scale.y;
        }

        return {min, max};
    }

    SceneExportInfo getSceneExportInfo(const SceneBounds &bounds, float zoom) {
        auto size = Canvas::Scene::instance().getSize();
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

        png_set_write_fn(pngPtr, &imgFile, [](png_structp png_ptr, png_bytep data, png_size_t length) {
							auto& stream = *static_cast<std::ostream*>(png_get_io_ptr(png_ptr));
							stream.write(reinterpret_cast<const char*>(data), length); }, [](png_structp png_ptr) {
							auto& stream = *static_cast<std::ostream*>(png_get_io_ptr(png_ptr));
            stream.flush(); });

        png_set_IHDR(pngPtr, pngInfoPtr, finalWidth, finalHeight, 8, PNG_COLOR_TYPE_RGBA,
                     PNG_INTERLACE_NONE, PNG_COMPRESSION_TYPE_DEFAULT, PNG_FILTER_TYPE_DEFAULT);

        png_write_info(pngPtr, pngInfoPtr);

        std::vector<unsigned char> imgRowBuffer(finalWidth * 4);
        const size_t snapRowSize = size.x * 4;

        // TODO: Implement Vulkan framebuffer creation for scene export
        // std::vector<Gl::FBAttachmentType> attachments = {Gl::FBAttachmentType::RGBA_RGBA,
        //                                                  Gl::FBAttachmentType::R32I_REDI,
        //                                                  Gl::FBAttachmentType::RGBA_RGBA,
        //                                                  Gl::FBAttachmentType::DEPTH32F_STENCIL8};
        // auto msaaFramebuffer = std::make_unique<Gl::FrameBuffer>(size.x, size.y, attachments, true);

        // attachments = {Gl::FBAttachmentType::RGBA_RGBA, Gl::FBAttachmentType::R32I_REDI};
        // auto normalFramebuffer = std::make_unique<Gl::FrameBuffer>(size.x, size.y, attachments);

        auto pos = min + snapSpan / 2.f;
        std::shared_ptr<Camera> camera = std::make_shared<Camera>(size.x, size.y);
        camera->setPos(pos);
        camera->setZoom(info.scale);

        for (int i = 0; i < snaps.y; i++) {
            pos.x = min.x + (snapSpan.x / 2.f);
            std::vector<std::vector<unsigned char>> snapsData;
            snapsData.reserve(snaps.x);
            for (int j = 0; j < snaps.x; j++) {
                camera->setPos(pos);

                // TODO: Implement Vulkan framebuffer operations for scene export
                // msaaFramebuffer->bind();
                // msaaFramebuffer->clearColorAttachment<GL_FLOAT>(0, glm::value_ptr(ViewportTheme::colors.background));
                // Gl::FrameBuffer::clearDepthStencilBuf();
                // Canvas::Scene::instance().drawScene(camera);
                // Gl::FrameBuffer::unbindAll();
                // msaaFramebuffer->bindColorAttachmentForRead(0);
                // normalFramebuffer->bindColorAttachmentForDraw(0);
                // Gl::FrameBuffer::blitColorBuffer(size.x, size.y);
                // Gl::FrameBuffer::unbindAll();

                // snapsData.emplace_back(normalFramebuffer->getPixelsFromColorAttachment(0));
                
                // Placeholder: create empty data for now
                snapsData.emplace_back(std::vector<unsigned char>(size.x * size.y * 4, 0));

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

        BESS_TRACE("[ExportSceneView] Successfully saved file to {}", path.string());
    }

    SceneBounds sceneBounds;
    glm::vec2 imgSize;
    void SceneExportWindow::draw() {
        if (!m_shown)
            return;

        if (m_firstDraw) {
            exportPath = std::filesystem::absolute(exportPath);
            if (!std::filesystem::exists(exportPath))
                std::filesystem::create_directories(exportPath);

            auto mainPage = Pages::MainPageState::getInstance();

            const auto now = std::chrono::system_clock::now();
            const std::chrono::zoned_time localTime{std::chrono::current_zone(), now};

            fileName = std::format("{}_{:%Y-%m-%d_%H:%M:%S}", mainPage->getCurrentProjectFile()->getName(), localTime);

            sceneBounds = computeSceneBounds();
            imgSize = getSceneExportInfo(sceneBounds, zoom).imgSize;
        }

        ImGui::SetNextWindowSize(ImVec2(600, 400), ImGuiCond_FirstUseEver);
        ImGui::Begin("Export scene as PNG", &m_shown);

        {
            float buttonHeight = ImGui::GetFrameHeight();
            float textHeight = ImGui::CalcTextSize("ajP").y;
            float verticalOffset = (buttonHeight - textHeight) / 2.0f;
            ImGui::SetCursorPosY(ImGui::GetCursorPosY() + verticalOffset);
            ImGui::Text("File Name");
            ImGui::SameLine();
            ImGui::SetCursorPosY(ImGui::GetCursorPosY() - verticalOffset);
            MWidgets::TextBox("##File Name", fileName);
            ImGui::SameLine();
            ImGui::TextDisabled(".png");
        }

        ImGui::Spacing();
        {
            MWidgets::TextBox("##Export Path", exportPath);
            ImGui::SameLine();
            if (ImGui::SmallButton(UI::Icons::FontAwesomeIcons::FA_FOLDER_OPEN)) {
                auto sel = Dialogs::showSelectPathDialog("Path to save");
                if (sel.size() > 0)
                    exportPath = sel;
            }
        }

        ImGui::Spacing();
        if (ImGui::SliderInt("Scale", &zoom, 1, 4)) {
            imgSize = getSceneExportInfo(sceneBounds, zoom).imgSize;
        }

        ImGui::TextDisabled("Image Size %lux%lu px.", (uint64_t)imgSize.x, (uint64_t)imgSize.y);

        ImGui::Spacing();
        ImGui::Spacing();

        if (ImGui::Button("Start Export")) {
            auto info = getSceneExportInfo(sceneBounds, zoom);
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
