#include "dock.h"

#include "common/bess_assert.h"
#include "common/logger.h"
#include <algorithm>

namespace Bess::UI {
    UUID DockManager::getHitRect(const glm::vec2 &point) {
        for (const auto &rect : m_rects) {
            if (rect.contains(point)) {
                return rect.id;
            }
        }
        return UUID::null;
    }

    void DockManager::layout(const bool force) {
        if (!m_layoutDirty && !force) {
            return;
        }

        // Laying out root nodes
        if (m_rootNode != UUID::null) {
            auto rootNode = getNode(m_rootNode);
            BESS_ASSERT(rootNode, "Root node not found");
            layoutNode(rootNode);
        }

        // Laying out floating nodes
        for (const auto &[id, node] : m_nodes) {
            if (node->isFloating()) {
                layoutNode(node);
            }
        }

        m_layoutDirty = false;

        m_rects.clear();
        m_rects.reserve(m_nodes.size());

        for (const auto &[id, node] : m_nodes) {
            const bool shouldAddRect = node->isFloating() || node->isLeaf();

            DockRect rect;
            rect.id = id;
            rect.pos = node->getPos();
            rect.size = node->getSize();
            m_rects.push_back(rect);
        }
    }

    bool DockManager::dockNode(const UUID &nodeId,
                               const UUID &targetId,
                               DockZone zone) {
        auto node = getNode(nodeId);

        bool res = false;

        if (targetId == m_rootNode && m_rootNode == UUID::null) {
            setRootNode(node);
            res = true;
        } else {
            auto target = getNode(targetId);
            const auto &targetType = target->getNodeType();

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
        }

        if (res) {
            node->setDockedTo(targetId);
        }

        m_layoutDirty = res;
        return res;
    }

    std::shared_ptr<IDockNode> DockManager::getNode(const UUID &nodeId) {
        auto it = m_nodes.find(nodeId);
        if (it != m_nodes.end()) {
            return it->second;
        }
        return nullptr;
    }

    bool DockManager::undockNode(const UUID &nodeId) {
        auto node = getNode(nodeId);
        if (!node) {
            BESS_ERROR("Node {} not found for undocking", nodeId);
            return false;
        }

        if (node->isFloating()) {
            return false;
        }

        if (!node->isLeaf()) {
            BESS_ERROR("Node {} is not a leaf node, cannot undock", nodeId);
            return false;
        }

        const auto dockedToId = node->getDockedTo();
        auto dockedToNode = getNode(dockedToId);

        if (!dockedToNode) {
            BESS_ERROR("Docked to node {} not found for undocking node {}",
                       dockedToId,
                       nodeId);
            return false;
        }

        bool res = false;
        switch (dockedToNode->getNodeType()) {
        case DockNodeType::leaf:
            BESS_ERROR("Cannot undock from a leaf node");
            return false;
        case DockNodeType::tab:
            res = undockFromTab(nodeId, dockedToNode);
            break;
        case DockNodeType::split:
            res = undockFromSplitter(nodeId, dockedToNode);
            break;
        default:
            BESS_ERROR("Unknown docked to node type {} for node {}",
                       static_cast<int>(dockedToNode->getNodeType()),
                       nodeId);
            return false;
        }

        if (res) {
            node->setDockedTo(UUID::null);
            m_layoutDirty = true;
        }

        return res;
    }

    void DockManager::init() {
        m_rootNode = UUID::null;
        m_nodes.clear();
        m_rects.clear();
    }

    void DockManager::setSize(const glm::vec2 &size) {
        auto rootNode = getNode(m_rootNode);
        if (rootNode) {
            rootNode->setSize(size);
            m_layoutDirty = true;
        }
    }

    void DockManager::layoutNode(const std::shared_ptr<IDockNode> &node) {
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

    void DockManager::layoutTabNode(const std::shared_ptr<IDockNode> &node) {
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

    void DockManager::layoutSplitNode(const std::shared_ptr<IDockNode> &node) {
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

    bool
    DockManager::replaceWithSplitter(const std::shared_ptr<IDockNode> &node,
                                     const std::shared_ptr<IDockNode> &target,
                                     DockZone zone) {
        BESS_ASSERT(node && target, "Invalid node or target");
        BESS_ASSERT(zone != DockZone::main,
                    "Cannot dock to main zone when replacing with splitter");

        auto splitterNode = replaceWithNew<DockSplitter>(target);
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

    bool DockManager::dockToLeaf(const std::shared_ptr<IDockNode> &node,
                                 const std::shared_ptr<IDockNode> &target,
                                 DockZone zone) {
        BESS_ASSERT(node && target, "Invalid node or target");
        BESS_ASSERT(target->isLeaf(), "Target must be a leaf node");

        if (zone == DockZone::main) {
            // Change the target node to a tab node
            auto tabNode = replaceWithNew<DockTab>(target);
            tabNode->setDockedNodes({target->getId(), node->getId()});
        } else {
            // Change the target node to a splitter node
            return replaceWithSplitter(node, target, zone);
        }

        return true;
    }

    bool DockManager::dockToTab(const std::shared_ptr<IDockNode> &node,
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

    bool DockManager::dockToSplitter(const std::shared_ptr<IDockNode> &node,
                                     const std::shared_ptr<IDockNode> &target,
                                     DockZone zone) {
        BESS_ASSERT(node && target, "Invalid node or target");
        BESS_ASSERT(target->isSplitter(), "Target must be a splitter node");
        BESS_ASSERT(zone != DockZone::main,
                    "Cannot dock to main zone when docking to splitter");

        return replaceWithSplitter(node, target, zone);
    }

    bool DockManager::undockFromTab(const UUID &node,
                                    const std::shared_ptr<IDockNode> &target) {
        BESS_ASSERT(target && target->isTab(), "Target must be a tab node");
        auto tabNode = std::dynamic_pointer_cast<DockTab>(target);
        BESS_ASSERT(tabNode, "Failed to cast target to DockTab");

        auto &dockedNodes = tabNode->getDockedNodes();
        dockedNodes.erase(std::ranges::remove(dockedNodes, node).begin(),
                          dockedNodes.end());

        if (dockedNodes.size() == 1) {
            // If only one node remains, change the tab node back to a leaf node
            auto remainingNodeId = dockedNodes.front();
            auto remainingNode = getNode(remainingNodeId);
            BESS_ASSERT(remainingNode, "Remaining node not found");
            replaceNode(remainingNode, tabNode);
            eraseNode(tabNode->getId());
        }

        return true;
    }

    bool
    DockManager::undockFromSplitter(const UUID &node,
                                    const std::shared_ptr<IDockNode> &target) {
        BESS_ASSERT(target && target->isSplitter(),
                    "Target must be a splitter node");
        auto splitterNode = std::dynamic_pointer_cast<DockSplitter>(target);
        BESS_ASSERT(splitterNode, "Failed to cast target to DockSplitter");

        auto splitNodes = splitterNode->getSplitNodes();
        if (splitNodes.first == node) {
            // If the first node is being undocked, replace the splitter with
            // the second node
            auto secondNode = getNode(splitNodes.second);
            BESS_ASSERT(secondNode, "Second node not found");
            replaceNode(secondNode, splitterNode);
            eraseNode(splitterNode->getId());
        } else if (splitNodes.second == node) {
            // If the second node is being undocked, replace the splitter with
            // the first node
            auto firstNode = getNode(splitNodes.first);
            BESS_ASSERT(firstNode, "First node not found");
            replaceNode(firstNode, splitterNode);
            eraseNode(splitterNode->getId());
        } else {
            BESS_ERROR("Node {} is not a child of splitter {}",
                       node,
                       splitterNode->getId());
            return false;
        }

        return true;
    }

    void DockManager::eraseNode(const UUID &nodeId) {
        auto it = m_nodes.find(nodeId);
        if (it != m_nodes.end()) {
            m_nodes.erase(it);
        }
    }

    void DockManager::setRootNode(const std::shared_ptr<IDockNode> &node) {
        m_nodes.erase(m_rootNode);

        m_rootNode = node->getId();
        m_nodes[m_rootNode] = node;
    }

    bool DockManager::replaceNode(const std::shared_ptr<IDockNode> &newNode,
                                  const std::shared_ptr<IDockNode> &oldNode) {
        BESS_ASSERT(newNode && oldNode, "Invalid new or old node");
        newNode->setId(oldNode->getId());
        newNode->setDockedTo(oldNode->getDockedTo());
        newNode->setPos(oldNode->getPos());
        newNode->setSize(oldNode->getSize());

        m_nodes[oldNode->getId()] = newNode;

        oldNode->setId(UUID()); // Assign a new ID to the old node
        m_nodes[oldNode->getId()] = oldNode;
        return true;
    }

} // namespace Bess::UI
