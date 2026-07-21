#pragma once

#include "common/bess_assert.h"
#include "common/bess_uuid.h"
#include "common/class_helpers.h"
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

        bool isFloating() const {
            return m_dockedTo == UUID::null;
        }

        bool isRoot() const {
            return m_id == m_dockedTo;
        }

        MAKE_GETTER_SETTER(DockNodeType, NodeType, m_nodeType)
        MAKE_GETTER_SETTER(bool, AllowsDocking, m_allowsDocking)
        MAKE_GETTER_SETTER(UUID, Id, m_id)
        MAKE_GETTER_SETTER(glm::vec2, Pos, m_pos)
        MAKE_GETTER_SETTER(glm::vec2, Size, m_size)
        MAKE_GETTER_SETTER(UUID, DockedTo, m_dockedTo)

      protected:
        UUID m_dockedTo = UUID::null;
        bool m_allowsDocking = false;
        DockNodeType m_nodeType = DockNodeType::leaf;
        UUID m_id;
        glm::vec2 m_pos = glm::vec2(0.0f, 0.0f);
        glm::vec2 m_size = glm::vec2(0.0f, 0.0f);
    };

    class DockSplitter : public IDockNode {
      public:
        DockSplitter() {
            m_nodeType = DockNodeType::split;
        }

        DockSplitter(SplitDirection dir, float ratio)
            : m_splitDir(dir),
              m_splitRatio(ratio) {
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
        SplitNodesType m_splitNodes{UUID::null, UUID::null};
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
    };

    struct DockRect {
        UUID id;
        glm::vec2 pos;
        glm::vec2 size;

        bool contains(const glm::vec2 &point) const {
            return point.x >= pos.x && point.x <= pos.x + size.x &&
                   point.y >= pos.y && point.y <= pos.y + size.y;
        }
    };

    class DockManager {
      public:
        DockManager() = default;

        void setRootNode(const std::shared_ptr<IDockNode> &node);

        UUID getHitRect(const glm::vec2 &point);

        void layout(bool force = false);

        bool dockNode(const UUID &nodeId, const UUID &targetId, DockZone zone);

        std::shared_ptr<IDockNode> getNode(const UUID &nodeId);

        bool undockNode(const UUID &nodeId);

        template <typename T, typename... Args>
            requires(std::is_base_of_v<IDockNode, T>)
        std::shared_ptr<T> getNode(const UUID &nodeId) {
            auto node = getNode(nodeId);
            if (!node) {
                return nullptr;
            }
            return std::dynamic_pointer_cast<T>(node);
        }

        // Call this before any op
        // Clears the list of nodes and rects
        void init();

        // Make sure init was called before calling this function
        void setSize(const glm::vec2 &size);

        // Top left position in the window
        void setPos(const glm::vec2 &pos);

        template <typename T, typename... Args>
            requires(std::is_base_of_v<IDockNode, T>)
        std::shared_ptr<T> createNode(Args &&...args) {
            auto node = std::make_shared<T>(std::forward<Args>(args)...);
            m_nodes[node->getId()] = node;
            return node;
        }

        MAKE_GETTER(UUID, RootNode, m_rootNode)

      private:
        void layoutNode(const std::shared_ptr<IDockNode> &node);

        void layoutTabNode(const std::shared_ptr<IDockNode> &node);

        void layoutSplitNode(const std::shared_ptr<IDockNode> &node);

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
            newNode->setDockedTo(node->getDockedTo());
            m_nodes[node->getId()] = newNode;

            return newNode;
        }

        template <typename T>
            requires(std::is_base_of_v<IDockNode, T>)
        std::shared_ptr<T>
        replaceWithNew(const std::shared_ptr<IDockNode> &node) {
            BESS_ASSERT(node, "Invalid node");

            auto newNode = changeNodeType<T>(node); // Change nodes type
            const auto replacementId = newNode->getId();
            node->setId(UUID());                    // assign new id
            node->setDockedTo(replacementId);
            m_nodes[node->getId()] = node; // store old node with new id
            reparentChildren(node);

            return newNode;
        }

        bool replaceNode(const std::shared_ptr<IDockNode> &newNode,
                         const std::shared_ptr<IDockNode> &oldNode);

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
                                 DockZone zone);

        // If zone is MAIN then the node will be changed to tab node and the
        // target will be added to the tab node Otherwise, a splitter will be
        // created
        bool dockToLeaf(const std::shared_ptr<IDockNode> &node,
                        const std::shared_ptr<IDockNode> &target,
                        DockZone zone);

        // If zone is MAIN then the node will be added to the children
        // Otherwise, split will be created
        bool dockToTab(const std::shared_ptr<IDockNode> &node,
                       const std::shared_ptr<IDockNode> &target,
                       DockZone zone);

        // Splitter will not allow docking to main zone
        bool dockToSplitter(const std::shared_ptr<IDockNode> &node,
                            const std::shared_ptr<IDockNode> &target,
                            DockZone zone);

        // Removes from the list of tabs
        // If the tab node has only two children it will be replaced with
        // the remaining child node
        bool undockFromTab(const UUID &nodeId,
                           const std::shared_ptr<IDockNode> &target);

        // Removes from the splitter node
        // And replaces the splitter node with the remaining child node
        bool undockFromSplitter(const UUID &nodeId,
                                const std::shared_ptr<IDockNode> &target);

        void eraseNode(const UUID &nodeId);
        void reparentChildren(const std::shared_ptr<IDockNode> &node);

      private:
        UUID m_rootNode = UUID::null;
        HashMap<UUID, std::shared_ptr<IDockNode>> m_nodes;
        bool m_layoutDirty = false;
        std::vector<DockRect> m_rects;
    };
} // namespace Bess::UI
