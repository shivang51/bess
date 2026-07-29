#pragma once

#include "common/bess_api.h"

#include "bess_core/scene/scene_events.h"
#include "bess_core/scene_driver.h"
#include "common/events.h"
#include "events/sim_engine_events.h"
#include "pages/main_page/services/hierarchical_scene_layout.h"
#include "project_session/status.h"
#include <vector>

namespace Bess {
    namespace Canvas {
        class Scene;
    }
    namespace SimEngine {
        class SimulationEngine;
    }
} // namespace Bess

namespace Bess::Pages {
    struct BESS_API PageActionFlags {
        bool openProject = false;
        bool saveProject = false;
    };

    struct BESS_API VerilogImportStatus {
        float progress = 0.f;
        std::string stageMessage;
        bool importing = false;
        bool finished = false;
        bool failed = false;
    };

    class BESS_API MainPageState {
      public:
        MainPageState();
        ~MainPageState();

        MainPageState(const MainPageState &) = delete;
        MainPageState &operator=(const MainPageState &) = delete;

        void update();

        typedef std::unordered_map<UUID, std::string *> TNetIdToNameMap;
        MAKE_GETTER_SETTER(TNetIdToNameMap, NetIdToNameMap, m_netIdToNameMap)
        MAKE_GETTER_SETTER(std::unordered_set<UUID>, Probes, m_probes)

        typedef std::unordered_map<UUID, std::vector<UUID>> TNetIdToCompMap;
        TNetIdToCompMap &getNetIdToCompMap(UUID sceneId);

        [[nodiscard]] Status resetProj(bool updateWindowName = true);
        [[nodiscard]] Status newProj(bool updateWindowName = true);
        [[nodiscard]] Status save();
        [[nodiscard]] Status load(const std::string &path);
        bool importVerilogFile(const std::string &path,
                               std::string *errorMessage = nullptr);
        bool importVerilogFiles(const std::vector<std::string> &paths,
                                std::string *errorMessage = nullptr);
        HierarchicalSceneLayoutResult applyHierarchicalLayoutToActiveScene();
        void startVerilogImport(const std::string &path);
        void startVerilogImport(const std::vector<std::string> &paths);
        VerilogImportStatus
        advanceVerilogImport(std::string *errorMessage = nullptr);
        void cancelVerilogImport();

        void init();

        std::shared_ptr<SceneDriver> getSceneDriver() const;
        std::shared_ptr<SceneDriver> getSceneDriver();

        PageActionFlags actionFlags = {};

        bool m_simulationPaused = false;
        void updateNets(const std::shared_ptr<Canvas::Scene> &scene);

        // contains the state of keyboard keys pressed

      private:
        void onWindowDropped(const Events::WindowDropEvent &event);
        void onEntityMoved(const Canvas::Events::EntityMovedEvent &e);

        void onEntityAdded(const Canvas::Events::ComponentAddedEvent &e);
        void onEntityRemoved(const Canvas::Events::ComponentRemovedEvent &e);

        void onCompDefOutputsResized(
            const SimEngine::Events::CompDefOutputsResizedEvent &e);
        void onCompDefInputsResized(
            const SimEngine::Events::CompDefInputsResizedEvent &e);

      private:
        std::unordered_map<int, bool> m_releasedKeysFrame;
        std::unordered_map<int, bool> m_pressedKeysFrame;
        std::unordered_map<int, bool> m_downKeys;
        struct SceneCompInfo {
            UUID sceneCompId;
            UUID sceneId;
        };
        std::unordered_map<UUID, SceneCompInfo> m_simIdToSceneCompId;
        std::unordered_set<UUID> m_probes;
        TNetIdToNameMap m_netIdToNameMap;
        std::unordered_map<UUID, TNetIdToCompMap> m_netIdToCompMap;
        struct VerilogImportSession;
        std::unique_ptr<VerilogImportSession> m_verilogImportSession;
    };
} // namespace Bess::Pages
