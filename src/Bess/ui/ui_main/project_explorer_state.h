#pragma once

#include "bess_uuid.h"
#include "common/log.h"
#include "entt/entity/fwd.hpp"
#include "entt/entt.hpp"
#include <algorithm>
#include <memory>
#include <string>
#include <unordered_map>

namespace Bess::UI {

    struct ProjectExplorerNode {
        bool selected;
        bool multiSelectMode;
        bool isGroup = false;
        UUID nodeId;
        UUID parentId = UUID::null;
        entt::entity sceneEntity = entt::null;
        std::string label;
        std::vector<std::shared_ptr<ProjectExplorerNode>> children;
        int visibleIndex = -1;
    };

    class ProjectExplorerState {
      public:
        ProjectExplorerState() = default;
        ~ProjectExplorerState() = default;

        void reset() {
            nodes.clear();
            nodesLookup.clear();
            enttNodesLookup.clear();
        }

        void addNode(const std::shared_ptr<ProjectExplorerNode> &node) {
            nodesLookup[node->nodeId] = node;

            if (node->sceneEntity != entt::null) {
                enttNodesLookup[node->sceneEntity] = node;
            }

            if (node->parentId == UUID::null) {
                nodes.emplace_back(node);
            }
        }

        void removeSceneEnttNode(entt::entity sceneEntity) {
            auto it = enttNodesLookup.find(sceneEntity);
            if (it != enttNodesLookup.end()) {
                auto node = it->second;
                nodesLookup.erase(node->nodeId);
                enttNodesLookup.erase(it);

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
        }

        std::shared_ptr<ProjectExplorerNode> getNodeFromSceneEntity(entt::entity sceneEntity) {
            auto it = enttNodesLookup.find(sceneEntity);
            if (it != enttNodesLookup.end()) {
                return it->second;
            }
            return nullptr;
        }

        void moveNode(UUID node, UUID newParent) {
            BESS_ASSERT(nodesLookup.contains(node), "Nodes lookup does not contain node");
            BESS_ASSERT(newParent == UUID::null || nodesLookup.contains(newParent), "Nodes lookup does not contain new parent");
            moveNode(nodesLookup.at(node),
                     newParent == UUID::null ? nullptr : nodesLookup.at(newParent));
        }

        void moveNode(std::shared_ptr<ProjectExplorerNode> node, const std::shared_ptr<ProjectExplorerNode> &newParent) {
            BESS_ASSERT(node != nullptr, "node is null");

            if (node->parentId != UUID::null) {
                auto oldParentIt = nodesLookup.find(node->parentId);
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

            if (newParent != nullptr) {
                newParent->children.emplace_back(node);
                node->parentId = newParent->nodeId;
            } else {
                nodes.emplace_back(node);
                node->parentId = UUID::null;
            }
        }

        std::unordered_map<UUID, std::shared_ptr<ProjectExplorerNode>> nodesLookup;
        std::unordered_map<entt::entity, std::shared_ptr<ProjectExplorerNode>> enttNodesLookup;
        std::vector<std::shared_ptr<ProjectExplorerNode>> nodes;
    };
} // namespace Bess::UI
