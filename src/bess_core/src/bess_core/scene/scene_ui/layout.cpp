#include "bess_core/scene/scene_ui/layout.h"
#include "common/bess_assert.h"
#include "common/logger.h"
#include <algorithm>
#include <cmath>

namespace Bess::Canvas::UI {
    namespace {
        float finiteOrZero(float value) {
            return std::isfinite(value) ? value : 0.f;
        }

        float nonNegative(float value) {
            return std::max(0.f, finiteOrZero(value));
        }

        glm::vec2 finiteVec(const glm::vec2 &value) {
            return {finiteOrZero(value.x), finiteOrZero(value.y)};
        }

        glm::vec2 nonNegativeVec(const glm::vec2 &value) {
            return {nonNegative(value.x), nonNegative(value.y)};
        }

        float relativeUnit(float value) {
            return std::clamp(finiteOrZero(value), 0.f, 1.f);
        }

        glm::vec2 relativeVec(const glm::vec2 &value) {
            return {relativeUnit(value.x), relativeUnit(value.y)};
        }

        float edge(float value, bool allowNegative) {
            value = finiteOrZero(value);
            return allowNegative ? value : std::max(0.f, value);
        }

        // glm::vec4 edges are top, right, bottom, left.
        glm::vec2 edgeSize(const glm::vec4 &edges, bool allowNegative) {
            return {
                edge(edges.y, allowNegative) + edge(edges.w, allowNegative),
                edge(edges.x, allowNegative) + edge(edges.z, allowNegative),
            };
        }

        glm::vec2 edgeTopLeft(const glm::vec4 &edges, bool allowNegative) {
            return {
                edge(edges.w, allowNegative),
                edge(edges.x, allowNegative),
            };
        }

        glm::vec2 constrainSize(glm::vec2 size,
                                const glm::vec2 &minSize,
                                const glm::vec2 &maxSize) {
            size = nonNegativeVec(size);

            if (std::isfinite(maxSize.x) && maxSize.x >= 0.f) {
                size.x = std::min(size.x, maxSize.x);
            }
            if (std::isfinite(maxSize.y) && maxSize.y >= 0.f) {
                size.y = std::min(size.y, maxSize.y);
            }

            // Minimum size wins over max size, matching CSS min/max behavior.
            if (std::isfinite(minSize.x) && minSize.x >= 0.f) {
                size.x = std::max(size.x, minSize.x);
            }
            if (std::isfinite(minSize.y) && minSize.y >= 0.f) {
                size.y = std::max(size.y, minSize.y);
            }

            return nonNegativeVec(size);
        }

        bool sameVec(const glm::vec2 &lhs, const glm::vec2 &rhs) {
            return lhs.x == rhs.x && lhs.y == rhs.y;
        }

        float alignedStart(float start,
                           float availableSize,
                           float childSize,
                           LayoutAlignment alignment) {
            switch (alignment) {
            case LayoutAlignment::center:
                return start + ((availableSize - childSize) * 0.5f);
            case LayoutAlignment::end:
                return start + availableSize - childSize;
            case LayoutAlignment::start:
                return start;
            }

            return start;
        }
    } // namespace

    UINode *UINodeRegistry::addNode(const UINode &node) {
        m_nodes[node.getId()] = node;
        return &m_nodes.at(node.getId());
    }

    UINode *UINodeRegistry::addNode(const UUID &nodeId) {
        auto [itr, success] = m_nodes.try_emplace(nodeId, nodeId);
        if (!success) {
            return nullptr;
        }
        return &itr->second;
    }

    void UINodeRegistry::removeNode(const UUID &id) {
        m_nodes.erase(id);
    }

    UINode *UINodeRegistry::getNode(const UUID &id) {
        auto it = m_nodes.find(id);
        if (it != m_nodes.end()) {
            return &it->second;
        }
        return nullptr;
    }

    const UINode *UINodeRegistry::getNode(const UUID &id) const {
        auto it = m_nodes.find(id);
        if (it != m_nodes.end()) {
            return &it->second;
        }
        return nullptr;
    }

    void UINodeRegistry::clear() {
        m_nodes.clear();
    }

    UINode::UINode(const UUID &id) : m_id(id) {
    }

    void UINode::setPosDirty(bool dirty) {
        m_posDirty = dirty;
    }

    void UINode::setSizeDirty(bool dirty) {
        m_sizeDirty = dirty;
        if (dirty) {
            m_posDirty = true;
        }
    }

    const LayoutDirection &UINode::getDirection() const {
        return m_direction;
    }

    void UINode::setDirection(const LayoutDirection &direction) {
        m_direction = direction;
        setSizeDirty();
    }

    LayoutDirection &UINode::getDirection() {
        return m_direction;
    }

    const LayoutAlignment &UINode::getAlignment() const {
        return m_alignment;
    }

    void UINode::setAlignment(const LayoutAlignment &alignment) {
        m_alignment = alignment;
        setPosDirty();
    }

    LayoutAlignment &UINode::getAlignment() {
        return m_alignment;
    }

    const PosMode &UINode::getPosMode() const {
        return m_posMode;
    }

    void UINode::setPosMode(const PosMode &posMode) {
        m_posMode = posMode;
        setSizeDirty();
    }

    PosMode &UINode::getPosMode() {
        return m_posMode;
    }

    const OrderedSet<UUID> &UINode::getChildren() const {
        return m_children;
    }

    void UINode::setChildren(const OrderedSet<UUID> &children) {
        m_children = children;
        setSizeDirty();
    }

    OrderedSet<UUID> &UINode::getChildren() {
        setSizeDirty();
        return m_children;
    }

    const float &UINode::getZVal() const {
        return m_zVal;
    }

    void UINode::setZVal(const float &zVal) {
        m_zVal = zVal;
        setPosDirty();
    }

    float &UINode::getZVal() {
        setPosDirty();
        return m_zVal;
    }

    void UINode::addChild(UINode *node) {
        BESS_ASSERT(node != nullptr, "Cannot add a null UI node child.");
        BESS_ASSERT(node->m_id != m_id,
                    "Cannot add a UI node as its own child.");

        if (node == nullptr || node->m_id == m_id) {
            return;
        }

        const auto previousSize = m_children.size();
        m_children.insert(node->m_id);
        if (m_children.size() != previousSize) {
            node->m_parentId = m_id;
            setSizeDirty();
        }
    }

    void UINode::removeChild(UINode *node) {
        BESS_ASSERT(node != nullptr, "Cannot remove a null UI node child.");
        if (m_children.erase(node->m_id) > 0) {
            node->m_parentId = UUID::null;
            setSizeDirty();
        }
    }

    void UINode::clearChildren() {
        if (!m_children.empty()) {
            m_children.clear();
            setSizeDirty();
        }
    }

    glm::vec2 UINode::measure(UINodeRegistry &registry, const UUID &parentId) {
        UINode *parentNode = nullptr;
        if (parentId != UUID::null) {
            parentNode = registry.getNode(parentId);
            BESS_ASSERT(parentNode,
                        "Parent node {} not found in registry.",
                        static_cast<uint64_t>(parentId));
        }

        HashSet<UUID> activeNodes;
        return measure(registry, parentNode, activeNodes);
    }

    void UINode::layout(UINodeRegistry &registry, const UUID &parentId) {
        UINode *parentNode = nullptr;
        if (parentId != UUID::null) {
            parentNode = registry.getNode(parentId);
            BESS_ASSERT(parentNode,
                        "Parent node {} not found in registry.",
                        static_cast<uint64_t>(parentId));
        }

        HashSet<UUID> measureStack;
        measure(registry, parentNode, measureStack);

        HashSet<UUID> activeNodes;
        layout(registry,
               parentNode,
               parentNode != nullptr ? parentNode->m_cachedZVal : 0.f,
               activeNodes);
    }

    glm::vec2 UINode::measure(UINodeRegistry &registry,
                              const UINode *parentNode,
                              HashSet<UUID> &activeNodes) {
        const bool tracksCycle = m_id != UUID::null;
        if (tracksCycle) {
            if (activeNodes.find(m_id) != activeNodes.end()) {
                BESS_ASSERT(false,
                            "Cycle detected while measuring UI node {}.",
                            static_cast<uint64_t>(m_id));
                return nonNegativeVec(m_cachedSize);
            }
            activeNodes.insert(m_id);
        }

        const glm::vec2 previousCachedSize = m_cachedSize;
        const glm::vec2 previousDrawSize = m_drawSize;
        bool childLayoutChanged = false;
        glm::vec2 drawSize{0.f};

        auto measureChild = [&](const UUID &childId, glm::vec2 *childrenSpan) {
            UINode *childNode = registry.getNode(childId);
            BESS_ASSERT(childNode,
                        "Child node {} not found in registry.",
                        static_cast<uint64_t>(childId));
            if (childNode == nullptr) {
                return;
            }

            const glm::vec2 childPreviousSize = childNode->m_cachedSize;
            const glm::vec2 childSize =
                childNode->measure(registry, this, activeNodes);
            if (!sameVec(childPreviousSize, childSize) ||
                childNode->m_posDirty) {
                childLayoutChanged = true;
            }

            if (childrenSpan == nullptr ||
                childNode->m_posMode == PosMode::absolute) {
                return;
            }

            if (m_direction == LayoutDirection::horizontal) {
                childrenSpan->x += childSize.x;
                childrenSpan->y = std::max(childrenSpan->y, childSize.y);
            } else {
                childrenSpan->y += childSize.y;
                childrenSpan->x = std::max(childrenSpan->x, childSize.x);
            }
        };

        if (m_sizeConstraint == SizeContraint::fixed) {
            drawSize =
                constrainSize(resolveSize(parentNode), m_minSize, m_maxSize);
            m_drawSize = drawSize;

            for (const auto &childId : m_children) {
                measureChild(childId, nullptr);
            }
        } else {
            glm::vec2 childrenSpan{0.f};
            for (const auto &childId : m_children) {
                measureChild(childId, &childrenSpan);
            }

            drawSize = constrainSize(
                childrenSpan + paddingSize(), m_minSize, m_maxSize);
        }

        m_drawSize = drawSize;
        m_cachedSize = nonNegativeVec(drawSize + marginSize());

        if (m_sizeDirty || childLayoutChanged ||
            !sameVec(previousCachedSize, m_cachedSize) ||
            !sameVec(previousDrawSize, m_drawSize)) {
            m_posDirty = true;
        }
        m_sizeDirty = false;

        if (tracksCycle) {
            activeNodes.erase(m_id);
        }

        return m_cachedSize;
    }

    void UINode::layout(UINodeRegistry &registry,
                        const UINode *parentNode,
                        float parentZVal,
                        HashSet<UUID> &activeNodes) {
        const bool tracksCycle = m_id != UUID::null;
        if (tracksCycle) {
            if (activeNodes.find(m_id) != activeNodes.end()) {
                BESS_ASSERT(false,
                            "Cycle detected while laying out UI node {}.",
                            static_cast<uint64_t>(m_id));
                return;
            }
            activeNodes.insert(m_id);
        }

        if (parentNode == nullptr) {
            m_cachedPos = resolvePos(nullptr);
        } else if (m_posMode == PosMode::absolute) {
            m_cachedPos = parentNode->m_cachedPos + resolvePos(parentNode);
        }

        m_cachedZVal = parentZVal + m_zVal;

        const glm::vec2 contentTopLeft =
            m_cachedPos - (m_drawSize * 0.5f) + edgeTopLeft(m_padding, false);
        const glm::vec2 availableContentSize = contentSize();
        glm::vec2 cursor = contentTopLeft;

        for (const auto &childId : m_children) {
            UINode *childNode = registry.getNode(childId);
            BESS_ASSERT(childNode,
                        "Child node {} not found in registry.",
                        static_cast<uint64_t>(childId));
            if (childNode == nullptr) {
                continue;
            }

            if (childNode->m_posMode == PosMode::absolute) {
                childNode->m_cachedPos =
                    m_cachedPos + childNode->resolvePos(this);
                childNode->layout(registry, this, m_cachedZVal, activeNodes);
                continue;
            }

            glm::vec2 marginBoxTopLeft = cursor;
            if (m_direction == LayoutDirection::horizontal) {
                marginBoxTopLeft.y = alignedStart(contentTopLeft.y,
                                                  availableContentSize.y,
                                                  childNode->m_cachedSize.y,
                                                  m_alignment);
            } else {
                marginBoxTopLeft.x = alignedStart(contentTopLeft.x,
                                                  availableContentSize.x,
                                                  childNode->m_cachedSize.x,
                                                  m_alignment);
            }

            const glm::vec2 drawTopLeft =
                marginBoxTopLeft + edgeTopLeft(childNode->m_margin, true);
            childNode->m_cachedPos = drawTopLeft +
                                     (childNode->m_drawSize * 0.5f) +
                                     childNode->resolvePos(this);

            childNode->layout(registry, this, m_cachedZVal, activeNodes);

            if (m_direction == LayoutDirection::horizontal) {
                cursor.x += childNode->m_cachedSize.x;
            } else {
                cursor.y += childNode->m_cachedSize.y;
            }
        }

        m_posDirty = false;

        if (tracksCycle) {
            activeNodes.erase(m_id);
        }
    }

    glm::vec2 UINode::resolveSize(const UINode *parentNode) const {
        if (m_sizeUnit == Unit::pixel) {
            return nonNegativeVec(m_size);
        }

        BESS_ASSERT(parentNode != nullptr,
                    "Relative size requires a parent node.");
        if (parentNode == nullptr) {
            return {0.f, 0.f};
        }

        return relativeVec(m_size) * parentNode->contentSize();
    }

    glm::vec2 UINode::resolvePos(const UINode *parentNode) const {
        if (m_posUnit == Unit::pixel) {
            return finiteVec(m_pos);
        }

        BESS_ASSERT(parentNode != nullptr,
                    "Relative position requires a parent node.");
        if (parentNode == nullptr) {
            return {0.f, 0.f};
        }

        return finiteVec(m_pos) * parentNode->contentSize();
    }

    glm::vec2 UINode::contentSize() const {
        return nonNegativeVec(m_drawSize - paddingSize());
    }

    glm::vec2 UINode::marginSize() const {
        return edgeSize(m_margin, true);
    }

    glm::vec2 UINode::paddingSize() const {
        return edgeSize(m_padding, false);
    }

} // namespace Bess::Canvas::UI
