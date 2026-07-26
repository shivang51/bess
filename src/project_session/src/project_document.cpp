#include "project_session/project_document.h"

#include <algorithm>
#include <limits>
#include <unordered_set>

namespace Bess::Session {
    ProjectDocument::ProjectDocument(ProjectMetadata metadata)
        : m_metadata(std::move(metadata)) {
        if (m_metadata.id == UUID::null) {
            m_metadata.id = UUID{};
        }
    }

    std::unique_ptr<ProjectDocument>
    ProjectDocument::createEmpty(std::string name) {
        ProjectMetadata metadata;
        metadata.id = UUID{};
        metadata.name = std::move(name);
        auto document = std::make_unique<ProjectDocument>(std::move(metadata));
        auto scene = document->createScene("Main", true);
        if (scene) {
            (void)document->setRootScene((*scene)->metadata().id);
        }
        return document;
    }

    const ProjectMetadata &ProjectDocument::metadata() const noexcept {
        return m_metadata;
    }

    ProjectMetadata &ProjectDocument::metadata() noexcept {
        return m_metadata;
    }

    Result<SceneDocument *> ProjectDocument::createScene(std::string name,
                                                         bool makeActive,
                                                         UUID requestedId) {
        const auto id = requestedId == UUID::null ? UUID{} : requestedId;
        if (m_scenes.contains(id)) {
            return fail(
                Error::alreadyExists("A scene with this UUID already exists"));
        }

        SceneMetadata metadata;
        metadata.id = id;
        metadata.name = name.empty() ? "Scene" : std::move(name);
        auto scene = std::make_unique<SceneDocument>(std::move(metadata));
        auto *result = scene.get();
        m_scenes.emplace(id, std::move(scene));
        m_sceneOrder.push_back(id);

        if (m_rootScene == UUID::null) {
            m_rootScene = id;
            result->metadata().isRoot = true;
        }
        if (makeActive || m_activeScene == UUID::null) {
            m_activeScene = id;
        }
        rebuildModuleIndex();
        return result;
    }

    Result<std::unique_ptr<SceneDocument>>
    ProjectDocument::removeScene(UUID id) {
        const auto found = m_scenes.find(id);
        if (found == m_scenes.end()) {
            return fail(Error::notFound("Scene does not exist"));
        }
        if (m_sceneOrder.size() == 1) {
            return fail(
                Error::conflict("A project must retain at least one scene"));
        }
        if (id == m_rootScene) {
            return fail(Error::conflict(
                "Set another root scene before removing this scene"));
        }
        for (const auto sceneId : m_sceneOrder) {
            const auto *ownerScene = scene(sceneId);
            const auto modules =
                ownerScene->registry()
                    .view<IdentityComponent, ModuleComponent>();
            for (const auto entity : modules) {
                if (modules.get<ModuleComponent>(entity).childScene == id) {
                    return fail(Error::conflict(
                        "Remove the owning module before its child scene"));
                }
            }
        }

        auto removed = std::move(found->second);
        m_scenes.erase(found);
        m_sceneOrder.erase(
            std::remove(m_sceneOrder.begin(), m_sceneOrder.end(), id),
            m_sceneOrder.end());

        if (m_activeScene == id) {
            m_activeScene =
                m_rootScene != UUID::null ? m_rootScene : m_sceneOrder.front();
        }
        rebuildModuleIndex();
        return removed;
    }

    Status ProjectDocument::restoreScene(std::unique_ptr<SceneDocument> scene,
                                         std::size_t index) {
        if (!scene) {
            return fail(
                Error::invalidArgument("Cannot restore a null scene document"));
        }
        const auto id = scene->metadata().id;
        if (id == UUID::null || m_scenes.contains(id)) {
            return fail(Error::conflict(
                "Cannot restore a scene with an invalid or duplicate UUID"));
        }

        index = std::min(index, m_sceneOrder.size());
        m_sceneOrder.insert(
            m_sceneOrder.begin() + static_cast<std::ptrdiff_t>(index), id);
        if (scene->metadata().isRoot) {
            if (m_rootScene != UUID::null) {
                if (auto *root = this->scene(m_rootScene)) {
                    root->metadata().isRoot = false;
                }
            }
            m_rootScene = id;
        }
        m_scenes.emplace(id, std::move(scene));
        rebuildModuleIndex();
        return {};
    }

    SceneDocument *ProjectDocument::scene(UUID id) noexcept {
        const auto found = m_scenes.find(id);
        return found == m_scenes.end() ? nullptr : found->second.get();
    }

    const SceneDocument *ProjectDocument::scene(UUID id) const noexcept {
        const auto found = m_scenes.find(id);
        return found == m_scenes.end() ? nullptr : found->second.get();
    }

    SceneDocument *ProjectDocument::activeScene() noexcept {
        return scene(m_activeScene);
    }

    const SceneDocument *ProjectDocument::activeScene() const noexcept {
        return scene(m_activeScene);
    }

    Status ProjectDocument::setActiveScene(UUID id) {
        auto *next = scene(id);
        if (!next) {
            return fail(Error::notFound("Scene does not exist"));
        }
        if (auto *current = activeScene(); current && current != next) {
            current->clearSelection();
            current->clearFocus();
        }
        m_activeScene = id;
        return {};
    }

    Status ProjectDocument::setRootScene(UUID id) {
        auto *next = scene(id);
        if (!next) {
            return fail(Error::notFound("Scene does not exist"));
        }
        if (auto *previous = scene(m_rootScene)) {
            previous->metadata().isRoot = false;
        }
        next->metadata().isRoot = true;
        m_rootScene = id;
        return {};
    }

    UUID ProjectDocument::activeSceneId() const noexcept {
        return m_activeScene;
    }

    UUID ProjectDocument::rootSceneId() const noexcept {
        return m_rootScene;
    }

    const std::vector<UUID> &ProjectDocument::sceneOrder() const noexcept {
        return m_sceneOrder;
    }

    std::size_t ProjectDocument::sceneCount() const noexcept {
        return m_sceneOrder.size();
    }

    std::size_t ProjectDocument::sceneIndex(UUID id) const noexcept {
        const auto found =
            std::find(m_sceneOrder.begin(), m_sceneOrder.end(), id);
        return found == m_sceneOrder.end()
                   ? std::numeric_limits<std::size_t>::max()
                   : static_cast<std::size_t>(
                         std::distance(m_sceneOrder.begin(), found));
    }

    SceneDocument *ProjectDocument::sceneForModule(UUID module) noexcept {
        const auto found = m_moduleToScene.find(module);
        return found == m_moduleToScene.end() ? nullptr : scene(found->second);
    }

    const SceneDocument *
    ProjectDocument::sceneForModule(UUID module) const noexcept {
        const auto found = m_moduleToScene.find(module);
        return found == m_moduleToScene.end() ? nullptr : scene(found->second);
    }

    void ProjectDocument::clear() {
        m_scenes.clear();
        m_sceneOrder.clear();
        m_moduleToScene.clear();
        m_activeScene = UUID::null;
        m_rootScene = UUID::null;
    }

    Status ProjectDocument::rebuildIndices() {
        rebuildModuleIndex();
        return validate();
    }

    void ProjectDocument::rebuildModuleIndex() {
        m_moduleToScene.clear();
        for (const auto sceneId : m_sceneOrder) {
            const auto *scene = this->scene(sceneId);
            if (scene && scene->metadata().module != UUID::null) {
                m_moduleToScene[scene->metadata().module] = sceneId;
            }
        }
    }

    Status ProjectDocument::validate() const {
        if (m_metadata.id == UUID::null) {
            return fail(Error::invalidState("Project UUID is null"));
        }
        if (m_metadata.formatVersion == 0 ||
            m_metadata.formatVersion > ProjectMetadata::currentFormatVersion) {
            return fail(Error::invalidState(
                "Project document has an unsupported format version"));
        }
        if (m_sceneOrder.empty() || m_sceneOrder.size() != m_scenes.size()) {
            return fail(Error::invalidState(
                "Project scene order and scene index are out of sync"));
        }
        if (!m_scenes.contains(m_rootScene) ||
            !m_scenes.contains(m_activeScene)) {
            return fail(
                Error::invalidState("Project root or active scene is missing"));
        }

        std::unordered_set<UUID> ordered;
        std::unordered_set<UUID> indexedModules;
        std::unordered_set<UUID> childSceneModules;
        std::size_t rootCount = 0;
        for (const auto id : m_sceneOrder) {
            if (!ordered.insert(id).second || !m_scenes.contains(id)) {
                return fail(Error::invalidState(
                    "Project scene order contains an invalid UUID"));
            }
            const auto &scene = *m_scenes.at(id);
            if (auto status = scene.validate(); !status) {
                return status;
            }
            if (scene.metadata().isRoot) {
                ++rootCount;
                if (id != m_rootScene) {
                    return fail(Error::invalidState(
                        "Project root-scene metadata is inconsistent"));
                }
            }
            const bool hasParent = scene.metadata().parentScene != UUID::null;
            const bool hasModule = scene.metadata().module != UUID::null;
            if (hasParent != hasModule) {
                return fail(Error::invalidState(
                    "Child-scene parent and module metadata are incomplete"));
            }
            if (hasModule) {
                const auto indexed =
                    m_moduleToScene.find(scene.metadata().module);
                if (!childSceneModules.insert(scene.metadata().module).second ||
                    indexed == m_moduleToScene.end() || indexed->second != id) {
                    return fail(Error::invalidState(
                        "A module owns more than one child scene"));
                }
            }

            const auto modules =
                scene.registry().view<IdentityComponent, ModuleComponent>();
            for (const auto entity : modules) {
                const auto &[identity, module] =
                    modules.get<IdentityComponent, ModuleComponent>(entity);
                if (module.childScene == UUID::null) {
                    continue;
                }
                const auto *childScene = this->scene(module.childScene);
                if (!childScene ||
                    childScene->metadata().module != identity.id ||
                    childScene->metadata().parentScene != id) {
                    return fail(Error::invalidState(
                        "Module and child-scene metadata are inconsistent"));
                }
                if (!indexedModules.insert(identity.id).second) {
                    return fail(Error::invalidState(
                        "A module owns more than one child scene"));
                }
            }
        }
        if (rootCount != 1) {
            return fail(Error::invalidState(
                "Project must contain exactly one root scene"));
        }
        if (indexedModules != childSceneModules) {
            return fail(Error::invalidState(
                "A child scene is not owned by its declared module"));
        }
        return {};
    }
} // namespace Bess::Session
