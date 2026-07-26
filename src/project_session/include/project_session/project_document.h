#pragma once

#include "common/bess_api.h"
#include "common/bess_uuid.h"
#include "project_session/result.h"
#include "project_session/scene_document.h"

#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace Bess::Session {
    struct BESS_API ProjectMetadata {
        static constexpr uint32_t currentFormatVersion = 1;

        UUID id = UUID::null;
        std::string name = "Untitled Project";
        uint32_t formatVersion = currentFormatVersion;
    };

    class BESS_API ProjectDocument {
      public:
        explicit ProjectDocument(ProjectMetadata metadata = {});
        ~ProjectDocument() = default;

        ProjectDocument(const ProjectDocument &) = delete;
        ProjectDocument &operator=(const ProjectDocument &) = delete;
        ProjectDocument(ProjectDocument &&) noexcept = default;
        ProjectDocument &operator=(ProjectDocument &&) noexcept = default;

        static std::unique_ptr<ProjectDocument>
        createEmpty(std::string name = "Untitled Project");

        const ProjectMetadata &metadata() const noexcept;
        ProjectMetadata &metadata() noexcept;

        Result<SceneDocument *> createScene(std::string name,
                                            bool makeActive = true,
                                            UUID requestedId = UUID::null);
        Result<std::unique_ptr<SceneDocument>> removeScene(UUID id);
        Status restoreScene(std::unique_ptr<SceneDocument> scene,
                            std::size_t index);

        SceneDocument *scene(UUID id) noexcept;
        const SceneDocument *scene(UUID id) const noexcept;
        SceneDocument *activeScene() noexcept;
        const SceneDocument *activeScene() const noexcept;

        Status setActiveScene(UUID id);
        Status setRootScene(UUID id);

        UUID activeSceneId() const noexcept;
        UUID rootSceneId() const noexcept;
        const std::vector<UUID> &sceneOrder() const noexcept;
        std::size_t sceneCount() const noexcept;
        std::size_t sceneIndex(UUID id) const noexcept;

        SceneDocument *sceneForModule(UUID module) noexcept;
        const SceneDocument *sceneForModule(UUID module) const noexcept;

        void clear();
        Status rebuildIndices();
        Status validate() const;

      private:
        void rebuildModuleIndex();

      private:
        ProjectMetadata m_metadata;
        std::unordered_map<UUID, std::unique_ptr<SceneDocument>> m_scenes;
        std::vector<UUID> m_sceneOrder;
        std::unordered_map<UUID, UUID> m_moduleToScene;
        UUID m_activeScene = UUID::null;
        UUID m_rootScene = UUID::null;
    };
} // namespace Bess::Session
