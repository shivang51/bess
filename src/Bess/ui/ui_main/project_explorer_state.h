#pragma once

#include "bess_uuid.h"
#include "entt/entity/fwd.hpp"
#include "entt/entt.hpp"
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

        Json::Value toJson() const;
        void fromJson(const Json::Value &j);

        std::unordered_map<UUID, std::shared_ptr<ProjectExplorerNode>> nodesLookup;
        std::unordered_map<entt::entity, std::shared_ptr<ProjectExplorerNode>> enttNodesLookup;
        std::vector<std::shared_ptr<ProjectExplorerNode>> nodes;
    };

    namespace JsonConvert {
        inline void toJsonValue(const std::shared_ptr<Bess::UI::ProjectExplorerNode> &node, Json::Value &j) {
            j = Json::Value(Json::objectValue);
            j["selected"] = node->selected;
            j["multiSelectMode"] = node->multiSelectMode;
            j["isGroup"] = node->isGroup;
            Bess::JsonConvert::toJsonValue(node->nodeId, j["nodeId"]);
            Bess::JsonConvert::toJsonValue(node->parentId, j["parentId"]);
            j["sceneEntity"] = static_cast<uint32_t>(node->sceneEntity);
            j["label"] = node->label;

            j["children"] = Json::Value(Json::arrayValue);
            for (const auto &child : node->children) {
                Json::Value childJson;
                toJsonValue(child, childJson);
                j["children"].append(childJson);
            }
            j["visibleIndex"] = node->visibleIndex;
        }

        inline void toJsonValue(const Bess::UI::ProjectExplorerNode &node, Json::Value &j) {
            j = Json::Value(Json::objectValue);
            j["selected"] = node.selected;
            j["multiSelectMode"] = node.multiSelectMode;
            j["isGroup"] = node.isGroup;
            Bess::JsonConvert::toJsonValue(node.nodeId, j["nodeId"]);
            Bess::JsonConvert::toJsonValue(node.parentId, j["parentId"]);
            j["sceneEntity"] = static_cast<uint32_t>(node.sceneEntity);
            j["label"] = node.label;

            j["children"] = Json::Value(Json::arrayValue);
            for (const auto &child : node.children) {
                Json::Value childJson;
                toJsonValue(child, childJson);
                j["children"].append(childJson);
            }
            j["visibleIndex"] = node.visibleIndex;
        }

        inline void fromJsonValue(const Json::Value &j, Bess::UI::ProjectExplorerNode &node) {
            if (!j.isObject()) {
                return;
            }
            node.selected = j.get("selected", false).asBool();
            node.multiSelectMode = j.get("multiSelectMode", false).asBool();
            node.isGroup = j.get("isGroup", false).asBool();
            Bess::JsonConvert::fromJsonValue(j["nodeId"], node.nodeId);
            Bess::JsonConvert::fromJsonValue(j["parentId"], node.parentId);
            node.sceneEntity = static_cast<entt::entity>(j.get("sceneEntity", 0).asUInt());
            node.label = j.get("label", "").asString();

            node.children.clear();
            if (j.isMember("children")) {
                for (const auto &childJ : j["children"]) {
                    UI::ProjectExplorerNode childNode;
                    fromJsonValue(childJ, childNode);
                    node.children.push_back(std::make_shared<UI::ProjectExplorerNode>(childNode));
                }
            }
            node.visibleIndex = j.get("visibleIndex", -1).asInt();
        }

        inline void toJsonValue(const Bess::UI::ProjectExplorerState &state, Json::Value &j) {
            j = Json::Value(Json::objectValue);

            j["nodes"] = Json::Value(Json::arrayValue);
            for (const auto &node : state.nodes) {
                Json::Value nodeJ;
                toJsonValue(node, nodeJ);
                j["nodes"].append(nodeJ);
            }
        }

        inline void fromJsonValue(const Json::Value &j, Bess::UI::ProjectExplorerState &state) {
            if (!j.isObject()) {
                return;
            }

            state.reset();

            if (j.isMember("nodes")) {
                for (const auto &nodeJ : j["nodes"]) {
                    UI::ProjectExplorerNode node;
                    fromJsonValue(nodeJ, node);
                    auto nodePtr = std::make_shared<UI::ProjectExplorerNode>(node);
                    state.addNode(nodePtr);
                }
            }
        }
    } // namespace JsonConvert
} // namespace Bess::UI
