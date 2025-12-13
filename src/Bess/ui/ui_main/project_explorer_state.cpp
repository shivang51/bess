#include "project_explorer_state.h"
#include "bess_uuid.h"
#include "common/log.h"
#include "json/value.h"
#include <cstdint>

namespace Bess::UI {
    void ProjectExplorerState::reset() {
        nodes.clear();
        nodesLookup.clear();
        enttNodesLookup.clear();
    }

    void ProjectExplorerState::addNode(const std::shared_ptr<ProjectExplorerNode> &node, bool recursive) {
        nodesLookup[node->nodeId] = node;

        if (node->sceneId != UUID::null) {
            enttNodesLookup[node->sceneId] = node;
        }

        if (node->parentId == UUID::null) {
            nodes.emplace_back(node);
        }

        if (!recursive)
            return;

        for (const auto &child : node->children) {
            addNode(child, true);
        }
    }

    std::shared_ptr<ProjectExplorerNode> ProjectExplorerState::getNodeOfSceneEntt(const UUID &sceneId) {
        auto it = enttNodesLookup.find(sceneId);
        if (it != enttNodesLookup.end()) {
            return it->second;
        }

        return nullptr;
    }

    void ProjectExplorerState::removeNode(const std::shared_ptr<ProjectExplorerNode> &node) {
        nodesLookup.erase(node->nodeId);
        if (node->sceneId != UUID::null) {
            enttNodesLookup.erase(node->sceneId);
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

    Json::Value ProjectExplorerState::toJson() const {
        Json::Value j = Json::Value(Json::objectValue);
        JsonConvert::toJsonValue(*this, j);
        return j;
    }

    void ProjectExplorerState::fromJson(const Json::Value &j) {
        JsonConvert::fromJsonValue(j, *this);
    }
} // namespace Bess::UI

namespace Bess::JsonConvert {
    void fromJsonValue(const Json::Value &j, Bess::UI::ProjectExplorerState &state) {
        if (!j.isObject()) {
            return;
        }

        state.reset();

        if (j.isMember("nodes")) {
            for (const auto &nodeJ : j["nodes"]) {
                UI::ProjectExplorerNode node;
                fromJsonValue(nodeJ, node);
                auto nodePtr = std::make_shared<UI::ProjectExplorerNode>(node);
                state.addNode(nodePtr, true);
            }
        }

        if (j.isMember("netIdToNameMap")) {
            for (const auto &netIdEntry : j["netIdToNameMap"]) {
                UUID netId;
                fromJsonValue(netIdEntry["id"], netId);
                std::string netName = netIdEntry["name"].asString();
                state.netIdToNameMap[netId] = netName;
            }
        }
    }

    void toJsonValue(const Bess::UI::ProjectExplorerState &state, Json::Value &j) {
        j = Json::Value(Json::objectValue);

        j["nodes"] = Json::Value(Json::arrayValue);
        for (const auto &node : state.nodes) {
            Json::Value nodeJ;
            toJsonValue(node, nodeJ);
            j["nodes"].append(nodeJ);
        }

        j["netIdToNameMap"] = Json::Value(Json::arrayValue);
        for (const auto &[netId, netName] : state.netIdToNameMap) {
            Json::Value entery;
            toJsonValue(netId, entery["id"]);
            entery["name"] = netName;
            j["netIdToNameMap"].append(entery);
        }
    }

    void fromJsonValue(const Json::Value &j, Bess::UI::ProjectExplorerNode &node) {
        if (!j.isObject()) {
            return;
        }
        node.selected = j.get("selected", false).asBool();
        node.multiSelectMode = j.get("multiSelectMode", false).asBool();
        node.isGroup = j.get("isGroup", false).asBool();
        Bess::JsonConvert::fromJsonValue(j["nodeId"], node.nodeId);
        Bess::JsonConvert::fromJsonValue(j["parentId"], node.parentId);
        Bess::JsonConvert::fromJsonValue(j["sceneId"], node.sceneId);
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

    void toJsonValue(const Bess::UI::ProjectExplorerNode &node, Json::Value &j) {
        j = Json::Value(Json::objectValue);
        j["selected"] = node.selected;
        j["multiSelectMode"] = node.multiSelectMode;
        j["isGroup"] = node.isGroup;
        Bess::JsonConvert::toJsonValue(node.nodeId, j["nodeId"]);
        Bess::JsonConvert::toJsonValue(node.parentId, j["parentId"]);
        Bess::JsonConvert::toJsonValue(node.sceneId, j["sceneId"]);
        j["label"] = node.label;

        j["children"] = Json::Value(Json::arrayValue);
        for (const auto &child : node.children) {
            Json::Value childJson;
            toJsonValue(child, childJson);
            j["children"].append(childJson);
        }
        j["visibleIndex"] = node.visibleIndex;
    }

    void toJsonValue(const std::shared_ptr<Bess::UI::ProjectExplorerNode> &node, Json::Value &j) {
        j = Json::Value(Json::objectValue);
        j["selected"] = node->selected;
        j["multiSelectMode"] = node->multiSelectMode;
        j["isGroup"] = node->isGroup;
        Bess::JsonConvert::toJsonValue(node->nodeId, j["nodeId"]);
        Bess::JsonConvert::toJsonValue(node->parentId, j["parentId"]);
        Bess::JsonConvert::toJsonValue(node->sceneId, j["sceneId"]);
        j["label"] = node->label;

        j["children"] = Json::Value(Json::arrayValue);
        for (const auto &child : node->children) {
            Json::Value childJson;
            toJsonValue(child, childJson);
            j["children"].append(childJson);
        }
        j["visibleIndex"] = node->visibleIndex;
    }

} // namespace Bess::JsonConvert
