#include "bess_core/scene/scene_ui/layout.h"
#include "common/bess_assert.h"
#include "yoga/YGNodeLayout.h"
#include "yoga/YGNodeStyle.h"
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

        float percentValue(float value) {
            value = finiteOrZero(value);
            if (value <= 1.f) {
                return relativeUnit(value) * 100.f;
            }
            return std::clamp(value, 0.f, 100.f);
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

        bool sameVec(const glm::vec2 &lhs, const glm::vec2 &rhs) {
            return lhs.x == rhs.x && lhs.y == rhs.y;
        }

        YGFlexDirection toYoga(LayoutDirection direction) {
            switch (direction) {
            case LayoutDirection::horizontal:
                return YGFlexDirectionRow;
            case LayoutDirection::horizontalReverse:
                return YGFlexDirectionRowReverse;
            case LayoutDirection::vertical:
                return YGFlexDirectionColumn;
            case LayoutDirection::verticalReverse:
                return YGFlexDirectionColumnReverse;
            }
            return YGFlexDirectionRow;
        }

        YGJustify toYoga(LayoutAlignment alignment) {
            switch (alignment) {
            case LayoutAlignment::start:
                return YGJustifyFlexStart;
            case LayoutAlignment::center:
                return YGJustifyCenter;
            case LayoutAlignment::end:
                return YGJustifyFlexEnd;
            }
            return YGJustifyFlexStart;
        }

        YGAlign toYogaAlign(LayoutAlignment alignment) {
            switch (alignment) {
            case LayoutAlignment::start:
                return YGAlignFlexStart;
            case LayoutAlignment::center:
                return YGAlignCenter;
            case LayoutAlignment::end:
                return YGAlignFlexEnd;
            }
            return YGAlignFlexStart;
        }

        YGAlign toYoga(LayoutSelfAlignment alignment) {
            switch (alignment) {
            case LayoutSelfAlignment::auto_:
                return YGAlignAuto;
            case LayoutSelfAlignment::start:
                return YGAlignFlexStart;
            case LayoutSelfAlignment::center:
                return YGAlignCenter;
            case LayoutSelfAlignment::end:
                return YGAlignFlexEnd;
            case LayoutSelfAlignment::stretch:
                return YGAlignStretch;
            }
            return YGAlignAuto;
        }
    } // namespace

    UINodeRegistry::UINodeRegistry() : m_ygConfig(YGConfigNew()) {
    }

    UINodeRegistry::~UINodeRegistry() {
        clear();
        if (m_ygConfig != nullptr) {
            YGConfigFree(m_ygConfig);
            m_ygConfig = nullptr;
        }
    }

    UINode *UINodeRegistry::addNode(const UINode &node) {
        if (auto *existingNode = getNode(node.getId()); existingNode == &node) {
            return existingNode;
        }

        removeNode(node.getId());

        auto [it, inserted] = m_nodes.emplace(node.getId(), node);
        (void)inserted;
        it->second.attachRegistry(this);
        return &it->second;
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
                YGNodeRemoveChild(parentNode->m_ygNode, node.m_ygNode);
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
        for (auto &[id, node] : m_nodes) {
            (void)id;
            node.attachRegistry(nullptr);
        }
        m_nodes.clear();
    }

    UINode::UINode() : UINode(UUID()) {
    }

    UINode::UINode(const UUID &id) : m_id(id) {
        createYogaNode();
        applyYogaStyle();
    }

    UINode::UINode(const UINode &other) {
        copyFrom(other);
    }

    UINode::UINode(UINode &&other) noexcept {
        moveFrom(other);
    }

    UINode::~UINode() {
        releaseYogaNode();
    }

    UINode &UINode::operator=(const UINode &other) {
        if (this != &other) {
            releaseYogaNode();
            copyFrom(other);
        }
        return *this;
    }

    UINode &UINode::operator=(UINode &&other) noexcept {
        if (this != &other) {
            releaseYogaNode();
            moveFrom(other);
        }
        return *this;
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
        applyPositionStyle();
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
        applyPositionStyle();
        setPosDirty();
    }

    Unit &UINode::getPosUnit() {
        setPosDirty();
        return m_posUnit;
    }

    void UINode::setWidth(float width) {
        setWidthDimension({DimensionMode::point, nonNegative(width)});
    }

    void UINode::setHeight(float height) {
        setHeightDimension({DimensionMode::point, nonNegative(height)});
    }

    void UINode::setWidthPercent(float width) {
        setWidthDimension({DimensionMode::percent, percentValue(width)});
    }

    void UINode::setHeightPercent(float height) {
        setHeightDimension({DimensionMode::percent, percentValue(height)});
    }

    void UINode::setWidthAuto() {
        setWidthDimension({DimensionMode::auto_, 0.f});
    }

    void UINode::setHeightAuto() {
        setHeightDimension({DimensionMode::auto_, 0.f});
    }

    void UINode::setWidthFitContent() {
        setWidthDimension({DimensionMode::fitContent, 0.f});
    }

    void UINode::setHeightFitContent() {
        setHeightDimension({DimensionMode::fitContent, 0.f});
    }

    void UINode::setWidthMaxContent() {
        setWidthDimension({DimensionMode::maxContent, 0.f});
    }

    void UINode::setHeightMaxContent() {
        setHeightDimension({DimensionMode::maxContent, 0.f});
    }

    void UINode::setWidthStretch() {
        setWidthDimension({DimensionMode::stretch, 0.f});
    }

    void UINode::setHeightStretch() {
        setHeightDimension({DimensionMode::stretch, 0.f});
    }

    const Core::Style::Padding &UINode::getPadding() const {
        return m_padding;
    }

    void UINode::setPadding(const Core::Style::Padding &padding) {
        if (m_padding == padding) {
            return;
        }
        m_padding = padding;
        YGNodeStyleSetPadding(m_ygNode, YGEdgeTop, padding.top);
        YGNodeStyleSetPadding(m_ygNode, YGEdgeRight, padding.right);
        YGNodeStyleSetPadding(m_ygNode, YGEdgeBottom, padding.bottom);
        YGNodeStyleSetPadding(m_ygNode, YGEdgeLeft, padding.left);
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
        YGNodeStyleSetMargin(m_ygNode, YGEdgeTop, margin.top);
        YGNodeStyleSetMargin(m_ygNode, YGEdgeRight, margin.right);
        YGNodeStyleSetMargin(m_ygNode, YGEdgeBottom, margin.bottom);
        YGNodeStyleSetMargin(m_ygNode, YGEdgeLeft, margin.left);
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
        applyMinSizeStyle();
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
        applyMaxSizeStyle();
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
        YGNodeStyleSetFlexDirection(m_ygNode, toYoga(direction));
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
        YGNodeStyleSetJustifyContent(m_ygNode, toYoga(alignment));
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
        YGNodeStyleSetAlignItems(m_ygNode, toYogaAlign(alignment));
        setPosDirty();
    }

    LayoutAlignment &UINode::getCrossAxisAlignment() {
        setPosDirty();
        return m_crossAxisAlignment;
    }

    const LayoutSelfAlignment &UINode::getAlignSelf() const {
        return m_alignSelf;
    }

    void UINode::setAlignSelf(const LayoutSelfAlignment &alignment) {
        if (m_alignSelf == alignment) {
            return;
        }
        m_alignSelf = alignment;
        YGNodeStyleSetAlignSelf(m_ygNode, toYoga(alignment));
        setPosDirty();
    }

    LayoutSelfAlignment &UINode::getAlignSelf() {
        setPosDirty();
        return m_alignSelf;
    }

    float UINode::getFlexGrow() const {
        return m_flexGrow;
    }

    void UINode::setFlexGrow(float grow) {
        grow = nonNegative(grow);
        if (m_flexGrow == grow) {
            return;
        }
        m_flexGrow = grow;
        YGNodeStyleSetFlexGrow(m_ygNode, grow);
        setSizeDirty();
    }

    float UINode::getFlexShrink() const {
        return m_flexShrink;
    }

    void UINode::setFlexShrink(float shrink) {
        shrink = nonNegative(shrink);
        if (m_flexShrink == shrink) {
            return;
        }
        m_flexShrink = shrink;
        YGNodeStyleSetFlexShrink(m_ygNode, shrink);
        setSizeDirty();
    }

    void UINode::setFlex(float grow, float shrink, float basis) {
        setFlexGrow(grow);
        setFlexShrink(shrink);
        setFlexBasis(basis);
    }

    void UINode::setFlexBasis(float basis, Unit unit) {
        basis = nonNegative(basis);
        m_flexBasisMode = unit == Unit::relative ? FlexBasisMode::percent
                                                 : FlexBasisMode::point;
        m_flexBasisUnit = unit;
        m_flexBasis = basis;
        applyFlexBasisStyle();
        setSizeDirty();
    }

    void UINode::setFlexBasisAuto() {
        m_flexBasisMode = FlexBasisMode::auto_;
        applyFlexBasisStyle();
        setSizeDirty();
    }

    void UINode::setFlexBasisFitContent() {
        m_flexBasisMode = FlexBasisMode::fitContent;
        applyFlexBasisStyle();
        setSizeDirty();
    }

    void UINode::setFlexBasisMaxContent() {
        m_flexBasisMode = FlexBasisMode::maxContent;
        applyFlexBasisStyle();
        setSizeDirty();
    }

    void UINode::setFlexBasisStretch() {
        m_flexBasisMode = FlexBasisMode::stretch;
        applyFlexBasisStyle();
        setSizeDirty();
    }

    const PosMode &UINode::getPosMode() const {
        return m_posMode;
    }

    void UINode::setPosMode(const PosMode &posMode) {
        if (m_posMode == posMode) {
            return;
        }
        m_posMode = posMode;
        YGNodeStyleSetPositionType(m_ygNode,
                                   posMode == PosMode::absolute
                                       ? YGPositionTypeAbsolute
                                       : YGPositionTypeRelative);
        applyPositionStyle();
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

        YGNodeRemoveAllChildren(m_ygNode);
        m_children = children;

        if (m_registry != nullptr) {
            uint32_t i = 0;
            for (const auto &childId : m_children) {
                if (auto *childNode = m_registry->getNode(childId)) {
                    childNode->m_parentId = m_id;
                    YGNodeInsertChild(m_ygNode, childNode->m_ygNode, i++);
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

        if (node->m_parentId != UUID::null && node->m_parentId != m_id) {
            UINode *previousParent = nullptr;
            if (node->m_registry != nullptr) {
                previousParent = node->m_registry->getNode(node->m_parentId);
            }
            if (previousParent == nullptr && m_registry != nullptr) {
                previousParent = m_registry->getNode(node->m_parentId);
            }

            if (previousParent != nullptr) {
                previousParent->m_children.erase(node->m_id);
                if (YGNodeGetOwner(node->m_ygNode) ==
                    previousParent->m_ygNode) {
                    YGNodeRemoveChild(previousParent->m_ygNode, node->m_ygNode);
                }
                previousParent->setSizeDirty();
            }
        }

        if (auto *owner = YGNodeGetOwner(node->m_ygNode);
            owner != nullptr && owner != m_ygNode) {
            YGNodeRemoveChild(owner, node->m_ygNode);
        }

        const auto previousSize = m_children.size();
        m_children.insert(node->m_id);

        const bool alreadyYogaChild =
            YGNodeGetOwner(node->m_ygNode) == m_ygNode;
        if (!alreadyYogaChild) {
            YGNodeInsertChild(
                m_ygNode, node->m_ygNode, YGNodeGetChildCount(m_ygNode));
        }

        if (m_children.size() != previousSize || !alreadyYogaChild) {
            node->m_parentId = m_id;
            node->setPosDirty();
            setSizeDirty();
        }
    }

    void UINode::removeChild(UINode *node) {
        BESS_ASSERT(node != nullptr, "Cannot remove a null UI node child.");
        if (node != nullptr && m_children.erase(node->m_id) > 0) {
            node->m_parentId = UUID::null;
            YGNodeRemoveChild(m_ygNode, node->m_ygNode);
            node->setPosDirty();
            setSizeDirty();
        }
    }

    void UINode::clearChildren() {
        if (m_children.empty()) {
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

        YGNodeRemoveAllChildren(m_ygNode);
        m_children.clear();
        setSizeDirty();
    }

    glm::vec2 UINode::measure(UINodeRegistry &registry, const UUID &parentId) {
        if (parentId == UUID::null) {
            YGNodeCalculateLayout(
                m_ygNode, YGUndefined, YGUndefined, YGDirectionLTR);
        }

        const UINode *parentNode = nullptr;
        if (parentId != UUID::null) {
            parentNode = registry.getNode(parentId);
        }

        HashSet<UUID> activeNodes;
        syncLayoutFromYoga(registry, parentNode, activeNodes);
        return m_cachedSize;
    }

    void UINode::layout(UINodeRegistry &registry, const UUID &parentId) {
        measure(registry, parentId);
    }

    glm::vec3 UINode::getDrawPos() const {
        return {m_cachedPos, m_cachedZVal};
    }

    YGNodeRef UINode::getYogaNode() {
        return m_ygNode;
    }

    YGNodeConstRef UINode::getYogaNode() const {
        return m_ygNode;
    }

    void UINode::attachRegistry(UINodeRegistry *registry) {
        m_registry = registry;
        if (m_ygNode != nullptr && registry != nullptr) {
            YGNodeSetConfig(m_ygNode, registry->getYogaConfig());
        }
        rebuildYogaChildren();
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

    void UINode::rebuildYogaChildren() {
        if (m_ygNode == nullptr) {
            return;
        }

        YGNodeRemoveAllChildren(m_ygNode);
        if (m_registry == nullptr) {
            return;
        }

        uint32_t index = 0;
        for (const auto &childId : m_children) {
            if (auto *childNode = m_registry->getNode(childId)) {
                childNode->m_parentId = m_id;
                YGNodeInsertChild(m_ygNode, childNode->m_ygNode, index++);
            }
        }
    }

    void UINode::syncLayoutFromYoga(UINodeRegistry &registry,
                                    const UINode *parentNode,
                                    HashSet<UUID> &activeNodes) {
        const bool tracksCycle = m_id != UUID::null;
        if (tracksCycle) {
            if (activeNodes.find(m_id) != activeNodes.end()) {
                BESS_ASSERT(false,
                            "Cycle detected while syncing UI node {} from "
                            "Yoga.",
                            static_cast<uint64_t>(m_id));
                return;
            }
            activeNodes.insert(m_id);
        }

        m_drawSize.x = nonNegative(YGNodeLayoutGetWidth(m_ygNode));
        m_drawSize.y = nonNegative(YGNodeLayoutGetHeight(m_ygNode));
        m_cachedSize = nonNegativeVec(m_drawSize + marginSize());

        glm::vec2 topLeft{0.f};
        float parentZVal = 0.f;
        if (parentNode != nullptr) {
            const auto parentPos = parentNode->getDrawPos();
            const auto parentSize = parentNode->getDrawSize();
            topLeft = glm::vec2(parentPos) - (parentSize * 0.5f);
            topLeft.x += YGNodeLayoutGetLeft(m_ygNode);
            topLeft.y += YGNodeLayoutGetTop(m_ygNode);
            parentZVal = parentPos.z;
        } else {
            topLeft = resolvePos(nullptr) - (m_drawSize * 0.5f);
        }

        m_cachedPos = topLeft + (m_drawSize * 0.5f);
        m_cachedZVal = parentZVal + m_zVal;

        for (const auto &childId : m_children) {
            UINode *childNode = registry.getNode(childId);
            BESS_ASSERT(childNode,
                        "Child node {} not found in registry.",
                        static_cast<uint64_t>(childId));
            if (childNode == nullptr) {
                continue;
            }
            childNode->syncLayoutFromYoga(registry, this, activeNodes);
        }

        m_sizeDirty = false;
        m_posDirty = false;

        if (tracksCycle) {
            activeNodes.erase(m_id);
        }
    }

    void UINode::copyFrom(const UINode &other) {
        m_id = other.m_id;
        m_pos = other.m_pos;
        m_zVal = other.m_zVal;
        m_cachedZVal = other.m_cachedZVal;
        m_posUnit = other.m_posUnit;
        m_minSize = other.m_minSize;
        m_maxSize = other.m_maxSize;
        m_padding = other.m_padding;
        m_margin = other.m_margin;
        m_direction = other.m_direction;
        m_mainAxisAlignment = other.m_mainAxisAlignment;
        m_crossAxisAlignment = other.m_crossAxisAlignment;
        m_alignSelf = other.m_alignSelf;
        m_flexGrow = other.m_flexGrow;
        m_flexShrink = other.m_flexShrink;
        m_flexBasisMode = other.m_flexBasisMode;
        m_flexBasis = other.m_flexBasis;
        m_flexBasisUnit = other.m_flexBasisUnit;
        m_posMode = other.m_posMode;
        m_width = other.m_width;
        m_height = other.m_height;
        m_children = other.m_children;
        m_posDirty = other.m_posDirty;
        m_sizeDirty = other.m_sizeDirty;
        m_cachedPos = other.m_cachedPos;
        m_cachedSize = other.m_cachedSize;
        m_drawSize = other.m_drawSize;
        m_parentId = other.m_parentId;
        m_registry = nullptr;

        if (other.m_ygNode != nullptr) {
            m_ygNode = YGNodeClone(other.m_ygNode);
            YGNodeRemoveAllChildren(m_ygNode);
        } else {
            createYogaNode();
            applyYogaStyle();
        }
    }

    void UINode::moveFrom(UINode &other) noexcept {
        m_id = other.m_id;
        m_pos = other.m_pos;
        m_zVal = other.m_zVal;
        m_cachedZVal = other.m_cachedZVal;
        m_posUnit = other.m_posUnit;
        m_minSize = other.m_minSize;
        m_maxSize = other.m_maxSize;
        m_padding = other.m_padding;
        m_margin = other.m_margin;
        m_direction = other.m_direction;
        m_mainAxisAlignment = other.m_mainAxisAlignment;
        m_crossAxisAlignment = other.m_crossAxisAlignment;
        m_alignSelf = other.m_alignSelf;
        m_flexGrow = other.m_flexGrow;
        m_flexShrink = other.m_flexShrink;
        m_flexBasisMode = other.m_flexBasisMode;
        m_flexBasis = other.m_flexBasis;
        m_flexBasisUnit = other.m_flexBasisUnit;
        m_posMode = other.m_posMode;
        m_width = other.m_width;
        m_height = other.m_height;
        m_children = other.m_children;
        m_posDirty = other.m_posDirty;
        m_sizeDirty = other.m_sizeDirty;
        m_cachedPos = other.m_cachedPos;
        m_cachedSize = other.m_cachedSize;
        m_drawSize = other.m_drawSize;
        m_parentId = other.m_parentId;
        m_registry = other.m_registry;
        m_ygNode = other.m_ygNode;

        other.m_registry = nullptr;
        other.m_ygNode = nullptr;
    }

    void UINode::releaseYogaNode() {
        if (m_ygNode != nullptr) {
            YGNodeFree(m_ygNode);
            m_ygNode = nullptr;
        }
    }

    void UINode::createYogaNode(YGConfigRef config) {
        m_ygNode =
            config != nullptr ? YGNodeNewWithConfig(config) : YGNodeNew();
    }

    void UINode::applyYogaStyle() {
        YGNodeStyleSetFlexDirection(m_ygNode, toYoga(m_direction));
        YGNodeStyleSetJustifyContent(m_ygNode, toYoga(m_mainAxisAlignment));
        YGNodeStyleSetAlignItems(m_ygNode, toYogaAlign(m_crossAxisAlignment));
        YGNodeStyleSetAlignSelf(m_ygNode, toYoga(m_alignSelf));
        YGNodeStyleSetFlexGrow(m_ygNode, m_flexGrow);
        YGNodeStyleSetFlexShrink(m_ygNode, m_flexShrink);
        YGNodeStyleSetPositionType(m_ygNode,
                                   m_posMode == PosMode::absolute
                                       ? YGPositionTypeAbsolute
                                       : YGPositionTypeRelative);
        applyWidthStyle();
        applyHeightStyle();
        applyPositionStyle();
        applyMinSizeStyle();
        applyMaxSizeStyle();
        applyFlexBasisStyle();
        YGNodeStyleSetPadding(m_ygNode, YGEdgeTop, m_padding.top);
        YGNodeStyleSetPadding(m_ygNode, YGEdgeRight, m_padding.right);
        YGNodeStyleSetPadding(m_ygNode, YGEdgeBottom, m_padding.bottom);
        YGNodeStyleSetPadding(m_ygNode, YGEdgeLeft, m_padding.left);
        YGNodeStyleSetMargin(m_ygNode, YGEdgeTop, m_margin.top);
        YGNodeStyleSetMargin(m_ygNode, YGEdgeRight, m_margin.right);
        YGNodeStyleSetMargin(m_ygNode, YGEdgeBottom, m_margin.bottom);
        YGNodeStyleSetMargin(m_ygNode, YGEdgeLeft, m_margin.left);
    }

    void UINode::applyWidthStyle() {
        switch (m_width.mode) {
        case DimensionMode::auto_:
            YGNodeStyleSetWidthAuto(m_ygNode);
            break;
        case DimensionMode::point:
            YGNodeStyleSetWidth(m_ygNode, nonNegative(m_width.value));
            break;
        case DimensionMode::percent:
            YGNodeStyleSetWidthPercent(m_ygNode, percentValue(m_width.value));
            break;
        case DimensionMode::fitContent:
            YGNodeStyleSetWidthFitContent(m_ygNode);
            break;
        case DimensionMode::maxContent:
            YGNodeStyleSetWidthMaxContent(m_ygNode);
            break;
        case DimensionMode::stretch:
            YGNodeStyleSetWidthStretch(m_ygNode);
            break;
        }
    }

    void UINode::applyHeightStyle() {
        switch (m_height.mode) {
        case DimensionMode::auto_:
            YGNodeStyleSetHeightAuto(m_ygNode);
            break;
        case DimensionMode::point:
            YGNodeStyleSetHeight(m_ygNode, nonNegative(m_height.value));
            break;
        case DimensionMode::percent:
            YGNodeStyleSetHeightPercent(m_ygNode, percentValue(m_height.value));
            break;
        case DimensionMode::fitContent:
            YGNodeStyleSetHeightFitContent(m_ygNode);
            break;
        case DimensionMode::maxContent:
            YGNodeStyleSetHeightMaxContent(m_ygNode);
            break;
        case DimensionMode::stretch:
            YGNodeStyleSetHeightStretch(m_ygNode);
            break;
        }
    }

    void UINode::applyPositionStyle() {
        if (m_posUnit == Unit::pixel) {
            YGNodeStyleSetPosition(m_ygNode, YGEdgeTop, m_pos.y);
            YGNodeStyleSetPosition(m_ygNode, YGEdgeLeft, m_pos.x);
        } else {
            YGNodeStyleSetPositionPercent(
                m_ygNode, YGEdgeTop, percentValue(m_pos.y));
            YGNodeStyleSetPositionPercent(
                m_ygNode, YGEdgeLeft, percentValue(m_pos.x));
        }
    }

    void UINode::applyMinSizeStyle() {
        if (m_minSize.x >= 0.f) {
            YGNodeStyleSetMinWidth(m_ygNode, m_minSize.x);
        } else {
            YGNodeStyleSetMinWidth(m_ygNode, YGUndefined);
        }

        if (m_minSize.y >= 0.f) {
            YGNodeStyleSetMinHeight(m_ygNode, m_minSize.y);
        } else {
            YGNodeStyleSetMinHeight(m_ygNode, YGUndefined);
        }
    }

    void UINode::applyMaxSizeStyle() {
        if (m_maxSize.x >= 0.f) {
            YGNodeStyleSetMaxWidth(m_ygNode, m_maxSize.x);
        } else {
            YGNodeStyleSetMaxWidth(m_ygNode, YGUndefined);
        }

        if (m_maxSize.y >= 0.f) {
            YGNodeStyleSetMaxHeight(m_ygNode, m_maxSize.y);
        } else {
            YGNodeStyleSetMaxHeight(m_ygNode, YGUndefined);
        }
    }

    void UINode::applyFlexBasisStyle() {
        switch (m_flexBasisMode) {
        case FlexBasisMode::auto_:
            YGNodeStyleSetFlexBasisAuto(m_ygNode);
            break;
        case FlexBasisMode::point:
            YGNodeStyleSetFlexBasis(m_ygNode, m_flexBasis);
            break;
        case FlexBasisMode::percent:
            YGNodeStyleSetFlexBasisPercent(m_ygNode, percentValue(m_flexBasis));
            break;
        case FlexBasisMode::fitContent:
            YGNodeStyleSetFlexBasisFitContent(m_ygNode);
            break;
        case FlexBasisMode::maxContent:
            YGNodeStyleSetFlexBasisMaxContent(m_ygNode);
            break;
        case FlexBasisMode::stretch:
            YGNodeStyleSetFlexBasisStretch(m_ygNode);
            break;
        }
    }

    void UINode::setWidthDimension(const Dimension &dimension) {
        if (m_width.mode == dimension.mode &&
            m_width.value == dimension.value) {
            return;
        }
        m_width = dimension;
        applyWidthStyle();
        setSizeDirty();
    }

    void UINode::setHeightDimension(const Dimension &dimension) {
        if (m_height.mode == dimension.mode &&
            m_height.value == dimension.value) {
            return;
        }
        m_height = dimension;
        applyHeightStyle();
        setSizeDirty();
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
