#include "widget_tree.h"

#include "ui_painter.h"

#include <algorithm>
#include <cmath>
#include <iterator>
#include <limits>
#include <stdexcept>

namespace Bess::UI {
    namespace {
        bool finiteVec(glm::vec2 value) noexcept {
            return std::isfinite(value.x) && std::isfinite(value.y);
        }

        WidgetInvalidation withoutFlag(WidgetInvalidation value,
                                       WidgetInvalidation flag) noexcept {
            return static_cast<WidgetInvalidation>(static_cast<uint8_t>(value) &
                                                   ~static_cast<uint8_t>(flag));
        }
    } // namespace

    WidgetTree::WidgetTree()
        : m_control(std::make_shared<Detail::WidgetTreeControl>(
              Detail::WidgetTreeControl{.tree = this})) {
    }

    WidgetTree::~WidgetTree() {
        // Expire external handles before invoking any widget unmount hooks.
        // Those hooks may own WidgetRefs back into this tree.
        m_control->tree = nullptr;
        clear();
        m_control.reset();
    }

    WidgetId WidgetTree::addWidget(std::unique_ptr<Widget> widget,
                                   WidgetId parent,
                                   size_t index) {
        if (widget == nullptr || m_destroying ||
            (parent && !contains(parent))) {
            return {};
        }

        WidgetId id;
        do {
            id = WidgetId::generate();
        } while (!id || contains(id));

        auto *layout = m_layoutRegistry.addNode(id.value());
        if (layout == nullptr) {
            return {};
        }

        Node node;
        node.widget = std::move(widget);
        node.parent = parent;
        node.properties.hitTestVisible = node.widget->traits().hitTestVisible;
        try {
            node.runtimeId = allocateRuntimeId();
        } catch (...) {
            m_layoutRegistry.removeNode(id.value());
            throw;
        }
        const uint32_t runtimeId = node.runtimeId;

        auto [it, inserted] = m_nodes.emplace(id, std::move(node));
        if (!inserted) {
            releaseRuntimeId(runtimeId);
            m_layoutRegistry.removeNode(id.value());
            return {};
        }
        m_runtimeToWidget.emplace(it->second.runtimeId, id);

        auto attach = [index](std::vector<WidgetId> &siblings, WidgetId child) {
            const size_t insertAt = index == append
                                        ? siblings.size()
                                        : std::min(index, siblings.size());
            siblings.insert(siblings.begin() + static_cast<ptrdiff_t>(insertAt),
                            child);
        };

        if (parent) {
            attach(m_nodes.find(parent)->second.children, id);
            syncLayoutChildren(parent);
        } else {
            attach(m_roots, id);
        }

        beginCallback();
        WidgetMountContext context{.state = *this, .id = id, .layout = *layout};
        try {
            it->second.widget->onMount(context);
        } catch (...) {
            endCallback();
            if (contains(id)) {
                removeWidget(id);
            }
            throw;
        }
        endCallback();

        if (!contains(id)) {
            return {};
        }

        m_invalidation |=
            WidgetInvalidation::layout | WidgetInvalidation::paint;
        return id;
    }

    bool WidgetTree::removeWidget(WidgetId id) {
        if (!contains(id)) {
            return false;
        }

        if (m_callbackDepth > 0) {
            if (std::find(m_pendingRemovals.begin(),
                          m_pendingRemovals.end(),
                          id) == m_pendingRemovals.end()) {
                m_pendingRemovals.push_back(id);
            }
            return true;
        }
        return removeWidgetNow(id);
    }

    void WidgetTree::clear() {
        if (m_destroying) {
            return;
        }
        if (m_callbackDepth > 0) {
            m_clearPending = true;
            return;
        }

        m_destroying = true;
        while (!m_roots.empty()) {
            removeWidgetNow(m_roots.back());
        }
        // Defensive cleanup for an inconsistent tree. Public operations never
        // create orphans, but destruction should still be total.
        while (!m_nodes.empty()) {
            removeWidgetNow(m_nodes.begin()->first);
        }
        m_pendingRemovals.clear();
        m_runtimeToWidget.clear();
        m_layoutRegistry.clear();
        m_focused = {};
        m_pointerCapture = {};
        m_hovered = {};
        m_lastPointerPosition.reset();
        m_clearPending = false;
        m_destroying = false;
    }

    bool
    WidgetTree::reparentWidget(WidgetId id, WidgetId newParent, size_t index) {
        auto *node = findNode(id);
        if (node == nullptr || (newParent && !contains(newParent)) ||
            id == newParent || (newParent && isDescendantOf(newParent, id))) {
            return false;
        }

        const WidgetId oldParent = node->parent;
        auto eraseFrom = [id](std::vector<WidgetId> &siblings) {
            const auto it = std::find(siblings.begin(), siblings.end(), id);
            if (it != siblings.end()) {
                siblings.erase(it);
            }
        };
        auto insertInto = [id, index](std::vector<WidgetId> &siblings) {
            const size_t insertAt = index == append
                                        ? siblings.size()
                                        : std::min(index, siblings.size());
            siblings.insert(siblings.begin() + static_cast<ptrdiff_t>(insertAt),
                            id);
        };

        if (oldParent) {
            eraseFrom(m_nodes.find(oldParent)->second.children);
        } else {
            eraseFrom(m_roots);
        }

        node->parent = newParent;
        if (newParent) {
            insertInto(m_nodes.find(newParent)->second.children);
        } else {
            insertInto(m_roots);
        }

        if (oldParent) {
            syncLayoutChildren(oldParent);
        }
        if (newParent) {
            syncLayoutChildren(newParent);
        } else if (auto *layout = getLayout(id)) {
            // setChildren on the old parent detached the Yoga owner. This is
            // intentionally empty; roots are measured independently.
            layout->setPosDirty();
        }

        m_invalidation |=
            WidgetInvalidation::layout | WidgetInvalidation::paint;
        return true;
    }

    bool WidgetTree::contains(WidgetId id) const noexcept {
        return id && m_nodes.find(id) != m_nodes.end();
    }

    Widget *WidgetTree::getWidget(WidgetId id) noexcept {
        auto *node = findNode(id);
        return node != nullptr ? node->widget.get() : nullptr;
    }

    const Widget *WidgetTree::getWidget(WidgetId id) const noexcept {
        const auto *node = findNode(id);
        return node != nullptr ? node->widget.get() : nullptr;
    }

    LayoutNode *WidgetTree::getLayout(WidgetId id) noexcept {
        return id ? m_layoutRegistry.getNode(id.value()) : nullptr;
    }

    const LayoutNode *WidgetTree::getLayout(WidgetId id) const noexcept {
        return id ? m_layoutRegistry.getNode(id.value()) : nullptr;
    }

    LayoutNodeRegistry &WidgetTree::layoutRegistry() noexcept {
        return m_layoutRegistry;
    }

    const LayoutNodeRegistry &WidgetTree::layoutRegistry() const noexcept {
        return m_layoutRegistry;
    }

    WidgetId WidgetTree::getParent(WidgetId id) const noexcept {
        const auto *node = findNode(id);
        return node != nullptr ? node->parent : WidgetId{};
    }

    std::span<const WidgetId>
    WidgetTree::getChildren(WidgetId id) const noexcept {
        const auto *node = findNode(id);
        return node != nullptr ? std::span<const WidgetId>{node->children}
                               : std::span<const WidgetId>{};
    }

    std::span<const WidgetId> WidgetTree::getRoots() const noexcept {
        return m_roots;
    }

    bool WidgetTree::setVisibility(WidgetId id, WidgetVisibility visibility) {
        auto *node = findNode(id);
        if (node == nullptr || node->properties.visibility == visibility) {
            return node != nullptr;
        }

        node->properties.visibility = visibility;
        if (auto *layout = getLayout(id)) {
            YGNodeStyleSetDisplay(layout->getYogaNode(),
                                  visibility == WidgetVisibility::collapsed
                                      ? YGDisplayNone
                                      : YGDisplayFlex);
            layout->setSizeDirty();
        }

        if (visibility != WidgetVisibility::visible) {
            if (m_focused && isDescendantOf(m_focused, id)) {
                clearFocus();
            }
            if (m_pointerCapture && isDescendantOf(m_pointerCapture, id)) {
                releasePointer();
            }
            if (m_hovered && isDescendantOf(m_hovered, id)) {
                updateHover({});
            }
        }
        m_invalidation |=
            WidgetInvalidation::layout | WidgetInvalidation::paint;
        return true;
    }

    WidgetVisibility WidgetTree::getVisibility(WidgetId id) const noexcept {
        const auto *node = findNode(id);
        return node != nullptr ? node->properties.visibility
                               : WidgetVisibility::collapsed;
    }

    bool WidgetTree::setEnabled(WidgetId id, bool enabled) {
        auto *node = findNode(id);
        if (node == nullptr || node->properties.enabled == enabled) {
            return node != nullptr;
        }
        node->properties.enabled = enabled;
        if (!enabled) {
            if (m_focused && isDescendantOf(m_focused, id)) {
                clearFocus();
            }
            if (m_pointerCapture && isDescendantOf(m_pointerCapture, id)) {
                releasePointer();
            }
        }
        m_invalidation |= WidgetInvalidation::paint;
        return true;
    }

    bool WidgetTree::isEnabled(WidgetId id) const noexcept {
        const auto *node = findNode(id);
        return node != nullptr && node->properties.enabled;
    }

    bool WidgetTree::setHitTestVisible(WidgetId id, bool visible) {
        auto *node = findNode(id);
        if (node == nullptr) {
            return false;
        }
        node->properties.hitTestVisible = visible;
        if (!visible && m_pointerCapture == id) {
            releasePointer(id);
        }
        return true;
    }

    void WidgetTree::setViewportSize(glm::vec2 size) {
        if (!finiteVec(size)) {
            size = {0.f, 0.f};
        }
        size = glm::max(size, glm::vec2{0.f});
        if (m_viewportSize == size) {
            return;
        }
        m_viewportSize = size;
        m_invalidation |=
            WidgetInvalidation::layout | WidgetInvalidation::paint;
    }

    glm::vec2 WidgetTree::getViewportSize() const noexcept {
        return m_viewportSize;
    }

    UITheme &WidgetTree::theme() noexcept {
        return m_theme;
    }

    const UITheme &WidgetTree::theme() const noexcept {
        return m_theme;
    }

    void WidgetTree::setTheme(UITheme theme) {
        m_theme = std::move(theme);
        ++m_themeRevision;
        m_invalidation |=
            WidgetInvalidation::layout | WidgetInvalidation::paint;
    }

    void WidgetTree::performLayout() {
        // Clear before callbacks so a layout invalidation raised by arrange()
        // remains pending for the next pass.
        m_invalidation =
            withoutFlag(m_invalidation, WidgetInvalidation::layout);

        const uint64_t themeRevision = m_themeRevision;
        const bool themeChanged = m_layoutThemeRevision != themeRevision;
        const auto layoutRoots = m_roots;
        beginCallback();
        try {
            for (const auto root : layoutRoots) {
                updateLayoutSubtree(root, themeChanged);
            }
        } catch (...) {
            endCallback();
            throw;
        }
        endCallback();
        m_layoutThemeRevision = themeRevision;

        for (auto &[id, node] : m_nodes) {
            (void)id;
            node.arrangedBounds.reset();
            node.arrangedVisible = true;
        }

        const auto roots = m_roots;
        for (const auto root : roots) {
            if (!contains(root)) {
                continue;
            }
            auto *layout = getLayout(root);
            if (layout == nullptr) {
                continue;
            }
            layout->setPos({0.f, 0.f});
            layout->setPosUnit(Unit::pixel);
            layout->setDrawPivot(DrawPivot::center);
            layout->setWidth(m_viewportSize.x);
            layout->setHeight(m_viewportSize.y);
            layout->measure(m_layoutRegistry, UUID::null);
            arrangeSubtree(root);
        }
    }

    WidgetBounds WidgetTree::getBounds(WidgetId id) const noexcept {
        const auto *node = findNode(id);
        if (node == nullptr) {
            return {};
        }
        if (node->arrangedBounds.has_value()) {
            return *node->arrangedBounds;
        }
        const auto *layout = getLayout(id);
        if (layout == nullptr) {
            return {};
        }
        return {
            .center = layout->getCachedPos(),
            .size = glm::max(layout->getDrawSize(), glm::vec2{0.f}),
        };
    }

    void WidgetTree::update(TimeMs deltaTime) {
        const auto roots = m_roots;
        beginCallback();
        for (const auto root : roots) {
            updateSubtree(root, deltaTime);
        }
        endCallback();
    }

    void WidgetTree::paint(UIPainter &painter) {
        if (hasInvalidation(m_invalidation, WidgetInvalidation::layout)) {
            performLayout();
        }

        // As with layout, clear before callbacks to retain re-entrant paint
        // invalidations instead of accidentally consuming them.
        m_invalidation = withoutFlag(m_invalidation, WidgetInvalidation::paint);
        const auto roots = m_roots;
        beginCallback();
        try {
            for (const auto root : roots) {
                paintSubtree(root, painter);
            }
        } catch (...) {
            endCallback();
            throw;
        }
        endCallback();
    }

    UIDispatchResult WidgetTree::dispatchEvent(const UIEvent &event) {
        if (const auto *resize = event.getIf<UITargetResizeEvent>()) {
            setViewportSize({static_cast<float>(resize->width),
                             static_cast<float>(resize->height)});
            UIDispatchResult result;
            const auto roots = m_roots;
            for (const auto root : roots) {
                const auto current =
                    dispatchToTarget(root, event, std::nullopt);
                result.handled = result.handled || current.handled;
                if (!result.target) {
                    result.target = current.target;
                }
            }
            return result;
        }

        const auto pointer = pointerPosition(event);
        WidgetId pointed;
        if (pointer.has_value()) {
            m_lastPointerPosition = *pointer;
            pointed = hitTest(*pointer);
            updateHover(pointed);
        }

        WidgetId target;
        const bool isMouseMove = event.is<Input::MouseMoveEvent>();
        const bool isMouseButton = event.is<Input::MouseButtonEvent>();
        if (isMouseMove || isMouseButton) {
            target = contains(m_pointerCapture) ? m_pointerCapture : pointed;
        } else if (event.is<Input::MouseWheelEvent>()) {
            target = pointed;
        } else if (event.is<Input::KeyEvent>() ||
                   event.is<Input::TextInputEvent>()) {
            target = contains(m_focused) ? m_focused : WidgetId{};
        }

        if (!target) {
            if (const auto *button = event.getIf<Input::MouseButtonEvent>();
                button != nullptr && button->button == MouseButton::left &&
                button->action == MouseButtonAction::press) {
                clearFocus();
            }
            return {};
        }

        auto result = dispatchToTarget(target, event, pointer);
        if (const auto *button = event.getIf<Input::MouseButtonEvent>();
            button != nullptr && button->action == MouseButtonAction::release &&
            m_pointerCapture) {
            releasePointer();
        }
        return result;
    }

    WidgetId WidgetTree::hitTest(glm::vec2 uiPosition) const {
        HitTestResult frontmost;
        // Walk in paint order. Equal-Z candidates encountered later replace
        // earlier candidates, while an explicitly higher accumulated Z wins
        // across parent/sibling subtree boundaries.
        for (const auto root : m_roots) {
            const auto candidate = hitTestSubtree(root, uiPosition, true, 0.f);
            if (candidate.id &&
                (!frontmost.id || candidate.zIndex >= frontmost.zIndex)) {
                frontmost = candidate;
            }
        }
        return frontmost.id;
    }

    WidgetId WidgetTree::getFocusedWidget() const noexcept {
        return m_focused;
    }

    bool WidgetTree::setFocus(WidgetId id) {
        if (id) {
            const auto *node = findNode(id);
            if (node == nullptr || !node->widget->traits().focusable ||
                !isEffectivelyEnabled(id) || !isEffectivelyVisible(id)) {
                return false;
            }
        }

        if (m_changingFocus) {
            m_deferredFocus = id;
            return true;
        }
        if (m_focused == id) {
            return true;
        }

        m_changingFocus = true;
        WidgetId requested = id;
        size_t remainingChanges = m_nodes.size() + 8;
        do {
            m_deferredFocus.reset();
            const WidgetId previous = m_focused;
            m_focused = {};
            if (previous && contains(previous)) {
                dispatchDirect(previous, UIFocusChangedEvent{.focused = false});
            }

            if (requested && contains(requested) &&
                isEffectivelyEnabled(requested) &&
                isEffectivelyVisible(requested) &&
                getWidget(requested)->traits().focusable) {
                m_focused = requested;
                dispatchDirect(requested, UIFocusChangedEvent{.focused = true});
            }
            if (m_deferredFocus.has_value()) {
                requested = *m_deferredFocus;
            }
        } while (m_deferredFocus.has_value() && requested != m_focused &&
                 remainingChanges-- > 0);
        m_deferredFocus.reset();
        m_changingFocus = false;
        m_invalidation |= WidgetInvalidation::paint;
        return m_focused == id || (!id && !m_focused);
    }

    void WidgetTree::clearFocus() {
        setFocus({});
    }

    WidgetId WidgetTree::getPointerCapture() const noexcept {
        return m_pointerCapture;
    }

    bool WidgetTree::capturePointer(WidgetId id) {
        if (!contains(id) || !isEffectivelyEnabled(id) ||
            !isEffectivelyVisible(id)) {
            return false;
        }
        m_pointerCapture = id;
        return true;
    }

    void WidgetTree::releasePointer(WidgetId id) {
        if (!id || m_pointerCapture == id) {
            m_pointerCapture = {};
        }
    }

    WidgetId WidgetTree::getHoveredWidget() const noexcept {
        return m_hovered;
    }

    CursorIcon WidgetTree::getCursorShape() const noexcept {
        if (!m_lastPointerPosition.has_value()) {
            return CursorIcon::arrow;
        }

        WidgetId current = contains(m_pointerCapture) ? m_pointerCapture
                                                      : m_hovered;
        size_t remaining = m_nodes.size() + 1;
        while (current && remaining-- > 0) {
            const auto *node = findNode(current);
            if (node == nullptr) {
                break;
            }
            const CursorIcon requested = node->widget->cursor({
                .state = *this,
                .id = current,
                .bounds = getBounds(current),
                .pointerPosition = *m_lastPointerPosition,
                .captured = current == m_pointerCapture,
            });
            if (requested != CursorIcon::inherit) {
                return requested;
            }
            current = node->parent;
        }
        return CursorIcon::arrow;
    }

    PickingId WidgetTree::getPickingId(WidgetId id,
                                       uint32_t info) const noexcept {
        const auto *node = findNode(id);
        return node != nullptr
                   ? PickingId{.runtimeId = node->runtimeId, .info = info}
                   : PickingId::invalid();
    }

    WidgetId WidgetTree::resolvePickingId(PickingId pickingId) const noexcept {
        if (!pickingId.isValid()) {
            return {};
        }
        const auto it = m_runtimeToWidget.find(pickingId.runtimeId);
        return it != m_runtimeToWidget.end() ? it->second : WidgetId{};
    }

    void WidgetTree::invalidate(WidgetId id, WidgetInvalidation invalidation) {
        if (!contains(id) || invalidation == WidgetInvalidation::none) {
            return;
        }
        m_invalidation |= invalidation;
        if (hasInvalidation(invalidation, WidgetInvalidation::layout)) {
            if (auto *layout = getLayout(id)) {
                layout->setSizeDirty();
            }
        }
    }

    WidgetInvalidation WidgetTree::pendingInvalidation() const noexcept {
        return m_invalidation;
    }

    WidgetInvalidation WidgetTree::consumeInvalidation() noexcept {
        const auto result = m_invalidation;
        m_invalidation = WidgetInvalidation::none;
        return result;
    }

    bool WidgetTree::setArrangedBounds(WidgetId owner,
                                       WidgetId child,
                                       WidgetBounds bounds) {
        auto *node = findNode(child);
        if (node == nullptr || node->parent != owner ||
            !finiteVec(bounds.center) || !finiteVec(bounds.size)) {
            return false;
        }
        bounds.size = glm::max(bounds.size, glm::vec2{0.f});
        node->arrangedBounds = bounds;
        if (auto *layout = getLayout(child)) {
            layout->measureWithin(m_layoutRegistry, bounds.center, bounds.size);
        }
        return true;
    }

    bool WidgetTree::setArrangedVisible(WidgetId owner,
                                        WidgetId child,
                                        bool visible) {
        auto *node = findNode(child);
        if (node == nullptr || node->parent != owner) {
            return false;
        }
        node->arrangedVisible = visible;
        return true;
    }

    WidgetTree::Node *WidgetTree::findNode(WidgetId id) noexcept {
        const auto it = m_nodes.find(id);
        return it != m_nodes.end() ? &it->second : nullptr;
    }

    const WidgetTree::Node *WidgetTree::findNode(WidgetId id) const noexcept {
        const auto it = m_nodes.find(id);
        return it != m_nodes.end() ? &it->second : nullptr;
    }

    bool WidgetTree::isDescendantOf(WidgetId id,
                                    WidgetId ancestor) const noexcept {
        WidgetId current = id;
        size_t remaining = m_nodes.size() + 1;
        while (current && remaining-- > 0) {
            if (current == ancestor) {
                return true;
            }
            current = getParent(current);
        }
        return false;
    }

    bool WidgetTree::isEffectivelyEnabled(WidgetId id) const noexcept {
        WidgetId current = id;
        size_t remaining = m_nodes.size() + 1;
        while (current && remaining-- > 0) {
            const auto *node = findNode(current);
            if (node == nullptr || !node->properties.enabled) {
                return false;
            }
            current = node->parent;
        }
        return !current;
    }

    bool WidgetTree::isEffectivelyVisible(WidgetId id) const noexcept {
        WidgetId current = id;
        size_t remaining = m_nodes.size() + 1;
        while (current && remaining-- > 0) {
            const auto *node = findNode(current);
            if (node == nullptr ||
                node->properties.visibility != WidgetVisibility::visible ||
                !node->arrangedVisible) {
                return false;
            }
            current = node->parent;
        }
        return !current;
    }

    void WidgetTree::syncLayoutChildren(WidgetId parent) {
        auto *parentNode = findNode(parent);
        auto *parentLayout = getLayout(parent);
        if (parentNode == nullptr || parentLayout == nullptr) {
            return;
        }
        OrderedSet<UUID> children;
        for (const auto child : parentNode->children) {
            if (contains(child)) {
                children.insert(child.value());
            }
        }
        parentLayout->setChildren(children);
    }

    bool WidgetTree::removeWidgetNow(WidgetId id) {
        auto *rootNode = findNode(id);
        if (rootNode == nullptr) {
            return false;
        }

        std::vector<WidgetId> subtree;
        collectSubtree(id, subtree);

        // Hold deferred destruction across focus/hover notifications as those
        // callbacks are allowed to request removal of this same subtree.
        beginCallback();

        if (m_focused && isDescendantOf(m_focused, id)) {
            clearFocus();
        }
        if (m_pointerCapture && isDescendantOf(m_pointerCapture, id)) {
            releasePointer();
        }
        if (m_hovered && isDescendantOf(m_hovered, id)) {
            updateHover({});
        }

        const WidgetId parent = rootNode->parent;
        auto eraseId = [id](std::vector<WidgetId> &ids) {
            ids.erase(std::remove(ids.begin(), ids.end(), id), ids.end());
        };
        if (parent) {
            eraseId(m_nodes.find(parent)->second.children);
            syncLayoutChildren(parent);
        } else {
            eraseId(m_roots);
        }

        for (auto it = subtree.rbegin(); it != subtree.rend(); ++it) {
            if (auto *node = findNode(*it); node != nullptr) {
                node->widget->onUnmount(*this, *it);
            }
        }

        for (auto it = subtree.rbegin(); it != subtree.rend(); ++it) {
            auto nodeIt = m_nodes.find(*it);
            if (nodeIt == m_nodes.end()) {
                continue;
            }
            releaseRuntimeId(nodeIt->second.runtimeId);
            m_layoutRegistry.removeNode(it->value());
            m_nodes.erase(nodeIt);
        }
        m_invalidation |=
            WidgetInvalidation::layout | WidgetInvalidation::paint;
        endCallback();
        return true;
    }

    void WidgetTree::collectSubtree(WidgetId id,
                                    std::vector<WidgetId> &ids) const {
        const auto *node = findNode(id);
        if (node == nullptr) {
            return;
        }
        ids.push_back(id);
        const auto children = node->children;
        for (const auto child : children) {
            collectSubtree(child, ids);
        }
    }

    void WidgetTree::flushPendingRemovals() {
        if (m_callbackDepth != 0) {
            return;
        }
        if (m_clearPending) {
            m_clearPending = false;
            clear();
            return;
        }
        if (m_pendingRemovals.empty()) {
            return;
        }
        auto pending = std::move(m_pendingRemovals);
        m_pendingRemovals.clear();
        for (const auto id : pending) {
            if (contains(id)) {
                removeWidgetNow(id);
            }
        }
    }

    void WidgetTree::arrangeSubtree(WidgetId id) {
        auto *node = findNode(id);
        if (node == nullptr ||
            node->properties.visibility == WidgetVisibility::collapsed) {
            return;
        }
        const auto children = node->children;
        beginCallback();
        WidgetArrangeContext context{
            .state = *this,
            .id = id,
            .bounds = getBounds(id),
        };
        node->widget->arrange(context);
        endCallback();
        for (const auto child : children) {
            arrangeSubtree(child);
        }
    }

    void WidgetTree::updateLayoutSubtree(WidgetId id, bool themeChanged) {
        auto *node = findNode(id);
        if (node == nullptr) {
            return;
        }
        const auto children = node->children;
        auto *layout = getLayout(id);
        if (layout != nullptr) {
            WidgetLayoutContext context{
                .state = *this,
                .id = id,
                .layout = *layout,
                .themeChanged = themeChanged,
            };
            node->widget->updateLayout(context);
        }
        for (const auto child : children) {
            updateLayoutSubtree(child, themeChanged);
        }
    }

    void WidgetTree::updateSubtree(WidgetId id, TimeMs deltaTime) {
        auto *node = findNode(id);
        if (node == nullptr || !isEffectivelyVisible(id)) {
            return;
        }
        const auto children = node->children;
        WidgetUpdateContext context{
            .state = *this,
            .id = id,
            .deltaTime = deltaTime,
        };
        node->widget->update(context);
        for (const auto child : children) {
            updateSubtree(child, deltaTime);
        }
    }

    void WidgetTree::paintSubtree(WidgetId id, UIPainter &painter) {
        auto *node = findNode(id);
        if (node == nullptr || !isEffectivelyVisible(id)) {
            return;
        }
        const auto bounds = getBounds(id);
        const auto children = node->children;
        WidgetPaintContext context{
            .state = *this,
            .painter = painter,
            .id = id,
            .bounds = bounds,
            .pickingId = getPickingId(id),
            .enabled = isEffectivelyEnabled(id),
            .hovered = m_hovered == id,
            .focused = m_focused == id,
        };
        const auto *layout = getLayout(id);
        painter.pushLayer(layout != nullptr ? layout->getZVal() : 0.f);
        bool layerPushed = true;
        const bool clip = node->widget->traits().clipChildren;
        bool clipPushed = false;
        try {
            node->widget->paint(context);
            if (clip) {
                painter.pushClip(bounds);
                clipPushed = true;
            }
            for (const auto child : children) {
                paintSubtree(child, painter);
            }
            node = findNode(id);
            if (node != nullptr && isEffectivelyVisible(id)) {
                node->widget->paintOverlay(context);
            }
            if (clipPushed) {
                clipPushed = false;
                painter.popClip();
            }
            layerPushed = false;
            painter.popLayer();
        } catch (...) {
            if (clipPushed) {
                painter.popClip();
            }
            if (layerPushed) {
                painter.popLayer();
            }
            throw;
        }
    }

    WidgetTree::HitTestResult
    WidgetTree::hitTestSubtree(WidgetId id,
                               glm::vec2 position,
                               bool ancestorsEnabled,
                               float ancestorZ) const {
        const auto *node = findNode(id);
        if (node == nullptr ||
            node->properties.visibility != WidgetVisibility::visible ||
            !node->arrangedVisible) {
            return {};
        }
        const bool enabled = ancestorsEnabled && node->properties.enabled;
        if (!enabled) {
            return {};
        }

        const auto *layout = getLayout(id);
        const float localZ =
            layout != nullptr && std::isfinite(layout->getZVal())
                ? layout->getZVal()
                : 0.f;
        const float summedZ = ancestorZ + localZ;
        const float zIndex = std::isfinite(summedZ) ? summedZ : ancestorZ;
        const auto bounds = getBounds(id);
        HitTestResult frontmost;
        if (node->widget->hitTest(bounds, position) &&
            node->properties.hitTestVisible &&
            node->widget->traits().hitTestVisible) {
            frontmost = {.id = id, .zIndex = zIndex};
        }

        const bool insideLayout = bounds.contains(position);
        if (insideLayout || !node->widget->traits().clipChildren) {
            for (const auto child : node->children) {
                const auto candidate =
                    hitTestSubtree(child, position, enabled, zIndex);
                if (candidate.id &&
                    (!frontmost.id || candidate.zIndex >= frontmost.zIndex)) {
                    frontmost = candidate;
                }
            }
        }
        return frontmost;
    }

    std::optional<glm::vec2>
    WidgetTree::pointerPosition(const UIEvent &event) const noexcept {
        glm::vec2 position;
        if (const auto *move = event.getIf<Input::MouseMoveEvent>()) {
            position = move->pos;
        } else if (const auto *wheel = event.getIf<Input::MouseWheelEvent>()) {
            position = wheel->pos;
        } else if (const auto *button =
                       event.getIf<Input::MouseButtonEvent>()) {
            position = button->pos;
        } else {
            return std::nullopt;
        }
        return position - m_viewportSize * 0.5f;
    }

    void WidgetTree::updateHover(WidgetId hovered) {
        if (hovered == m_hovered) {
            return;
        }
        const WidgetId previous = m_hovered;
        m_hovered = {};
        if (previous && contains(previous)) {
            dispatchDirect(previous, UIPointerCrossingEvent{.entered = false});
        }
        if (hovered && contains(hovered)) {
            m_hovered = hovered;
            dispatchDirect(hovered, UIPointerCrossingEvent{.entered = true});
        }
        m_invalidation |= WidgetInvalidation::paint;
    }

    UIDispatchResult
    WidgetTree::dispatchToTarget(WidgetId target,
                                 const UIEvent &event,
                                 std::optional<glm::vec2> pointer) {
        if (!contains(target)) {
            return {};
        }

        std::vector<WidgetId> route;
        WidgetId current = target;
        size_t remaining = m_nodes.size() + 1;
        while (current && remaining-- > 0) {
            route.push_back(current);
            current = getParent(current);
        }
        if (current) {
            return {};
        }

        UIDispatchResult result{.target = target};
        beginCallback();
        bool stopped = false;
        for (auto it = route.rbegin(); it != route.rend() && !stopped; ++it) {
            if (*it == target) {
                continue;
            }
            const auto reply =
                invokeEvent(*it, target, UIEventPhase::capture, event, pointer);
            result.handled = result.handled || reply.handled;
            stopped = reply.stopPropagation;
        }
        if (!stopped && contains(target)) {
            const auto reply = invokeEvent(
                target, target, UIEventPhase::target, event, pointer);
            result.handled = result.handled || reply.handled;
            stopped = reply.stopPropagation;
        }
        for (size_t i = 1; i < route.size() && !stopped; ++i) {
            if (!contains(route[i])) {
                continue;
            }
            const auto reply = invokeEvent(
                route[i], target, UIEventPhase::bubble, event, pointer);
            result.handled = result.handled || reply.handled;
            stopped = reply.stopPropagation;
        }
        endCallback();
        return result;
    }

    UIEventReply WidgetTree::invokeEvent(WidgetId current,
                                         WidgetId target,
                                         UIEventPhase phase,
                                         const UIEvent &event,
                                         std::optional<glm::vec2> pointer) {
        auto *node = findNode(current);
        if (node == nullptr) {
            return {};
        }
        WidgetEventContext context{
            .state = *this,
            .id = current,
            .target = target,
            .phase = phase,
            .bounds = getBounds(current),
            .pointerPosition = pointer.value_or(glm::vec2{0.f}),
            .hasPointerPosition = pointer.has_value(),
            .enabled = isEffectivelyEnabled(current),
            .hovered = m_hovered == current,
            .focused = m_focused == current,
        };
        const auto reply = node->widget->onEvent(context, event);
        applyReply(current, reply);
        return reply;
    }

    void WidgetTree::applyReply(WidgetId current, const UIEventReply &reply) {
        if (!contains(current)) {
            return;
        }
        if (reply.invalidate != WidgetInvalidation::none) {
            invalidate(current, reply.invalidate);
        }
        if (reply.clearFocus && m_focused == current) {
            clearFocus();
        } else if (reply.requestFocus) {
            setFocus(current);
        }
        if (reply.releasePointer) {
            releasePointer(current);
        } else if (reply.capturePointer) {
            capturePointer(current);
        }
    }

    void WidgetTree::dispatchDirect(WidgetId target, const UIEvent &event) {
        if (!contains(target)) {
            return;
        }
        beginCallback();
        invokeEvent(
            target, target, UIEventPhase::target, event, m_lastPointerPosition);
        endCallback();
    }

    void WidgetTree::beginCallback() noexcept {
        ++m_callbackDepth;
    }

    void WidgetTree::endCallback() {
        if (m_callbackDepth == 0) {
            return;
        }
        --m_callbackDepth;
        if (m_callbackDepth == 0) {
            flushPendingRemovals();
        }
    }

    uint32_t WidgetTree::allocateRuntimeId() {
        if (m_nextRuntimeId == PickingId::invalidRuntimeId) {
            throw std::runtime_error(
                "WidgetTree exhausted picking runtime IDs");
        }
        return m_nextRuntimeId++;
    }

    void WidgetTree::releaseRuntimeId(uint32_t runtimeId) {
        if (runtimeId == PickingId::invalidRuntimeId) {
            return;
        }
        m_runtimeToWidget.erase(runtimeId);
    }
} // namespace Bess::UI
