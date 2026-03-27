#pragma once

#include "common/bess_uuid.h"
#include "ui_panel.h"
#include <filesystem>
#include <memory>

namespace Bess::Canvas {
    class Scene;
}

namespace Bess::UI {
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
        UUID sceneId = UUID::null;
    };

    class SceneExportWindow : public Panel {
      public:
        SceneExportWindow();

        void onShow() override;
        void onBeforeDraw() override;
        void onDraw() override;

      private:
        void refreshSelectedScene();
        std::shared_ptr<Canvas::Scene> getSelectedScene() const;
        std::string getSceneLabel(const std::shared_ptr<Canvas::Scene> &scene) const;

        static SceneBounds computeSceneBounds(const std::shared_ptr<Canvas::Scene> &scene);
        static SceneExportInfo getSceneExportInfo(const std::shared_ptr<Canvas::Scene> &scene,
                                                  const SceneBounds &bounds,
                                                  float zoom);
        static void exportScene(const std::shared_ptr<Canvas::Scene> &scene,
                                const SceneExportInfo &info);

      private:
        std::string fileName = "bess_scene_export";
        std::string exportPath;
        std::filesystem::path defaultExportPath;

        SceneBounds sceneBounds;
        glm::vec2 imgSize;
        UUID m_selectedSceneId = UUID::null;

        int zoom = 4;
    };
} // namespace Bess::UI
