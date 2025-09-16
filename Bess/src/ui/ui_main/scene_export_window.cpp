#include "ui/ui_main/scene_export_window.h"
#include "common/log.h"
#include "imgui.h"
#include "scene/scene.h"
#include "ui/icons/FontAwesomeIcons.h"
#include "ui/m_widgets.h"
#include "ui/ui_main/dialogs.h"

#include "stb_image_write.h"
#include <filesystem>

namespace Bess::UI {
    bool SceneExportWindow::m_shown = false;

    int zoom = 4;

#ifdef WINDOWS
    std::string path = "~/Pictures/";
#else
    std::string exportPath = "~/Pictures/";
#endif

    std::string fileName = "bess_scene_export";

    /// will move this to separate thread when vulkan is integrated
    void exportScene() {
        const auto &reg = Canvas::Scene::instance().getEnttRegistry();
        const auto view = reg.view<Canvas::Components::TransformComponent>();

        glm::vec2 min, max;
        for (const auto &ent : view) {
            const auto &comp = view.get<Canvas::Components::TransformComponent>(ent);
            if (comp.position.x - comp.scale.x < min.x)
                min.x = comp.position.x - comp.scale.x;
            if (comp.position.x + comp.scale.x > max.x)
                max.x = comp.position.x + comp.scale.x;

            if (comp.position.y + comp.scale.y > max.y)
                max.y = comp.position.y + comp.scale.y;
            if (comp.position.y - comp.scale.y < min.y)
                min.y = comp.position.y - comp.scale.y;
        }

        auto size = Canvas::Scene::instance().getSize();
        std::shared_ptr<Camera> camera = std::make_shared<Camera>(size.x, size.y);
        std::vector<Gl::FBAttachmentType> attachments = {Gl::FBAttachmentType::RGBA_RGBA,
                                                         Gl::FBAttachmentType::R32I_REDI,
                                                         Gl::FBAttachmentType::RGBA_RGBA,
                                                         Gl::FBAttachmentType::DEPTH32F_STENCIL8};
        auto msaaFramebuffer = std::make_unique<Gl::FrameBuffer>(size.x, size.y, attachments, true);

        attachments = {Gl::FBAttachmentType::RGBA_RGBA, Gl::FBAttachmentType::R32I_REDI};
        auto normalFramebuffer = std::make_unique<Gl::FrameBuffer>(size.x, size.y, attachments);

        static constexpr int value = -1;
        camera->setPos(min);
        camera->setZoom(zoom);
        auto span = camera->getSpan();

        glm::vec2 dist = max - min;

        glm::vec2 rem = {(int)dist.x % (int)span.x, (int)dist.y % (int)span.y};
        rem = span - rem;
        rem /= 2;
        min -= rem;
        max += rem;

        dist = max - min;

        glm::ivec2 snaps = glm::round(dist / span);

        std::vector<unsigned char> buffer(snaps.y * size.y * snaps.x * size.x * 4, 255);

        BESS_INFO("[ExportSceneView] Exporting between ({}, {}) and  ({}, {}) covering dist of ({}, {}) with sliding window of {}, {}",
                  min.x, min.y, max.x, max.y, dist.x, dist.y, span.x, span.y);
        BESS_INFO("[ExportSceneView] Snaps = {}, {} with size per snap {}, {}", snaps.x, snaps.y, size.x, size.y);
        BESS_INFO("[ExportSceneView] Generating image of size {}x{}", snaps.x * size.x, snaps.y * size.y);

        auto pos = min + span / 2.f;
        stbi_flip_vertically_on_write(1);

        for (int i = 0; i < snaps.y; i++) {
            pos.x = min.x + (span.x / 2.f);
            for (int j = 0; j < snaps.x; j++) {
                camera->setPos(pos);

                msaaFramebuffer->bind();
                msaaFramebuffer->clearColorAttachment<GL_FLOAT>(0, glm::value_ptr(ViewportTheme::backgroundColor));
                msaaFramebuffer->clearColorAttachment<GL_INT>(1, &value);
                msaaFramebuffer->clearColorAttachment<GL_FLOAT>(2, glm::value_ptr(ViewportTheme::backgroundColor));

                Gl::FrameBuffer::clearDepthStencilBuf();
                Canvas::Scene::instance().drawScene(camera);

                Gl::FrameBuffer::unbindAll();

                for (int i_ = 0; i_ < 2; i_++) {
                    msaaFramebuffer->bindColorAttachmentForRead(i_);
                    normalFramebuffer->bindColorAttachmentForDraw(i_);
                    Gl::FrameBuffer::blitColorBuffer(size.x, size.y);
                }
                Gl::FrameBuffer::unbindAll();

                pos.x += span.x;

                auto data = normalFramebuffer->getPixelsFromColorAttachment(0);

                const size_t sourceStride = size.x * 4;
                const size_t destStride = snaps.x * size.x * 4;

                for (int row = 0; row < size.y; row++) {
                    unsigned char *srcPtr = data.data() + row * sourceStride;
                    int destRow = (snaps.y - 1 - i) * size.y + row;
                    int destColOffset = j * size.x * 4;
                    unsigned char *destPtr = buffer.data() + destRow * destStride + destColOffset;
                    memcpy(destPtr, srcPtr, sourceStride);
                }
            }
            pos.y += span.y;
        }

        std::filesystem::path path = exportPath;
        path /= (fileName + ".png");

        int result = stbi_write_png(
            path.c_str(),
            snaps.x * size.x,
            snaps.y * size.y,
            4,
            buffer.data(),
            snaps.x * size.x * 4);

        if (result == 0) {
            BESS_ERROR("[ExportSceneView] Failed to save file to {}", path.c_str());
        } else {
            BESS_TRACE("[ExportSceneView] Successfully saved file to {}", path.c_str());
        }
    }

    void SceneExportWindow::draw() {
        if (!m_shown)
            return;
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
        ImGui::SliderInt("Scale", &zoom, 1, 4);

        ImGui::Spacing();
        ImGui::Spacing();

        if (ImGui::Button("Start Export")) {
            exportScene();
        }

        ImGui::End();
    }

    void SceneExportWindow::show() {
        m_shown = true;
    }

    bool SceneExportWindow::isShown() {
        return m_shown;
    }

} // namespace Bess::UI
