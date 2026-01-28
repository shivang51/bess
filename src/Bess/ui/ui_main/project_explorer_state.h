#pragma once

#include "bess_uuid.h"
#include "json/value.h"
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
        UUID sceneId = UUID::null;
        std::string label;
        std::vector<std::shared_ptr<ProjectExplorerNode>> children;
        int visibleIndex = -1;
    };

    class ProjectExplorerState {
      public:
        ProjectExplorerState() = default;
        ~ProjectExplorerState() = default;

        void reset();

        void addNode(const std::shared_ptr<ProjectExplorerNode> &node, bool recursive = false);

        void removeNode(const std::shared_ptr<ProjectExplorerNode> &node);

        std::shared_ptr<ProjectExplorerNode> getNodeOfSceneEntt(const UUID &sceneId);

        void moveNode(UUID node, UUID newParent);

        void moveNode(std::shared_ptr<ProjectExplorerNode> node,
                      const std::shared_ptr<ProjectExplorerNode> &newParent);

        bool containsNode(const UUID &nodeId) const;
        bool containsSceneEntt(const UUID &sceneId) const;

        Json::Value toJson() const;
        void fromJson(const Json::Value &j);

        std::unordered_map<UUID, std::shared_ptr<ProjectExplorerNode>> nodesLookup;
        std::unordered_map<UUID, std::shared_ptr<ProjectExplorerNode>> enttNodesLookup;
        std::vector<std::shared_ptr<ProjectExplorerNode>> nodes;

        std::unordered_map<UUID, std::string> netIdToNameMap;
    };

} // namespace Bess::UI

namespace Bess::JsonConvert {
    void toJsonValue(const std::shared_ptr<Bess::UI::ProjectExplorerNode> &node, Json::Value &j);

    void toJsonValue(const Bess::UI::ProjectExplorerNode &node, Json::Value &j);

    void fromJsonValue(const Json::Value &j, Bess::UI::ProjectExplorerNode &node);

    void toJsonValue(const Bess::UI::ProjectExplorerState &state, Json::Value &j);

    void fromJsonValue(const Json::Value &j, Bess::UI::ProjectExplorerState &state);
} // namespace Bess::JsonConvert
