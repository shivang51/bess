#pragma once

#include "bess_uuid.h"
#include "common/log.h"
#include <algorithm>
#include <memory>
#include <string>
#include <unordered_map>

namespace Bess::UI {

    struct ProjectExplorerNode {
        UUID nodeId;
        bool selected;
        bool multiSelectMode;
        bool isGroup = false;
        std::string label;
        std::vector<std::shared_ptr<ProjectExplorerNode>> children;
        UUID parentId = UUID::null;
    };

    class ProjectExplorerState {
      public:
        ProjectExplorerState() = default;
        ~ProjectExplorerState() = default;

        void reset() {
            nodes.clear();
            nodesLookup.clear();
        }

        void addNode(const std::shared_ptr<ProjectExplorerNode> &node) {
            nodesLookup[node->nodeId] = node;

            if (node->parentId == UUID::null) {
                nodes.emplace_back(node);
            }
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
        std::vector<std::shared_ptr<ProjectExplorerNode>> nodes;
    };
} // namespace Bess::UI
