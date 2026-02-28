#pragma once

#include "ui_panel.h"
#include <filesystem>
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
    };

    class SceneExportWindow : public Panel {
      public:
        SceneExportWindow();

        void onShow() override;
        void onBeforeDraw() override;
        void onDraw() override;

      private:
        static SceneBounds computeSceneBounds();
        static SceneExportInfo getSceneExportInfo(const SceneBounds &bounds, float zoom);
        static void exportScene(const SceneExportInfo &info);

      private:
        std::string fileName = "bess_scene_export";
        std::string exportPath;
        std::filesystem::path defaultExportPath;

        SceneBounds sceneBounds;
        glm::vec2 imgSize;

        int zoom = 4;
    };
} // namespace Bess::UI
