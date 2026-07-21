#include "models/dock_model.h"

#include <algorithm>
#include <cmath>
#include <functional>

namespace Bess::UI {
    namespace {
        constexpr float minimumSplitRatio = 0.05f;
        constexpr float maximumSplitRatio = 0.95f;

        bool isSideZone(DockZone zone) noexcept {
            return zone == DockZone::left || zone == DockZone::right ||
                   zone == DockZone::top || zone == DockZone::bottom;
        }

        DockSplitAxis axisFor(DockZone zone) noexcept {
            return zone == DockZone::left || zone == DockZone::right
                       ? DockSplitAxis::horizontal
                       : DockSplitAxis::vertical;
        }

        bool insertedFirst(DockZone zone) noexcept {
            return zone == DockZone::left || zone == DockZone::top;
        }
    } // namespace

    DetachedDockItem::DetachedDockItem(DockItem item)
        : m_item(std::move(item)) {
    }

    DetachedDockItem::operator bool() const noexcept {
        return m_item.has_value();
    }

    const DockItem *DetachedDockItem::get() const noexcept {
        return m_item ? &*m_item : nullptr;
    }

    const DockStackLayout *
    DockLayoutResult::findStack(DockNodeId id) const noexcept {
        const auto it = std::find_if(
            stacks.begin(), stacks.end(), [id](const DockStackLayout &entry) {
                return entry.node == id;
            });
        return it != stacks.end() ? &*it : nullptr;
    }

    const DockSplitLayout *
    DockLayoutResult::findSplit(DockNodeId id) const noexcept {
        const auto it = std::find_if(
            splits.begin(), splits.end(), [id](const DockSplitLayout &entry) {
                return entry.node == id;
            });
        return it != splits.end() ? &*it : nullptr;
    }

    DockNodeId DockLayoutResult::stackAt(glm::vec2 position) const noexcept {
        for (auto it = stacks.rbegin(); it != stacks.rend(); ++it) {
            if (it->bounds.contains(position)) {
                return it->node;
            }
        }
        return {};
    }

    DockNodeId DockLayoutResult::dividerAt(glm::vec2 position) const noexcept {
        for (auto it = splits.rbegin(); it != splits.rend(); ++it) {
            if (it->dividerBounds.contains(position)) {
                return it->node;
            }
        }
        return {};
    }

    DockItemId DockSpaceModel::addItem(DockItem item,
                                       DockNodeId target,
                                       DockZone zone,
                                       size_t tabIndex) {
        if (!item.id) {
            item.id = DockItemId::generate();
        }
        if (m_itemLocations.contains(item.id)) {
            return {};
        }
        const DockItemId id = item.id;
        const DockNodeId targetBefore = target;
        const DockItemId inserted =
            dockItemInternal(std::move(item), target, zone, tabIndex);
        if (inserted) {
            m_changed.emit({.kind = DockModelChangeKind::itemAdded,
                            .item = inserted,
                            .target = targetBefore});
        }
        return inserted == id ? inserted : DockItemId{};
    }

    bool DockSpaceModel::attachItem(DetachedDockItem &&detached,
                                    DockNodeId target,
                                    DockZone zone,
                                    size_t tabIndex) {
        if (!detached.m_item || m_itemLocations.contains(detached.m_item->id) ||
            !canDock(target, zone)) {
            return false;
        }
        // Keep the transfer token intact until insertion succeeds. DockItem is
        // lightweight metadata; its content remains owned by WidgetTree.
        DockItem item = *detached.m_item;
        const auto id = item.id;
        const auto inserted =
            dockItemInternal(std::move(item), target, zone, tabIndex);
        if (!inserted) {
            return false;
        }
        detached.m_item.reset();
        m_changed.emit({.kind = DockModelChangeKind::itemAdded,
                        .item = id,
                        .target = target});
        return true;
    }

    bool DockSpaceModel::attachItemAtRoot(DetachedDockItem &&detached,
                                          DockZone zone) {
        const bool validZone =
            m_root ? isSideZone(zone) : zone == DockZone::main;
        if (!detached.m_item || m_itemLocations.contains(detached.m_item->id) ||
            !validZone) {
            return false;
        }

        DockItem item = *detached.m_item;
        const DockItemId id = item.id;
        const DockNodeId previousRoot = m_root;
        if (dockItemAtRootInternal(std::move(item), zone) != id) {
            return false;
        }
        detached.m_item.reset();
        m_changed.emit({.kind = DockModelChangeKind::itemAdded,
                        .item = id,
                        .target = previousRoot});
        return true;
    }

    bool DockSpaceModel::attachTree(DockSpaceModel &&source,
                                    DockNodeId target,
                                    DockZone zone) {
        return attachTreeInternal(source, target, zone, false);
    }

    bool DockSpaceModel::attachTreeAtRoot(DockSpaceModel &&source,
                                          DockZone zone) {
        return attachTreeInternal(source, m_root, zone, true);
    }

    DetachedDockItem DockSpaceModel::detachItem(DockItemId item) {
        DockNodeId source;
        auto detached = detachItemInternal(item, &source, true);
        if (!detached) {
            return {};
        }
        m_changed.emit({.kind = DockModelChangeKind::itemRemoved,
                        .item = item,
                        .source = source});
        return DetachedDockItem{std::move(*detached)};
    }

    bool DockSpaceModel::removeItem(DockItemId item) {
        auto detached = detachItem(item);
        return static_cast<bool>(detached);
    }

    bool DockSpaceModel::moveItem(DockItemId item,
                                  DockNodeId target,
                                  DockZone zone,
                                  size_t tabIndex) {
        const DockNodeId source = stackForItem(item);
        auto *sourceStack = const_cast<DockStackNode *>(getStack(source));
        if (sourceStack == nullptr || getStack(target) == nullptr ||
            (!isSideZone(zone) && zone != DockZone::main)) {
            return false;
        }

        if (source == target && zone == DockZone::main) {
            const size_t destination = tabIndex == DockTabModel::npos
                                           ? sourceStack->tabs.size() - 1
                                           : tabIndex;
            const bool moved = sourceStack->tabs.move(item, destination);
            if (moved) {
                sourceStack->tabs.activate(item);
                m_changed.emit({.kind = DockModelChangeKind::itemMoved,
                                .item = item,
                                .source = source,
                                .target = target});
            }
            return moved;
        }

        if (source == target && sourceStack->tabs.size() == 1) {
            return false;
        }

        DockNodeId detachedFrom;
        auto detached = detachItemInternal(item, &detachedFrom, true);
        if (!detached) {
            return false;
        }
        const DockItem recovery = *detached;
        const DockItemId inserted =
            dockItemInternal(std::move(*detached), target, zone, tabIndex);
        if (!inserted) {
            // All conditions were validated before detaching; this path is a
            // defensive recovery for future policies.
            const DockNodeId recoveryTarget =
                getStack(target) != nullptr ? target : firstStack();
            if (recoveryTarget) {
                dockItemInternal(DockItem{recovery},
                                 recoveryTarget,
                                 DockZone::main,
                                 DockTabModel::npos);
            } else {
                dockItemInternal(
                    DockItem{recovery}, {}, DockZone::main, DockTabModel::npos);
            }
            return false;
        }
        m_changed.emit({.kind = DockModelChangeKind::itemMoved,
                        .item = item,
                        .source = source,
                        .target = stackForItem(item)});
        return true;
    }

    bool DockSpaceModel::activateItem(DockItemId item) {
        const DockNodeId stackId = stackForItem(item);
        auto *stack = const_cast<DockStackNode *>(getStack(stackId));
        if (stack == nullptr) {
            return false;
        }
        if (stack->tabs.active() == item) {
            return true;
        }
        if (!stack->tabs.activate(item)) {
            return false;
        }
        m_changed.emit({.kind = DockModelChangeKind::itemActivated,
                        .item = item,
                        .target = stackId});
        return true;
    }

    bool DockSpaceModel::setItemTitle(DockItemId item, std::string title) {
        const DockNodeId stackId = stackForItem(item);
        auto *stack = const_cast<DockStackNode *>(getStack(stackId));
        if (stack == nullptr) {
            return false;
        }
        const auto *existing = stack->tabs.find(item);
        if (existing != nullptr && existing->title == title) {
            return true;
        }
        if (!stack->tabs.setTitle(item, std::move(title))) {
            return false;
        }
        m_changed.emit({.kind = DockModelChangeKind::itemUpdated,
                        .item = item,
                        .target = stackId});
        return true;
    }

    bool DockSpaceModel::setSplitRatio(DockNodeId splitId, float ratio) {
        auto *split = const_cast<DockSplitNode *>(getSplit(splitId));
        if (split == nullptr || !std::isfinite(ratio)) {
            return false;
        }
        const float clamped =
            std::clamp(ratio, minimumSplitRatio, maximumSplitRatio);
        if (split->ratio == clamped) {
            return true;
        }
        split->ratio = clamped;
        m_changed.emit(
            {.kind = DockModelChangeKind::splitResized, .target = splitId});
        return true;
    }

    DockNodeId DockSpaceModel::root() const noexcept {
        return m_root;
    }

    DockNodeId DockSpaceModel::firstStack() const noexcept {
        DockNodeId current = m_root;
        size_t remaining = m_nodes.size() + 1;
        while (current && remaining-- > 0) {
            if (getStack(current) != nullptr) {
                return current;
            }
            const auto *split = getSplit(current);
            if (split == nullptr) {
                return {};
            }
            current = split->first;
        }
        return {};
    }

    const DockStackNode *
    DockSpaceModel::getStack(DockNodeId id) const noexcept {
        const auto *node = getNode(id);
        return node != nullptr ? std::get_if<DockStackNode>(node) : nullptr;
    }

    const DockSplitNode *
    DockSpaceModel::getSplit(DockNodeId id) const noexcept {
        const auto *node = getNode(id);
        return node != nullptr ? std::get_if<DockSplitNode>(node) : nullptr;
    }

    DockNodeId DockSpaceModel::parentOf(DockNodeId id) const noexcept {
        if (const auto *stack = getStack(id)) {
            return stack->parent;
        }
        if (const auto *split = getSplit(id)) {
            return split->parent;
        }
        return {};
    }

    DockNodeId DockSpaceModel::stackForItem(DockItemId item) const noexcept {
        const auto it = m_itemLocations.find(item);
        return it != m_itemLocations.end() ? it->second : DockNodeId{};
    }

    const DockItem *DockSpaceModel::getItem(DockItemId item) const noexcept {
        const auto *stack = getStack(stackForItem(item));
        return stack != nullptr ? stack->tabs.find(item) : nullptr;
    }

    DockItemId DockSpaceModel::itemForContent(WidgetId content) const noexcept {
        if (!content) {
            return {};
        }
        for (const auto &[item, stackId] : m_itemLocations) {
            const auto *stack = getStack(stackId);
            const auto *entry =
                stack != nullptr ? stack->tabs.find(item) : nullptr;
            if (entry != nullptr && entry->content == content) {
                return item;
            }
        }
        return {};
    }

    size_t DockSpaceModel::nodeCount() const noexcept {
        return m_nodes.size();
    }

    size_t DockSpaceModel::itemCount() const noexcept {
        return m_itemLocations.size();
    }

    size_t DockSpaceModel::stackCount() const noexcept {
        size_t count = 0;
        for (const auto &[id, node] : m_nodes) {
            (void)id;
            count += std::holds_alternative<DockStackNode>(node) ? 1u : 0u;
        }
        return count;
    }

    std::vector<DockItemId> DockSpaceModel::itemIds() const {
        std::vector<DockItemId> result;
        result.reserve(m_itemLocations.size());
        if (!m_root) {
            return result;
        }

        std::function<void(DockNodeId)> append = [&](DockNodeId node) {
            if (const auto *stack = getStack(node)) {
                for (const auto &item : stack->tabs.items()) {
                    result.push_back(item.id);
                }
                return;
            }
            if (const auto *split = getSplit(node)) {
                append(split->first);
                append(split->second);
            }
        };
        append(m_root);
        return result;
    }

    bool DockSpaceModel::empty() const noexcept {
        return !m_root;
    }

    DockLayoutResult DockSpaceModel::layout(WidgetBounds bounds,
                                            float tabBarHeight,
                                            float splitterThickness) const {
        DockLayoutResult result;
        if (!m_root || bounds.empty()) {
            return result;
        }
        layoutNode(m_root,
                   bounds,
                   std::max(0.f, tabBarHeight),
                   std::max(0.f, splitterThickness),
                   result);
        return result;
    }

    bool DockSpaceModel::validate(std::string *reason) const {
        auto fail = [reason](std::string message) {
            if (reason != nullptr) {
                *reason = std::move(message);
            }
            return false;
        };

        if (!m_root) {
            return m_nodes.empty() && m_itemLocations.empty()
                       ? true
                       : fail("empty root has retained nodes or items");
        }
        if (getNode(m_root) == nullptr || parentOf(m_root)) {
            return fail("root is missing or has a parent");
        }

        HashSet<DockNodeId> visitedNodes;
        HashSet<DockItemId> visitedItems;
        std::function<bool(DockNodeId, DockNodeId)> visit =
            [&](DockNodeId id, DockNodeId expectedParent) {
                if (!id || !visitedNodes.insert(id).second) {
                    return fail("dock topology contains a cycle or duplicate");
                }
                if (parentOf(id) != expectedParent) {
                    return fail("dock node parent link is inconsistent");
                }
                if (const auto *stack = getStack(id)) {
                    if (stack->id != id || stack->tabs.empty() ||
                        !stack->tabs.validate()) {
                        return fail("dock stack is invalid or empty");
                    }
                    for (const auto &item : stack->tabs.items()) {
                        if (!item.id || !visitedItems.insert(item.id).second) {
                            return fail("dock item appears more than once");
                        }
                        const auto location = m_itemLocations.find(item.id);
                        if (location == m_itemLocations.end() ||
                            location->second != id) {
                            return fail("dock item location index is stale");
                        }
                    }
                    return true;
                }
                const auto *split = getSplit(id);
                if (split == nullptr || split->id != id || !split->first ||
                    !split->second || split->first == split->second ||
                    !std::isfinite(split->ratio) || split->ratio <= 0.f ||
                    split->ratio >= 1.f) {
                    return fail("dock split is invalid");
                }
                return visit(split->first, id) && visit(split->second, id);
            };

        if (!visit(m_root, {})) {
            return false;
        }
        if (visitedNodes.size() != m_nodes.size()) {
            return fail("dock model contains unreachable nodes");
        }
        if (visitedItems.size() != m_itemLocations.size()) {
            return fail("dock model contains unreachable items");
        }
        return true;
    }

    DockSpaceModel::ChangedSignal &DockSpaceModel::changed() noexcept {
        return m_changed;
    }

    DockNodeId DockSpaceModel::createStack(DockItem item) {
        DockNodeId id;
        do {
            id = DockNodeId::generate();
        } while (m_nodes.contains(id));
        DockStackNode stack{.id = id};
        const DockItemId itemId = stack.tabs.insert(std::move(item));
        if (!itemId) {
            return {};
        }
        m_nodes.emplace(id, std::move(stack));
        m_itemLocations.emplace(itemId, id);
        return id;
    }

    bool DockSpaceModel::canDock(DockNodeId target,
                                 DockZone zone) const noexcept {
        if (!m_root) {
            return !target && zone == DockZone::main;
        }
        return getStack(target) != nullptr &&
               (zone == DockZone::main || isSideZone(zone));
    }

    DockItemId DockSpaceModel::dockItemInternal(DockItem item,
                                                DockNodeId target,
                                                DockZone zone,
                                                size_t tabIndex) {
        if (!item.id || m_itemLocations.contains(item.id) ||
            !canDock(target, zone)) {
            return {};
        }
        const DockItemId itemId = item.id;
        if (!m_root) {
            m_root = createStack(std::move(item));
            return m_root ? itemId : DockItemId{};
        }

        auto *targetStack = const_cast<DockStackNode *>(getStack(target));
        if (targetStack == nullptr) {
            return {};
        }
        if (zone == DockZone::main) {
            const DockItemId inserted =
                targetStack->tabs.insert(std::move(item), tabIndex);
            if (inserted) {
                targetStack->tabs.activate(inserted);
                m_itemLocations.emplace(inserted, target);
            }
            return inserted;
        }
        const DockNodeId previousParent = targetStack->parent;
        const DockNodeId newStack = createStack(std::move(item));
        if (!newStack) {
            return {};
        }
        DockNodeId splitId;
        do {
            splitId = DockNodeId::generate();
        } while (m_nodes.contains(splitId));

        DockSplitNode split{
            .id = splitId,
            .parent = previousParent,
            .axis = axisFor(zone),
            .ratio = 0.5f,
            .first = insertedFirst(zone) ? newStack : target,
            .second = insertedFirst(zone) ? target : newStack,
        };
        m_nodes.emplace(splitId, split);
        targetStack = const_cast<DockStackNode *>(getStack(target));
        targetStack->parent = splitId;
        const_cast<DockStackNode *>(getStack(newStack))->parent = splitId;

        if (previousParent) {
            if (!replaceChild(previousParent, target, splitId)) {
                return {};
            }
        } else {
            m_root = splitId;
        }
        return itemId;
    }

    DockItemId DockSpaceModel::dockItemAtRootInternal(DockItem item,
                                                      DockZone zone) {
        if (!item.id || m_itemLocations.contains(item.id)) {
            return {};
        }
        if (!m_root) {
            return zone == DockZone::main ? dockItemInternal(std::move(item),
                                                             {},
                                                             DockZone::main,
                                                             DockTabModel::npos)
                                          : DockItemId{};
        }
        if (!isSideZone(zone)) {
            return {};
        }

        const DockItemId itemId = item.id;
        const DockNodeId previousRoot = m_root;
        const DockNodeId newStack = createStack(std::move(item));
        if (!newStack) {
            return {};
        }

        DockNodeId splitId;
        do {
            splitId = DockNodeId::generate();
        } while (m_nodes.contains(splitId));

        m_nodes.emplace(
            splitId,
            DockSplitNode{
                .id = splitId,
                .axis = axisFor(zone),
                .ratio = 0.5f,
                .first = insertedFirst(zone) ? newStack : previousRoot,
                .second = insertedFirst(zone) ? previousRoot : newStack,
            });
        if (auto *rootStack =
                const_cast<DockStackNode *>(getStack(previousRoot))) {
            rootStack->parent = splitId;
        } else if (auto *rootSplit =
                       const_cast<DockSplitNode *>(getSplit(previousRoot))) {
            rootSplit->parent = splitId;
        }
        const_cast<DockStackNode *>(getStack(newStack))->parent = splitId;
        m_root = splitId;
        return itemId;
    }

    bool DockSpaceModel::attachTreeInternal(DockSpaceModel &source,
                                            DockNodeId target,
                                            DockZone zone,
                                            bool atRoot) {
        if (&source == this || source.empty() || !source.validate() ||
            !validate()) {
            return false;
        }
        if (empty()) {
            if (target || zone != DockZone::main) {
                return false;
            }
        } else {
            if (!isSideZone(zone)) {
                return false;
            }
            if (atRoot) {
                target = m_root;
            } else if (getStack(target) == nullptr) {
                return false;
            }
        }

        for (const auto &[node, value] : source.m_nodes) {
            (void)value;
            if (m_nodes.contains(node)) {
                return false;
            }
        }
        for (const auto &[item, node] : source.m_itemLocations) {
            (void)node;
            if (m_itemLocations.contains(item)) {
                return false;
            }
        }

        const DockNodeId sourceRoot = source.m_root;
        const DockNodeId previousRoot = m_root;
        if (empty()) {
            m_nodes.reserve(source.m_nodes.size());
            m_itemLocations.reserve(source.m_itemLocations.size());
            for (auto &[id, node] : source.m_nodes) {
                m_nodes.emplace(id, std::move(node));
            }
            for (const auto &[item, node] : source.m_itemLocations) {
                m_itemLocations.emplace(item, node);
            }
            m_root = sourceRoot;
            source.m_nodes.clear();
            source.m_itemLocations.clear();
            source.m_root = {};
            m_changed.emit({.kind = DockModelChangeKind::topologyAttached,
                            .target = m_root});
            source.m_changed.emit(
                {.kind = DockModelChangeKind::topologyDetached,
                 .source = sourceRoot});
            return true;
        }

        const DockNodeId previousParent =
            atRoot ? DockNodeId{} : parentOf(target);
        if (previousParent) {
            const auto *parent = getSplit(previousParent);
            if (parent == nullptr ||
                (parent->first != target && parent->second != target)) {
                return false;
            }
        } else if (target != m_root) {
            return false;
        }

        DockNodeId splitId;
        do {
            splitId = DockNodeId::generate();
        } while (m_nodes.contains(splitId) || source.m_nodes.contains(splitId));
        m_nodes.reserve(m_nodes.size() + source.m_nodes.size() + 1);
        m_itemLocations.reserve(m_itemLocations.size() +
                                source.m_itemLocations.size());
        for (auto &[id, node] : source.m_nodes) {
            m_nodes.emplace(id, std::move(node));
        }
        for (const auto &[item, node] : source.m_itemLocations) {
            m_itemLocations.emplace(item, node);
        }
        m_nodes.emplace(splitId,
                        DockSplitNode{
                            .id = splitId,
                            .parent = previousParent,
                            .axis = axisFor(zone),
                            .ratio = 0.5f,
                            .first = insertedFirst(zone) ? sourceRoot : target,
                            .second = insertedFirst(zone) ? target : sourceRoot,
                        });

        if (auto *sourceStack =
                const_cast<DockStackNode *>(getStack(sourceRoot))) {
            sourceStack->parent = splitId;
        } else {
            const_cast<DockSplitNode *>(getSplit(sourceRoot))->parent = splitId;
        }
        if (auto *targetStack = const_cast<DockStackNode *>(getStack(target))) {
            targetStack->parent = splitId;
        } else {
            const_cast<DockSplitNode *>(getSplit(target))->parent = splitId;
        }
        if (previousParent) {
            static_cast<void>(replaceChild(previousParent, target, splitId));
        } else {
            m_root = splitId;
        }

        source.m_nodes.clear();
        source.m_itemLocations.clear();
        source.m_root = {};
        m_changed.emit({.kind = DockModelChangeKind::topologyAttached,
                        .source = previousRoot,
                        .target = splitId});
        source.m_changed.emit({.kind = DockModelChangeKind::topologyDetached,
                               .source = sourceRoot});
        return true;
    }

    std::optional<DockItem> DockSpaceModel::detachItemInternal(
        DockItemId item, DockNodeId *source, bool collapse) {
        const DockNodeId stackId = stackForItem(item);
        auto *stack = const_cast<DockStackNode *>(getStack(stackId));
        if (stack == nullptr) {
            return std::nullopt;
        }
        if (source != nullptr) {
            *source = stackId;
        }
        auto detached = stack->tabs.detach(item);
        auto value = std::move(detached).take();
        if (!value) {
            return std::nullopt;
        }
        m_itemLocations.erase(item);
        if (collapse && stack->tabs.empty()) {
            collapseEmptyStack(stackId);
        }
        return value;
    }

    void DockSpaceModel::collapseEmptyStack(DockNodeId stackId) {
        const auto *stack = getStack(stackId);
        if (stack == nullptr || !stack->tabs.empty()) {
            return;
        }
        const DockNodeId parentId = stack->parent;
        if (!parentId) {
            m_nodes.erase(stackId);
            m_root = {};
            return;
        }

        const auto *parent = getSplit(parentId);
        if (parent == nullptr) {
            return;
        }
        const DockNodeId sibling =
            parent->first == stackId ? parent->second : parent->first;
        const DockNodeId grandParent = parent->parent;

        if (grandParent) {
            replaceChild(grandParent, parentId, sibling);
        } else {
            m_root = sibling;
        }
        if (auto *siblingStack =
                const_cast<DockStackNode *>(getStack(sibling))) {
            siblingStack->parent = grandParent;
        } else if (auto *siblingSplit =
                       const_cast<DockSplitNode *>(getSplit(sibling))) {
            siblingSplit->parent = grandParent;
        }
        m_nodes.erase(stackId);
        m_nodes.erase(parentId);
    }

    bool DockSpaceModel::replaceChild(DockNodeId parentId,
                                      DockNodeId oldChild,
                                      DockNodeId newChild) {
        auto *parent = const_cast<DockSplitNode *>(getSplit(parentId));
        if (parent == nullptr || !newChild) {
            return false;
        }
        if (parent->first == oldChild) {
            parent->first = newChild;
            return true;
        }
        if (parent->second == oldChild) {
            parent->second = newChild;
            return true;
        }
        return false;
    }

    void DockSpaceModel::layoutNode(DockNodeId nodeId,
                                    WidgetBounds bounds,
                                    float tabBarHeight,
                                    float splitterThickness,
                                    DockLayoutResult &result) const {
        if (const auto *stack = getStack(nodeId)) {
            const float tabHeight = std::min(tabBarHeight, bounds.size.y);
            const auto topLeft = bounds.topLeft();
            const WidgetBounds tabs{
                .center = {bounds.center.x, topLeft.y + tabHeight * 0.5f},
                .size = {bounds.size.x, tabHeight},
            };
            const float contentHeight =
                std::max(0.f, bounds.size.y - tabHeight);
            const WidgetBounds content{
                .center = {bounds.center.x,
                           topLeft.y + tabHeight + contentHeight * 0.5f},
                .size = {bounds.size.x, contentHeight},
            };
            result.stacks.push_back({.node = nodeId,
                                     .bounds = bounds,
                                     .tabBarBounds = tabs,
                                     .contentBounds = content,
                                     .activeItem = stack->tabs.active()});
            return;
        }

        const auto *split = getSplit(nodeId);
        if (split == nullptr) {
            return;
        }
        const float axisSize = split->axis == DockSplitAxis::horizontal
                                   ? bounds.size.x
                                   : bounds.size.y;
        const float divider = std::min(splitterThickness, axisSize);
        const float available = std::max(0.f, axisSize - divider);
        const float firstSize = available * split->ratio;
        const float secondSize = available - firstSize;
        const auto topLeft = bounds.topLeft();

        WidgetBounds first;
        WidgetBounds second;
        WidgetBounds dividerBounds;
        if (split->axis == DockSplitAxis::horizontal) {
            first = {.center = {topLeft.x + firstSize * 0.5f, bounds.center.y},
                     .size = {firstSize, bounds.size.y}};
            dividerBounds = {
                .center = {topLeft.x + firstSize + divider * 0.5f,
                           bounds.center.y},
                .size = {divider, bounds.size.y},
            };
            second = {
                .center = {topLeft.x + firstSize + divider + secondSize * 0.5f,
                           bounds.center.y},
                .size = {secondSize, bounds.size.y},
            };
        } else {
            first = {.center = {bounds.center.x, topLeft.y + firstSize * 0.5f},
                     .size = {bounds.size.x, firstSize}};
            dividerBounds = {
                .center = {bounds.center.x,
                           topLeft.y + firstSize + divider * 0.5f},
                .size = {bounds.size.x, divider},
            };
            second = {
                .center = {bounds.center.x,
                           topLeft.y + firstSize + divider + secondSize * 0.5f},
                .size = {bounds.size.x, secondSize},
            };
        }

        result.splits.push_back(
            {.node = nodeId, .bounds = bounds, .dividerBounds = dividerBounds});
        layoutNode(
            split->first, first, tabBarHeight, splitterThickness, result);
        layoutNode(
            split->second, second, tabBarHeight, splitterThickness, result);
    }

    DockSpaceModel::Node *DockSpaceModel::getNode(DockNodeId id) noexcept {
        const auto it = m_nodes.find(id);
        return it != m_nodes.end() ? &it->second : nullptr;
    }

    const DockSpaceModel::Node *
    DockSpaceModel::getNode(DockNodeId id) const noexcept {
        const auto it = m_nodes.find(id);
        return it != m_nodes.end() ? &it->second : nullptr;
    }
} // namespace Bess::UI
