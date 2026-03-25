#pragma once

#include "common/bess_uuid.h"
#include "scene/scene.h"
#include <memory>

namespace Bess {
    class SceneDriver {
      public:
        SceneDriver() = default;
        ~SceneDriver() = default;

        std::shared_ptr<Canvas::Scene> getActiveScene() const;

        void addScene(const std::shared_ptr<Canvas::Scene> &scene);
        void removeScene(const UUID &id);

        std::shared_ptr<Canvas::Scene> getSceneAtIdx(size_t index) const;
        std::shared_ptr<Canvas::Scene> getSceneWithId(const UUID &id) const;

        std::shared_ptr<Canvas::Scene> createNewScene();

        std::shared_ptr<Canvas::Scene> setActiveScene(size_t index, bool updateCmdSys = true);
        std::shared_ptr<Canvas::Scene> setActiveScene(UUID id, bool updateCmdSys = true);

        std::shared_ptr<Canvas::Scene> getSceneForModule(const UUID &modId) const;

        void removeScenes();

        size_t getActiveSceneIdx() const;

        size_t getSceneCount() const;

        // syncs the net ids of comps in sim engine and passed scene
        void updateNets(const std::shared_ptr<Canvas::Scene> &scene);

        // syncs the net ids of comps in sim engine and active scene
        void updateNets();

        // using pointer operator to directly access active scene
        std::shared_ptr<Canvas::Scene> operator->() {
            return m_activeScene;
        }

        const std::shared_ptr<Canvas::Scene> &operator->() const {
            return m_activeScene;
        }

        MAKE_GETTER_SETTER(UUID, RootSceneId, m_rootSceneId);
        MAKE_GETTER_SETTER(std::vector<std::shared_ptr<Canvas::Scene>>, Scenes, m_scenes);

        void makeRootSceneActive();

      private:
        std::shared_ptr<Canvas::Scene> m_activeScene;
        std::vector<std::shared_ptr<Canvas::Scene>> m_scenes;
        std::unordered_map<UUID, std::shared_ptr<Canvas::Scene>> m_sceneIdToSceneMap;
        std::unordered_map<UUID, std::shared_ptr<Canvas::Scene>> m_modIdToSceneMap;
        UUID m_rootSceneId{UUID::null};
        size_t m_activeSceneIdx{0};
    };
} // namespace Bess
