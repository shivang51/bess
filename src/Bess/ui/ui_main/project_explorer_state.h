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

        void reset();

        void addNode(const std::shared_ptr<ProjectExplorerNode> &node);

        void removeSceneEnttNode(entt::entity sceneEntity);

        void removeNode(const std::shared_ptr<ProjectExplorerNode> &node);

        std::shared_ptr<ProjectExplorerNode> getNodeFromSceneEntity(entt::entity sceneEntity);

        void moveNode(UUID node, UUID newParent);

        void moveNode(std::shared_ptr<ProjectExplorerNode> node, const std::shared_ptr<ProjectExplorerNode> &newParent);

        std::unordered_map<UUID, std::shared_ptr<ProjectExplorerNode>> nodesLookup;
        std::unordered_map<entt::entity, std::shared_ptr<ProjectExplorerNode>> enttNodesLookup;
        std::vector<std::shared_ptr<ProjectExplorerNode>> nodes;
    };
} // namespace Bess::UI
