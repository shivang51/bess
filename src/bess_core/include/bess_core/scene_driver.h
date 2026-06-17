#pragma once

#include "common/bess_uuid.h"
#include "common/sub_system.h"
#include "bess_core/scene/scene.h"

#include <memory>

namespace Bess {
    class BESS_API SceneDriver : public ISubSystem {
      public:
        SceneDriver() = default;
        ~SceneDriver() override = default;

        void onInit() override;
        void onShutdown() override;
        void onDestroy() override;
        void onUpdate(TimeMs ts) override;

        std::shared_ptr<Canvas::Scene> getActiveScene() const;

        void addScene(const std::shared_ptr<Canvas::Scene> &scene);
        void removeScene(const UUID &id);

        std::shared_ptr<Canvas::Scene> getSceneAtIdx(size_t index) const;
        std::shared_ptr<Canvas::Scene> getSceneWithId(const UUID &id) const;

        std::shared_ptr<Canvas::Scene> createNewScene();

        std::shared_ptr<Canvas::Scene> setActiveScene(size_t index);
        std::shared_ptr<Canvas::Scene> setActiveScene(UUID id);

        std::shared_ptr<Canvas::Scene>
        getSceneForModule(const UUID &modId) const;

        void removeScenes();

        void reset();

        size_t getActiveSceneIdx() const;

        size_t getSceneCount() const;

        // using pointer operator to directly access active scene
        std::shared_ptr<Canvas::Scene> operator->() {
            std::lock_guard lock(m_scenesMutex);
            return m_activeScene;
        }

        MAKE_GETTER_SETTER(UUID, RootSceneId, m_rootSceneId);
        MAKE_GETTER_SETTER(std::vector<std::shared_ptr<Canvas::Scene>>,
                           Scenes,
                           m_scenes);

        MAKE_GETTER_SETTER(bool, IsPaused, m_isPaused);

        void makeRootSceneActive();

      private:
        std::shared_ptr<Canvas::Scene> m_activeScene;
        std::vector<std::shared_ptr<Canvas::Scene>> m_scenes;
        std::unordered_map<UUID, std::shared_ptr<Canvas::Scene>>
            m_sceneIdToSceneMap;
        std::unordered_map<UUID, std::shared_ptr<Canvas::Scene>>
            m_modIdToSceneMap;
        UUID m_rootSceneId{UUID::null};
        size_t m_activeSceneIdx{0};

        bool m_isPaused = false;
        mutable std::mutex m_scenesMutex;
    };
} // namespace Bess
