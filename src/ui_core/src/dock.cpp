#include "dock.h"
#include "common/bess_assert.h"
#include "common/logger.h"

namespace Bess::UI {
    UUID DockManager::getHitRect(const glm::vec2 &point) {
        for (const auto &rect : m_rects) {
            if (rect.contains(point)) {
                return rect.id;
            }
        }
        return UUID::null;
    }

    void DockManager::layout() {
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

        m_rects.clear();
        m_rects.reserve(m_nodes.size());

        for (const auto &[id, node] : m_nodes) {
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
        auto target = getNode(targetId);

        bool res = false;
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

    bool DockManager::dockToLeaf(const std::shared_ptr<IDockNode> &node,
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

} // namespace Bess::UI
