#include "project_explorer_state.h"

namespace Bess::UI {
    void ProjectExplorerState::reset() {
        nodes.clear();
        nodesLookup.clear();
        enttNodesLookup.clear();
    }

    void ProjectExplorerState::addNode(const std::shared_ptr<ProjectExplorerNode> &node) {
        nodesLookup[node->nodeId] = node;

        if (node->sceneEntity != entt::null) {
            enttNodesLookup[node->sceneEntity] = node;
        }

        if (node->parentId == UUID::null) {
            nodes.emplace_back(node);
        }
    }

    void ProjectExplorerState::removeSceneEnttNode(entt::entity sceneEntity) {
        auto it = enttNodesLookup.find(sceneEntity);
        if (it != enttNodesLookup.end()) {
            auto node = it->second;
            removeNode(node);
        }
    }

    void ProjectExplorerState::removeNode(const std::shared_ptr<ProjectExplorerNode> &node) {
        nodesLookup.erase(node->nodeId);
        if (node->sceneEntity != entt::null) {
            enttNodesLookup.erase(node->sceneEntity);
        }

        if (node->parentId != UUID::null) {
            auto parentIt = nodesLookup.find(node->parentId);
            if (parentIt != nodesLookup.end()) {
                auto &parentChildren = parentIt->second->children;
                const auto [first, last] = std::ranges::remove_if(
                    parentChildren,
                    [&](auto &f) { return f->nodeId == node->nodeId; });
                parentChildren.erase(first, last);
            }
        } else {
            const auto [first, last] = std::ranges::remove_if(nodes,
                                                              [&](auto &f) { return f->nodeId == node->nodeId; });
            nodes.erase(first, last);
        }
    }

    std::shared_ptr<ProjectExplorerNode> ProjectExplorerState::getNodeFromSceneEntity(entt::entity sceneEntity) {
        auto it = enttNodesLookup.find(sceneEntity);
        if (it != enttNodesLookup.end()) {
            return it->second;
        }
        return nullptr;
    }

    void ProjectExplorerState::moveNode(UUID node, UUID newParent) {
        BESS_ASSERT(nodesLookup.contains(node), "Nodes lookup does not contain node");
        BESS_ASSERT(newParent == UUID::null || nodesLookup.contains(newParent), "Nodes lookup does not contain new parent");
        moveNode(nodesLookup.at(node),
                 newParent == UUID::null ? nullptr : nodesLookup.at(newParent));
    }

    void ProjectExplorerState::moveNode(std::shared_ptr<ProjectExplorerNode> node, const std::shared_ptr<ProjectExplorerNode> &newParent) {
        BESS_ASSERT(node != nullptr, "node is null");

        if (node->parentId == (newParent ? newParent->nodeId : UUID::null)) {
            return; // No change
        }

        auto prevParentId = node->parentId;

        if (newParent != nullptr) {
            newParent->children.emplace_back(node);
            node->parentId = newParent->nodeId;
        } else {
            nodes.emplace_back(node);
            node->parentId = UUID::null;
        }

        if (prevParentId != UUID::null) {
            auto oldParentIt = nodesLookup.find(prevParentId);
            if (oldParentIt != nodesLookup.end()) {
                auto &oldParentChildren = oldParentIt->second->children;
                const auto [first, last] = std::ranges::remove_if(
                    oldParentChildren,
                    [&](auto &f) { return f->nodeId == node->nodeId; });
                oldParentChildren.erase(first, last);
            }
        } else {
            const auto [first, last] = std::ranges::remove_if(nodes,
                                                              [&](auto &f) { return f->nodeId == node->nodeId; });
            nodes.erase(first, last);
        }
    }
} // namespace Bess::UI
