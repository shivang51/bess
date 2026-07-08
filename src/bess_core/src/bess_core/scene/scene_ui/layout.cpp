#include "bess_core/scene/scene_ui/layout.h"
#include "common/bess_assert.h"
#include <algorithm>
#include <cmath>
#include <ranges>

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

        bool sameVec(const glm::vec4 &lhs, const glm::vec4 &rhs) {
            return lhs.x == rhs.x && lhs.y == rhs.y && lhs.z == rhs.z &&
                   lhs.w == rhs.w;
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
        m_nodes.at(node.getId()).attachRegistry(this);
        return &m_nodes.at(node.getId());
    }

    UINode *UINodeRegistry::addNode(const UUID &nodeId) {
        auto [itr, success] = m_nodes.try_emplace(nodeId, nodeId);
        if (!success) {
            return nullptr;
        }
        itr->second.attachRegistry(this);
        return &itr->second;
    }

    void UINodeRegistry::removeNode(const UUID &id) {
        auto it = m_nodes.find(id);
        if (it == m_nodes.end()) {
            return;
        }

        UINode &node = it->second;
        if (node.m_parentId != UUID::null) {
            if (auto *parentNode = getNode(node.m_parentId)) {
                parentNode->m_children.erase(id);
                parentNode->setSizeDirty();
            }
        }

        for (const auto &childId : node.m_children) {
            if (auto *childNode = getNode(childId)) {
                childNode->m_parentId = UUID::null;
                childNode->setPosDirty();
            }
        }

        node.attachRegistry(nullptr);
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
        if (dirty) {
            propagatePosDirtyToAncestors();
        }
    }

    void UINode::setSizeDirty(bool dirty) {
        m_sizeDirty = dirty;
        if (dirty) {
            m_posDirty = true;
            propagateSizeDirtyToAncestors();
        }
    }

    bool UINode::getPosDirty() const {
        return m_posDirty;
    }

    bool UINode::getSizeDirty() const {
        return m_sizeDirty;
    }

    const glm::vec2 &UINode::getPos() const {
        return m_pos;
    }

    void UINode::setPos(const glm::vec2 &pos) {
        if (sameVec(m_pos, pos)) {
            return;
        }
        m_pos = pos;
        setPosDirty();
    }

    glm::vec2 &UINode::getPos() {
        setPosDirty();
        return m_pos;
    }

    const Unit &UINode::getPosUnit() const {
        return m_posUnit;
    }

    void UINode::setPosUnit(const Unit &posUnit) {
        if (m_posUnit == posUnit) {
            return;
        }
        m_posUnit = posUnit;
        setPosDirty();
    }

    Unit &UINode::getPosUnit() {
        setPosDirty();
        return m_posUnit;
    }

    const glm::vec2 &UINode::getSize() const {
        return m_size;
    }

    void UINode::setSize(const glm::vec2 &size) {
        if (sameVec(m_size, size)) {
            return;
        }
        m_size = size;
        setSizeDirty();
    }

    glm::vec2 &UINode::getSize() {
        setSizeDirty();
        return m_size;
    }

    const Unit &UINode::getSizeUnit() const {
        return m_sizeUnit;
    }

    void UINode::setSizeUnit(const Unit &sizeUnit) {
        if (m_sizeUnit == sizeUnit) {
            return;
        }
        m_sizeUnit = sizeUnit;
        setSizeDirty();
    }

    Unit &UINode::getSizeUnit() {
        setSizeDirty();
        return m_sizeUnit;
    }

    const SizeContraint &UINode::getSizeConstraint() const {
        return m_sizeConstraint;
    }

    void UINode::setSizeConstraint(const SizeContraint &sizeConstraint) {
        if (m_sizeConstraint == sizeConstraint) {
            return;
        }
        m_sizeConstraint = sizeConstraint;
        setSizeDirty();
    }

    SizeContraint &UINode::getSizeConstraint() {
        setSizeDirty();
        return m_sizeConstraint;
    }

    const Core::Style::Padding &UINode::getPadding() const {
        return m_padding;
    }

    void UINode::setPadding(const Core::Style::Padding &padding) {
        if (m_padding == padding) {
            return;
        }
        m_padding = padding;
        setSizeDirty();
    }

    Core::Style::Padding &UINode::getPadding() {
        setSizeDirty();
        return m_padding;
    }

    const Core::Style::Margin &UINode::getMargin() const {
        return m_margin;
    }

    void UINode::setMargin(const Core::Style::Margin &margin) {
        if (m_margin == margin) {
            return;
        }
        m_margin = margin;
        setSizeDirty();
    }

    Core::Style::Margin &UINode::getMargin() {
        setSizeDirty();
        return m_margin;
    }

    const glm::vec2 &UINode::getMinSize() const {
        return m_minSize;
    }

    void UINode::setMinSize(const glm::vec2 &minSize) {
        if (sameVec(m_minSize, minSize)) {
            return;
        }
        m_minSize = minSize;
        setSizeDirty();
    }

    glm::vec2 &UINode::getMinSize() {
        setSizeDirty();
        return m_minSize;
    }

    const glm::vec2 &UINode::getMaxSize() const {
        return m_maxSize;
    }

    void UINode::setMaxSize(const glm::vec2 &maxSize) {
        if (sameVec(m_maxSize, maxSize)) {
            return;
        }
        m_maxSize = maxSize;
        setSizeDirty();
    }

    glm::vec2 &UINode::getMaxSize() {
        setSizeDirty();
        return m_maxSize;
    }

    const glm::vec2 &UINode::getCachedPos() const {
        return m_cachedPos;
    }

    const glm::vec2 &UINode::getCachedSize() const {
        return m_cachedSize;
    }

    const glm::vec2 &UINode::getDrawSize() const {
        return m_drawSize;
    }

    const float &UINode::getCachedZVal() const {
        return m_cachedZVal;
    }

    const UUID &UINode::getParentId() const {
        return m_parentId;
    }

    const LayoutDirection &UINode::getDirection() const {
        return m_direction;
    }

    void UINode::setDirection(const LayoutDirection &direction) {
        if (m_direction == direction) {
            return;
        }
        m_direction = direction;
        setSizeDirty();
    }

    LayoutDirection &UINode::getDirection() {
        setSizeDirty();
        return m_direction;
    }

    const LayoutAlignment &UINode::getMainAxisAlignment() const {
        return m_mainAxisAlignment;
    }

    void UINode::setMainAxisAlignment(const LayoutAlignment &alignment) {
        if (m_mainAxisAlignment == alignment) {
            return;
        }
        m_mainAxisAlignment = alignment;
        setPosDirty();
    }

    LayoutAlignment &UINode::getMainAxisAlignment() {
        setPosDirty();
        return m_mainAxisAlignment;
    }

    const LayoutAlignment &UINode::getCrossAxisAlignment() const {
        return m_crossAxisAlignment;
    }

    void UINode::setCrossAxisAlignment(const LayoutAlignment &alignment) {
        if (m_crossAxisAlignment == alignment) {
            return;
        }
        m_crossAxisAlignment = alignment;
        setPosDirty();
    }

    LayoutAlignment &UINode::getCrossAxisAlignment() {
        setPosDirty();
        return m_crossAxisAlignment;
    }

    const PosMode &UINode::getPosMode() const {
        return m_posMode;
    }

    void UINode::setPosMode(const PosMode &posMode) {
        if (m_posMode == posMode) {
            return;
        }
        m_posMode = posMode;
        setSizeDirty();
    }

    PosMode &UINode::getPosMode() {
        setSizeDirty();
        return m_posMode;
    }

    const OrderedSet<UUID> &UINode::getChildren() const {
        return m_children;
    }

    void UINode::setChildren(const OrderedSet<UUID> &children) {
        if (m_children == children) {
            return;
        }

        if (m_registry != nullptr) {
            for (const auto &childId : m_children) {
                if (auto *childNode = m_registry->getNode(childId)) {
                    childNode->m_parentId = UUID::null;
                    childNode->setPosDirty();
                }
            }
        }

        m_children = children;
        if (m_registry != nullptr) {
            for (const auto &childId : m_children) {
                if (auto *childNode = m_registry->getNode(childId)) {
                    childNode->m_parentId = m_id;
                    childNode->setPosDirty();
                }
            }
        }
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
        if (m_zVal == zVal) {
            return;
        }
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
            node->setPosDirty();
            setSizeDirty();
        }
    }

    void UINode::removeChild(UINode *node) {
        BESS_ASSERT(node != nullptr, "Cannot remove a null UI node child.");
        if (m_children.erase(node->m_id) > 0) {
            node->m_parentId = UUID::null;
            node->setPosDirty();
            setSizeDirty();
        }
    }

    void UINode::clearChildren() {
        if (!m_children.empty()) {
            if (m_registry != nullptr) {
                for (const auto &childId : m_children) {
                    if (auto *childNode = m_registry->getNode(childId)) {
                        childNode->m_parentId = UUID::null;
                        childNode->setPosDirty();
                    }
                }
            }
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

    glm::vec3 UINode::getDrawPos() const {
        return {m_cachedPos, m_cachedZVal};
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

        if (!m_sizeDirty) {
            if (tracksCycle) {
                activeNodes.erase(m_id);
            }
            return m_cachedSize;
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

            if (m_direction == LayoutDirection::horizontal ||
                m_direction == LayoutDirection::horizontalReverse) {
                childrenSpan->x += childSize.x;
                childrenSpan->y = std::max(childrenSpan->y, childSize.y);
            } else {
                childrenSpan->y += childSize.y;
                childrenSpan->x = std::max(childrenSpan->x, childSize.x);
            }
        };

        if (m_sizeConstraint == SizeContraint::fixed) {
            auto size = resolveSize(parentNode);
            auto tentativeSize = size;

            if (m_size.x < 0.f) {
                tentativeSize.x = 0.f;
            }
            if (m_size.y < 0.f) {
                tentativeSize.y = 0.f;
            }

            m_drawSize = constrainSize(tentativeSize, m_minSize, m_maxSize);

            glm::vec2 childrenSpan{0.f};
            for (const auto &childId : m_children) {
                measureChild(childId, &childrenSpan);
            }

            const auto minContentSize = childrenSpan + paddingSize();

            if (m_size.x < 0.f) {
                size.x = minContentSize.x;
            } else {
                size.x = std::max(size.x, minContentSize.x);
            }
            if (m_size.y < 0.f) {
                size.y = minContentSize.y;
            } else {
                size.y = std::max(size.y, minContentSize.y);
            }

            drawSize = constrainSize(size, m_minSize, m_maxSize);
            m_drawSize = drawSize;

        } else {
            m_drawSize = constrainSize(paddingSize(), m_minSize, m_maxSize);

            glm::vec2 childrenSpan{0.f};
            for (const auto &childId : m_children) {
                measureChild(childId, &childrenSpan);
                m_drawSize = constrainSize(
                    childrenSpan + paddingSize(), m_minSize, m_maxSize);
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

        if (!m_posDirty) {
            if (tracksCycle) {
                activeNodes.erase(m_id);
            }
            return;
        }

        if (parentNode == nullptr) {
            m_cachedPos = resolvePos(nullptr);
        } else if (m_posMode == PosMode::absolute) {
            m_cachedPos = parentNode->m_cachedPos + resolvePos(parentNode);
        }

        m_cachedZVal = parentZVal + m_zVal;

        const glm::vec2 contentTopLeft = m_cachedPos - (m_drawSize * 0.5f) +
                                         edgeTopLeft(m_padding.toVec4(), false);
        const glm::vec2 availableContentSize = contentSize();
        const bool isHorizontal =
            m_direction == LayoutDirection::horizontal ||
            m_direction == LayoutDirection::horizontalReverse;

        const bool isReverse =
            m_direction == LayoutDirection::horizontalReverse ||
            m_direction == LayoutDirection::verticalReverse;

        float childrenMainSpan = 0.f;

        // Its just span collection so order does not matter
        // Skipping isReverse here
        for (const auto &childId : m_children) {
            const UINode *childNode = registry.getNode(childId);
            if (childNode == nullptr ||
                childNode->m_posMode == PosMode::absolute) {
                continue;
            }

            childrenMainSpan += isHorizontal ? childNode->m_cachedSize.x
                                             : childNode->m_cachedSize.y;
        }

        const float contentMainStart =
            isHorizontal ? contentTopLeft.x : contentTopLeft.y;
        const float availableMainSize =
            isHorizontal ? availableContentSize.x : availableContentSize.y;
        float cursor = alignedStart(contentMainStart,
                                    availableMainSize,
                                    childrenMainSpan,
                                    m_mainAxisAlignment);

        auto processChild = [&](UINode *childNode) {
            if (childNode->m_posMode == PosMode::absolute) {
                const auto childPos = m_cachedPos + childNode->resolvePos(this);
                const auto childZ = m_cachedZVal + childNode->m_zVal;
                if (!sameVec(childNode->m_cachedPos, childPos) ||
                    childNode->m_cachedZVal != childZ) {
                    childNode->m_posDirty = true;
                }
                childNode->m_cachedPos = childPos;
                childNode->layout(registry, this, m_cachedZVal, activeNodes);
                return;
            }

            glm::vec2 marginBoxTopLeft{0.f};
            if (isHorizontal) {
                marginBoxTopLeft = {cursor, contentTopLeft.y};
                marginBoxTopLeft.y = alignedStart(contentTopLeft.y,
                                                  availableContentSize.y,
                                                  childNode->m_cachedSize.y,
                                                  m_crossAxisAlignment);
            } else {
                marginBoxTopLeft = {contentTopLeft.x, cursor};
                marginBoxTopLeft.x = alignedStart(contentTopLeft.x,
                                                  availableContentSize.x,
                                                  childNode->m_cachedSize.x,
                                                  m_crossAxisAlignment);
            }

            const glm::vec2 drawTopLeft =
                marginBoxTopLeft +
                edgeTopLeft(childNode->m_margin.toVec4(), true);
            const auto childPos = drawTopLeft + (childNode->m_drawSize * 0.5f) +
                                  childNode->resolvePos(this);
            const auto childZ = m_cachedZVal + childNode->m_zVal;
            if (!sameVec(childNode->m_cachedPos, childPos) ||
                childNode->m_cachedZVal != childZ) {
                childNode->m_posDirty = true;
            }
            childNode->m_cachedPos = childPos;

            childNode->layout(registry, this, m_cachedZVal, activeNodes);

            if (isHorizontal) {
                cursor += childNode->m_cachedSize.x;
            } else {
                cursor += childNode->m_cachedSize.y;
            }
        };

        if (isReverse) {
            for (auto it : std::views::reverse(m_children)) {
                UINode *childNode = registry.getNode(it);
                BESS_ASSERT(childNode,
                            "Child node {} not found in registry.",
                            static_cast<uint64_t>(it));
                if (childNode == nullptr) {
                    continue;
                }
                processChild(childNode);
            }
        } else {
            for (const auto &childId : m_children) {
                UINode *childNode = registry.getNode(childId);
                BESS_ASSERT(childNode,
                            "Child node {} not found in registry.",
                            static_cast<uint64_t>(childId));
                if (childNode == nullptr) {
                    continue;
                }
                processChild(childNode);
            }
        }

        m_posDirty = false;

        if (tracksCycle) {
            activeNodes.erase(m_id);
        }
    }

    void UINode::attachRegistry(UINodeRegistry *registry) {
        m_registry = registry;
    }

    void UINode::propagateSizeDirtyToAncestors() {
        if (m_registry == nullptr) {
            return;
        }

        UUID parentId = m_parentId;
        while (parentId != UUID::null) {
            UINode *parentNode = m_registry->getNode(parentId);
            if (parentNode == nullptr) {
                return;
            }

            parentNode->m_sizeDirty = true;
            parentNode->m_posDirty = true;
            parentId = parentNode->m_parentId;
        }
    }

    void UINode::propagatePosDirtyToAncestors() {
        if (m_registry == nullptr) {
            return;
        }

        UUID parentId = m_parentId;
        while (parentId != UUID::null) {
            UINode *parentNode = m_registry->getNode(parentId);
            if (parentNode == nullptr) {
                return;
            }

            parentNode->m_posDirty = true;
            parentId = parentNode->m_parentId;
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
        return edgeSize(m_margin.toVec4(), true);
    }

    glm::vec2 UINode::paddingSize() const {
        return edgeSize(m_padding.toVec4(), false);
    }

} // namespace Bess::Canvas::UI
