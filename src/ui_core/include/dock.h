#pragma once

#include "common/bess_assert.h"
#include "common/bess_uuid.h"
#include "common/class_helpers.h"
#include "common/logger.h"
#include "common/types.h"

#include <cstdint>

namespace Bess::UI {
    enum class DockZone : uint8_t {
        main = 0,
        left = 1,
        right = 2,
        top = 3,
        bottom = 4,
    };

    enum class DockNodeType : uint8_t {
        leaf = 0,
        split = 1,
        tab = 2,
    };

    enum class SplitDirection : uint8_t {
        horizontal = 0,
        vertical = 1,
    };

    class IDockNode {
      public:
        IDockNode() = default;
        virtual ~IDockNode() = default;

      public:
        bool isSplitter() const {
            return m_nodeType == DockNodeType::split;
        }

        bool isTab() const {
            return m_nodeType == DockNodeType::tab;
        }

        bool isLeaf() const {
            return m_nodeType == DockNodeType::leaf;
        }

        MAKE_GETTER_SETTER(DockNodeType, NodeType, m_nodeType)
        MAKE_GETTER_SETTER(bool, AllowsDocking, m_allowsDocking)
        MAKE_GETTER_SETTER(UUID, Id, m_id)
        MAKE_GETTER_SETTER(glm::vec2, Pos, m_pos)
        MAKE_GETTER_SETTER(glm::vec2, Size, m_size)

      protected:
        bool m_allowsDocking = false;
        DockNodeType m_nodeType = DockNodeType::leaf;
        UUID m_id = UUID::null;
        glm::vec2 m_pos = glm::vec2(0.0f, 0.0f);
        glm::vec2 m_size = glm::vec2(0.0f, 0.0f);
    };

    class DockSplitter : public IDockNode {
      public:
        DockSplitter() {
            m_nodeType = DockNodeType::split;
        }

      public:
        MAKE_GETTER_SETTER(SplitDirection, SplitDir, m_splitDir)
        MAKE_GETTER_SETTER(float, SplitRatio, m_splitRatio)

        typedef std::pair<UUID, UUID> SplitNodesType;
        MAKE_GETTER_SETTER(SplitNodesType, SplitNodes, m_splitNodes)

      private:
        SplitDirection m_splitDir = SplitDirection::horizontal;
        float m_splitRatio = 0.5f; // Ratio for split nodes
        SplitNodesType m_splitNodes;
    };

    class DockTab : public IDockNode {
      public:
        DockTab() {
            m_nodeType = DockNodeType::tab;
        }

        MAKE_GETTER_SETTER(std::vector<UUID>, DockedNodes, m_dockedNodes)

      protected:
        std::vector<UUID> m_dockedNodes;
    };

    class DockLeaf : public IDockNode {
      public:
        DockLeaf() {
            m_nodeType = DockNodeType::leaf;
        }

        MAKE_GETTER_SETTER(bool, IsDocked, m_isDocked)

      private:
        bool m_isDocked = false;
    };

    class DockManager {

      public:
        DockManager() = default;

        void layout() {
            if (!m_layoutDirty) {
                return;
            }

            if (m_rootNode == UUID::null) {
                BESS_ERROR("Root node is null, cannot layout");
                return;
            }

            auto rootNode = getNode(m_rootNode);
            BESS_ASSERT(rootNode, "Root node not found");

            layoutNode(rootNode);
            m_layoutDirty = false;
        }

        bool dockNode(const UUID &nodeId, const UUID &targetId, DockZone zone) {
            auto node = getNode(nodeId);
            auto target = getNode(targetId);

            bool res = false;
            const auto &targetType = node->getNodeType();

            switch (targetType) {
            case DockNodeType::leaf:
                res = dockToLeaf(node, target, zone);
                break;
            case DockNodeType::tab:
                res = dockToTab(node, target, zone);
                break;
            case DockNodeType::split:
                res = dockToSplitter(node, target, zone);
                break;
            default:
                BESS_ERROR("Unknown target node type {} for target {}",
                           static_cast<int>(targetType),
                           targetId);
                return false;
            }

            m_layoutDirty = res;

            return res;
        }

        std::shared_ptr<IDockNode> getNode(const UUID &nodeId) {
            auto it = m_nodes.find(nodeId);
            if (it != m_nodes.end()) {
                return it->second;
            }
            return nullptr;
        }

      private:
        void layoutNode(const std::shared_ptr<IDockNode> &node) {
            BESS_ASSERT(node, "Invalid node");

            switch (node->getNodeType()) {
            case DockNodeType::leaf:
                return;
            case DockNodeType::tab:
                layoutTabNode(node);
                break;
            case DockNodeType::split:
                layoutSplitNode(node);
                break;
            default:
                BESS_ERROR("Unknown node type {} for node {}",
                           static_cast<int>(node->getNodeType()),
                           node->getId());
                break;
            }
        }

        void layoutTabNode(const std::shared_ptr<IDockNode> &node) {
            BESS_ASSERT(node && node->getNodeType() == DockNodeType::tab,
                        "Invalid node");

            auto tabNode = std::dynamic_pointer_cast<DockTab>(node);
            BESS_ASSERT(tabNode, "Failed to cast node to DockTab");

            for (const auto &dockedNodeId : tabNode->getDockedNodes()) {
                auto dockedNode = getNode(dockedNodeId);
                node->setPos(tabNode->getPos());
                node->setSize(tabNode->getSize());
                layoutNode(dockedNode);
            }
        }

        void layoutSplitNode(const std::shared_ptr<IDockNode> &node) {
            BESS_ASSERT(node && node->getNodeType() == DockNodeType::split,
                        "Invalid node");
            auto splitterNode = std::dynamic_pointer_cast<DockSplitter>(node);
            BESS_ASSERT(splitterNode, "Failed to cast node to DockSplitter");

            auto splitNodes = splitterNode->getSplitNodes();
            BESS_ASSERT(splitNodes.first != UUID::null &&
                            splitNodes.second != UUID::null,
                        "Invalid split nodes");

            auto firstNode = getNode(splitNodes.first);
            auto secondNode = getNode(splitNodes.second);

            BESS_ASSERT(firstNode && secondNode, "Invalid split nodes");

            glm::vec2 pos = splitterNode->getPos();
            glm::vec2 size = splitterNode->getSize();

            if (splitterNode->getSplitDir() == SplitDirection::horizontal) {
                float firstHeight = size.y * splitterNode->getSplitRatio();
                float secondHeight = size.y - firstHeight;

                firstNode->setPos(pos);
                firstNode->setSize(glm::vec2(size.x, firstHeight));

                secondNode->setPos(glm::vec2(pos.x, pos.y + firstHeight));
                secondNode->setSize(glm::vec2(size.x, secondHeight));
            } else {
                float firstWidth = size.x * splitterNode->getSplitRatio();
                float secondWidth = size.x - firstWidth;

                firstNode->setPos(pos);
                firstNode->setSize(glm::vec2(firstWidth, size.y));

                secondNode->setPos(glm::vec2(pos.x + firstWidth, pos.y));
                secondNode->setSize(glm::vec2(secondWidth, size.y));
            }

            layoutNode(firstNode);
            layoutNode(secondNode);
        }

        template <typename T>
            requires(std::is_base_of_v<IDockNode, T>)
        std::shared_ptr<T>
        changeNodeType(const std::shared_ptr<IDockNode> &node) {
            BESS_ASSERT(node, "Invalid node");

            std::shared_ptr<T> newNode = std::make_shared<T>();
            BESS_ASSERT(newNode,
                        "Failed to create new node of type {}",
                        typeid(T).name());

            newNode->setId(node->getId());
            newNode->setPos(node->getPos());
            newNode->setSize(node->getSize());
            m_nodes[node->getId()] = newNode;

            return newNode;
        }

        template <typename T>
            requires(std::is_base_of_v<IDockNode, T>)
        std::shared_ptr<T> replaceNode(const std::shared_ptr<IDockNode> &node) {
            BESS_ASSERT(node, "Invalid node");

            auto newNode = changeNodeType<T>(node); // Change nodes type
            node->setId(UUID());                    // assign new id
            m_nodes[node->getId()] = node; // store old node with new id

            return newNode;
        }

        // Replaces the target node with a splitter node and docks the given
        // node to the splitter in the specified zone.
        // if zone is LEFT or TOP, the new node will be placed before the target
        // node in the splitter
        // else if zone is RIGHT or BOTTOM, the new node will be placed after
        // the target node in the splitter
        // VERTICAL splitter is for left and right zones,
        // HORIZONTAL splitter is for top and bottom zones
        bool replaceWithSplitter(const std::shared_ptr<IDockNode> &node,
                                 const std::shared_ptr<IDockNode> &target,
                                 DockZone zone) {
            BESS_ASSERT(node && target, "Invalid node or target");
            BESS_ASSERT(target->isLeaf(), "Target must be a leaf node");
            BESS_ASSERT(
                zone != DockZone::main,
                "Cannot dock to main zone when replacing with splitter");

            auto splitterNode = replaceNode<DockSplitter>(target);
            if (zone == DockZone::left || zone == DockZone::right) {
                splitterNode->setSplitDir(SplitDirection::vertical);
            } else {
                splitterNode->setSplitDir(SplitDirection::horizontal);
            }

            if (zone == DockZone::left || zone == DockZone::top) {
                splitterNode->setSplitNodes({node->getId(), target->getId()});
            } else {
                splitterNode->setSplitNodes({target->getId(), node->getId()});
            }

            return true;
        }

        // If zone is MAIN then the node will be changed to tab node and the
        // target will be added to the tab node Otherwise, a splitter will be
        // created
        bool dockToLeaf(const std::shared_ptr<IDockNode> &node,
                        const std::shared_ptr<IDockNode> &target,
                        DockZone zone) {
            BESS_ASSERT(node && target, "Invalid node or target");
            BESS_ASSERT(target->isLeaf(), "Target must be a leaf node");

            if (zone == DockZone::main) {
                // Change the target node to a tab node
                auto tabNode = replaceNode<DockTab>(target);
                tabNode->setDockedNodes({target->getId(), node->getId()});
            } else {
                // Change the target node to a splitter node
                return replaceWithSplitter(node, target, zone);
            }

            return true;
        }

        // If zone is MAIN then the node will be added to the children
        // Otherwise, split will be created
        bool dockToTab(const std::shared_ptr<IDockNode> &node,
                       const std::shared_ptr<IDockNode> &target,
                       DockZone zone) {
            BESS_ASSERT(node && target, "Invalid node or target");
            BESS_ASSERT(target->isTab(), "Target must be a tab node");

            if (zone == DockZone::main) {
                auto tabNode = std::dynamic_pointer_cast<DockTab>(target);
                BESS_ASSERT(tabNode, "Failed to cast target to DockTab");
                auto &dockedNodes = tabNode->getDockedNodes();
                dockedNodes.push_back(node->getId());
            } else {
                return replaceWithSplitter(node, target, zone);
            }

            return true;
        }

        // Splitter will not allow docking to main zone
        bool dockToSplitter(const std::shared_ptr<IDockNode> &node,
                            const std::shared_ptr<IDockNode> &target,
                            DockZone zone) {
            BESS_ASSERT(node && target, "Invalid node or target");
            BESS_ASSERT(target->isSplitter(), "Target must be a splitter node");
            BESS_ASSERT(zone != DockZone::main,
                        "Cannot dock to main zone when docking to splitter");

            return replaceWithSplitter(node, target, zone);
        }

      private:
        UUID m_rootNode = UUID::null;
        HashMap<UUID, std::shared_ptr<IDockNode>> m_nodes;
        bool m_layoutDirty = false;
    };
} // namespace Bess::UI
