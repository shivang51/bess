#include "widget_tree.h"

#include "popup.h"
#include "ui_painter.h"

#include <algorithm>
#include <cmath>
#include <exception>
#include <iterator>
#include <limits>
#include <stdexcept>

namespace Bess::UI {
    namespace {
        bool finiteVec(glm::vec2 value) noexcept {
            return std::isfinite(value.x) && std::isfinite(value.y);
        }

        WidgetBounds intersectBounds(WidgetBounds lhs,
                                     WidgetBounds rhs) noexcept {
            if (!finiteVec(rhs.center) || !finiteVec(rhs.size)) {
                return {.center = lhs.center, .size = {0.f, 0.f}};
            }
            const glm::vec2 topLeft = glm::max(lhs.topLeft(), rhs.topLeft());
            const glm::vec2 bottomRight =
                glm::min(lhs.bottomRight(), rhs.bottomRight());
            const glm::vec2 size =
                glm::max(bottomRight - topLeft, glm::vec2{0.f});
            return {.center = topLeft + size * 0.5f, .size = size};
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
        try {
            clear();
        } catch (...) {
            // Destructors must not propagate application callback failures.
            // clear() still completes structural teardown before rethrowing.
        }
        m_control.reset();
    }

    WidgetId WidgetTree::addWidget(std::unique_ptr<Widget> widget,
                                   WidgetId parent,
                                   size_t index) {
        if (widget == nullptr || m_destroying ||
            (parent && (!contains(parent) || m_removing.contains(parent)))) {
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
            return insertAt;
        };

        if (parent) {
            auto &siblings = m_nodes.find(parent)->second.children;
            const size_t insertAt = attach(siblings, id);
            // Declarative composition overwhelmingly appends. Updating Yoga
            // incrementally keeps a row/column with N children O(N) to build;
            // indexed insertion retains the authoritative full-order sync.
            if (insertAt + 1 == siblings.size()) {
                if (auto *parentLayout = getLayout(parent)) {
                    parentLayout->addChild(layout);
                } else {
                    syncLayoutChildren(parent);
                }
            } else {
                syncLayoutChildren(parent);
            }
        } else {
            static_cast<void>(attach(m_roots, id));
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
        if (m_removing.contains(id)) {
            return true;
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
        std::exception_ptr failure;
        const auto removeAndRemember = [this, &failure](WidgetId id) {
            try {
                static_cast<void>(removeWidgetNow(id));
            } catch (...) {
                if (failure == nullptr) {
                    failure = std::current_exception();
                }
            }
        };
        while (!m_roots.empty()) {
            removeAndRemember(m_roots.back());
        }
        // Defensive cleanup for an inconsistent tree. Public operations never
        // create orphans, but destruction should still be total.
        while (!m_nodes.empty()) {
            removeAndRemember(m_nodes.begin()->first);
        }
        m_pendingRemovals.clear();
        m_removing.clear();
        m_runtimeToWidget.clear();
        m_layoutRegistry.clear();
        m_focused = {};
        m_pointerCapture = {};
        m_hovered = {};
        m_focusScopes.clear();
        m_lastPointerPosition.reset();
        m_clearPending = false;
        m_destroying = false;
        if (failure != nullptr) {
            std::rethrow_exception(failure);
        }
    }

    bool
    WidgetTree::reparentWidget(WidgetId id, WidgetId newParent, size_t index) {
        auto *node = findNode(id);
        if (node == nullptr || (newParent && !contains(newParent)) ||
            m_removing.contains(id) ||
            (newParent && m_removing.contains(newParent)) || id == newParent ||
            (newParent && isDescendantOf(newParent, id))) {
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
        reconcileInteractionState();
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

        m_invalidation |=
            WidgetInvalidation::layout | WidgetInvalidation::paint;
        reconcileInteractionState();
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
        m_invalidation |= WidgetInvalidation::paint;
        reconcileInteractionState();
        return true;
    }

    bool WidgetTree::isEnabled(WidgetId id) const noexcept {
        const auto *node = findNode(id);
        return node != nullptr && node->properties.enabled;
    }

    bool WidgetTree::setHitTestVisible(WidgetId id, bool visible) {
        auto *node = findNode(id);
        if (node == nullptr || node->properties.hitTestVisible == visible) {
            return node != nullptr;
        }
        node->properties.hitTestVisible = visible;
        if (!visible && m_pointerCapture == id) {
            releasePointer(id);
        }
        reconcileInteractionState();
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

    const UITheme &WidgetTree::theme() const noexcept {
        return m_theme;
    }

    void WidgetTree::setTheme(UITheme theme) {
        m_theme = std::move(theme);
        ++m_themeRevision;
        m_invalidation |=
            WidgetInvalidation::layout | WidgetInvalidation::paint;
    }

    void WidgetTree::setPlatformServices(
        std::shared_ptr<UIPlatformServices> services) {
        m_platformServices = services != nullptr ? std::move(services)
                                                 : nullUIPlatformServices();
    }

    UIPlatformServices &WidgetTree::platformServices() noexcept {
        return *m_platformServices;
    }

    UIPlatformServices &WidgetTree::platformServices() const noexcept {
        return *m_platformServices;
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
            m_invalidation |=
                WidgetInvalidation::layout | WidgetInvalidation::paint;
            throw;
        }
        endCallback();
        m_layoutThemeRevision = themeRevision;

        try {
            for (auto &[id, node] : m_nodes) {
                (void)id;
                node.arrangedBounds.reset();
                node.arrangedVisible = true;
                node.arrangedZOffset = 0.f;
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
            resolvePendingAutoFocus();
            reconcileInteractionState();
        } catch (...) {
            // A later pass must recompute the whole arranged state; retaining
            // partially arranged geometry after an application callback
            // throws would make painting and hit testing disagree.
            m_invalidation |=
                WidgetInvalidation::layout | WidgetInvalidation::paint;
            throw;
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
        try {
            for (const auto root : roots) {
                updateSubtree(root, deltaTime);
            }
        } catch (...) {
            endCallback();
            m_invalidation |= WidgetInvalidation::paint;
            throw;
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
            m_invalidation |= WidgetInvalidation::paint;
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

        if (const auto *key = event.getIf<Input::KeyEvent>();
            key != nullptr && key->key == KeyCode::escape &&
            key->action == KeyAction::press && m_popupHost != nullptr &&
            m_popupHost->dismissTopmostOnEscape()) {
            return {.target = getFocusedWidget(), .handled = true};
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
                   event.is<Input::TextInputEvent>() ||
                   event.is<Input::TextCompositionEvent>()) {
            target = contains(m_focused) ? m_focused : WidgetId{};
        }

        if (!target) {
            if (const auto *key = event.getIf<Input::KeyEvent>();
                key != nullptr && key->key == KeyCode::tab &&
                (key->action == KeyAction::press ||
                 key->action == KeyAction::hold)) {
                return {.target = getFocusedWidget(),
                        .handled =
                            moveFocus(event.modifiers.shift
                                          ? FocusTraversalDirection::backward
                                          : FocusTraversalDirection::forward)};
            }
            if (const auto *button = event.getIf<Input::MouseButtonEvent>();
                button != nullptr && button->button == MouseButton::left &&
                button->action == MouseButtonAction::press) {
                clearFocus();
            }
            return {};
        }

        UIDispatchResult result;
        try {
            result = dispatchToTarget(target, event, pointer);
        } catch (...) {
            if (const auto *button = event.getIf<Input::MouseButtonEvent>();
                button != nullptr &&
                button->action == MouseButtonAction::release &&
                m_pointerCapture) {
                releasePointer();
            }
            throw;
        }
        if (!result.handled) {
            if (const auto *key = event.getIf<Input::KeyEvent>();
                key != nullptr && key->key == KeyCode::tab &&
                (key->action == KeyAction::press ||
                 key->action == KeyAction::hold)) {
                result.handled = moveFocus(
                    event.modifiers.shift ? FocusTraversalDirection::backward
                                          : FocusTraversalDirection::forward);
                result.target = getFocusedWidget();
            }
        }
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
            if (!isFocusable(id) || !focusAllowedByActiveScope(id)) {
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
        try {
            WidgetId requested = id;
            size_t remainingChanges = m_nodes.size() + 8;
            do {
                m_deferredFocus.reset();
                const WidgetId previous = m_focused;
                m_focused = {};
                if (previous && contains(previous)) {
                    dispatchDirect(previous,
                                   UIFocusChangedEvent{.focused = false});
                }

                if (requested && contains(requested) &&
                    isEffectivelyEnabled(requested) &&
                    isEffectivelyVisible(requested) &&
                    getWidget(requested)->traits().focusable &&
                    focusAllowedByActiveScope(requested)) {
                    m_focused = requested;
                    dispatchDirect(requested,
                                   UIFocusChangedEvent{.focused = true});
                }
                if (m_deferredFocus.has_value()) {
                    requested = *m_deferredFocus;
                }
            } while (m_deferredFocus.has_value() && requested != m_focused &&
                     remainingChanges-- > 0);
        } catch (...) {
            m_deferredFocus.reset();
            m_changingFocus = false;
            m_invalidation |= WidgetInvalidation::paint;
            throw;
        }
        m_deferredFocus.reset();
        m_changingFocus = false;
        m_invalidation |= WidgetInvalidation::paint;
        return m_focused == id || (!id && !m_focused);
    }

    void WidgetTree::clearFocus() {
        setFocus({});
    }

    bool WidgetTree::moveFocus(FocusTraversalDirection direction) {
        WidgetId traversalRoot;
        for (auto it = m_focusScopes.rbegin(); it != m_focusScopes.rend();
             ++it) {
            if (it->policy.trapFocus && contains(it->scope) &&
                isEffectivelyVisible(it->scope) &&
                isEffectivelyEnabled(it->scope)) {
                traversalRoot = it->scope;
                break;
            }
        }

        std::vector<WidgetId> candidates;
        if (traversalRoot) {
            collectFocusable(traversalRoot, candidates);
        } else {
            for (const auto root : m_roots) {
                collectFocusable(root, candidates);
            }
        }
        if (candidates.empty()) {
            return false;
        }

        const auto current =
            std::find(candidates.begin(), candidates.end(), m_focused);
        size_t index = 0;
        if (direction == FocusTraversalDirection::forward) {
            index =
                current == candidates.end()
                    ? 0
                    : (static_cast<size_t>(current - candidates.begin()) + 1) %
                          candidates.size();
        } else {
            index = current == candidates.end() || current == candidates.begin()
                        ? candidates.size() - 1
                        : static_cast<size_t>(current - candidates.begin() - 1);
        }
        return setFocus(candidates[index]);
    }

    bool WidgetTree::activateFocusScope(WidgetId scope,
                                        FocusScopePolicy policy) {
        if (!contains(scope)) {
            return false;
        }
        const auto existing = std::find_if(
            m_focusScopes.begin(),
            m_focusScopes.end(),
            [scope](const auto &entry) { return entry.scope == scope; });
        if (existing != m_focusScopes.end()) {
            existing->policy = policy;
            existing->pendingAutoFocus = policy.autoFocus;
            if (policy.trapFocus && m_focused &&
                !isDescendantOf(m_focused, scope)) {
                clearFocus();
            }
            return true;
        }
        m_focusScopes.push_back({.scope = scope,
                                 .previousFocus = m_focused,
                                 .policy = policy,
                                 .pendingAutoFocus = policy.autoFocus});
        if (policy.trapFocus && m_focused &&
            !isDescendantOf(m_focused, scope)) {
            clearFocus();
        }
        if (policy.autoFocus) {
            m_invalidation |= WidgetInvalidation::paint;
        }
        return true;
    }

    bool WidgetTree::deactivateFocusScope(WidgetId scope) {
        const auto it = std::find_if(
            m_focusScopes.begin(),
            m_focusScopes.end(),
            [scope](const auto &entry) { return entry.scope == scope; });
        if (it == m_focusScopes.end()) {
            return false;
        }

        const WidgetId restore = it->previousFocus;
        const bool shouldRestore = it->policy.restoreFocus;
        const bool focusWasOwned =
            m_focused && contains(scope) && isDescendantOf(m_focused, scope);
        const bool focusNeedsRestore = focusWasOwned || !m_focused;
        m_focusScopes.erase(it);
        if (focusNeedsRestore) {
            if (shouldRestore && restore && isFocusable(restore) &&
                focusAllowedByActiveScope(restore)) {
                static_cast<void>(setFocus(restore));
            } else {
                clearFocus();
            }
        }
        return true;
    }

    bool WidgetTree::setDefaultFocus(WidgetId scope, WidgetId widget) {
        const auto it = std::find_if(
            m_focusScopes.begin(),
            m_focusScopes.end(),
            [scope](const auto &entry) { return entry.scope == scope; });
        if (it == m_focusScopes.end() ||
            (widget && (!contains(widget) || !isDescendantOf(widget, scope)))) {
            return false;
        }
        it->defaultFocus = widget;
        if (it->policy.autoFocus) {
            it->pendingAutoFocus = true;
            resolvePendingAutoFocus();
        }
        return true;
    }

    bool WidgetTree::focusDefault(WidgetId scope) {
        const auto it = std::find_if(
            m_focusScopes.begin(),
            m_focusScopes.end(),
            [scope](const auto &entry) { return entry.scope == scope; });
        if (it == m_focusScopes.end() || !contains(scope)) {
            return false;
        }
        if (isFocusable(it->defaultFocus) &&
            isDescendantOf(it->defaultFocus, scope)) {
            return setFocus(it->defaultFocus);
        }
        std::vector<WidgetId> candidates;
        collectFocusable(scope, candidates);
        return !candidates.empty() && setFocus(candidates.front());
    }

    WidgetId WidgetTree::activeFocusScope() const noexcept {
        for (auto it = m_focusScopes.rbegin(); it != m_focusScopes.rend();
             ++it) {
            if (contains(it->scope) && isEffectivelyVisible(it->scope) &&
                isEffectivelyEnabled(it->scope)) {
                return it->scope;
            }
        }
        return {};
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

    std::optional<glm::vec2> WidgetTree::getPointerPosition() const noexcept {
        return m_lastPointerPosition;
    }

    CursorIcon WidgetTree::getCursorShape() const noexcept {
        if (!m_lastPointerPosition.has_value()) {
            return CursorIcon::arrow;
        }

        WidgetId current =
            contains(m_pointerCapture) ? m_pointerCapture : m_hovered;
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

    void WidgetTree::setPopupHost(PopupHost *host) noexcept {
        m_popupHost = host;
    }

    PopupHost *WidgetTree::popupHost() noexcept {
        return m_popupHost;
    }

    const PopupHost *WidgetTree::popupHost() const noexcept {
        return m_popupHost;
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

    bool WidgetTree::setArrangedZOffset(WidgetId owner,
                                        WidgetId child,
                                        float offset) {
        auto *node = findNode(child);
        if (node == nullptr || node->parent != owner ||
            !std::isfinite(offset)) {
            return false;
        }
        node->arrangedZOffset = offset;
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

    bool WidgetTree::isFocusable(WidgetId id) const noexcept {
        const auto *node = findNode(id);
        return node != nullptr && node->widget->traits().focusable &&
               isEffectivelyEnabled(id) && isEffectivelyVisible(id);
    }

    bool WidgetTree::focusAllowedByActiveScope(WidgetId id) const noexcept {
        size_t trappingIndex = m_focusScopes.size();
        for (size_t index = m_focusScopes.size(); index > 0; --index) {
            const auto &entry = m_focusScopes[index - 1];
            if (entry.policy.trapFocus && contains(entry.scope) &&
                isEffectivelyVisible(entry.scope) &&
                isEffectivelyEnabled(entry.scope)) {
                trappingIndex = index - 1;
                break;
            }
        }
        if (trappingIndex == m_focusScopes.size()) {
            return true;
        }
        if (isDescendantOf(id, m_focusScopes[trappingIndex].scope)) {
            return true;
        }
        // A later scope is considered owned by the trapping scope. This lets
        // an anchored popup opened by a modal dialog receive focus without
        // allowing arbitrary background widgets to escape the dialog trap.
        for (size_t index = trappingIndex + 1; index < m_focusScopes.size();
             ++index) {
            if (contains(m_focusScopes[index].scope) &&
                isDescendantOf(id, m_focusScopes[index].scope)) {
                return true;
            }
        }
        return false;
    }

    void WidgetTree::collectFocusable(WidgetId root,
                                      std::vector<WidgetId> &result) const {
        const auto *node = findNode(root);
        if (node == nullptr || !isEffectivelyVisible(root) ||
            !isEffectivelyEnabled(root)) {
            return;
        }
        if (isFocusable(root) && focusAllowedByActiveScope(root)) {
            result.push_back(root);
        }
        for (const auto child : node->children) {
            collectFocusable(child, result);
        }
    }

    void WidgetTree::resolvePendingAutoFocus() {
        for (auto &entry : m_focusScopes) {
            if (!entry.pendingAutoFocus || !contains(entry.scope) ||
                !isEffectivelyVisible(entry.scope) ||
                !isEffectivelyEnabled(entry.scope)) {
                continue;
            }
            entry.pendingAutoFocus = false;
            static_cast<void>(focusDefault(entry.scope));
        }
    }

    void WidgetTree::reconcileInteractionState() {
        if (m_reconcilingInteraction || m_destroying) {
            return;
        }

        m_reconcilingInteraction = true;
        try {
            // Crossing and focus callbacks may mutate visibility again. Walk
            // to a fixed point with a bounded iteration count so interaction
            // state always refers to a currently eligible widget without
            // permitting a hostile callback to spin forever.
            size_t remaining = m_nodes.size() + 4;
            while (remaining-- > 0) {
                bool changed = false;
                if (m_focused && (!isFocusable(m_focused) ||
                                  !focusAllowedByActiveScope(m_focused))) {
                    clearFocus();
                    changed = true;
                }
                if (m_pointerCapture &&
                    (!isEffectivelyEnabled(m_pointerCapture) ||
                     !isEffectivelyVisible(m_pointerCapture))) {
                    releasePointer();
                    changed = true;
                }

                const WidgetId pointed = m_lastPointerPosition.has_value()
                                             ? hitTest(*m_lastPointerPosition)
                                             : WidgetId{};
                if (pointed != m_hovered) {
                    updateHover(pointed);
                    changed = true;
                }
                if (!changed) {
                    break;
                }
            }
        } catch (...) {
            m_reconcilingInteraction = false;
            throw;
        }
        m_reconcilingInteraction = false;
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
        if (!contains(id) || m_removing.contains(id)) {
            return false;
        }

        auto *rootNode = findNode(id);
        if (rootNode == nullptr) {
            return false;
        }

        std::vector<WidgetId> subtree;
        collectSubtree(id, subtree);
        for (const auto widget : subtree) {
            m_removing.insert(widget);
        }

        std::exception_ptr failure;
        if (m_popupHost != nullptr) {
            try {
                static_cast<void>(m_popupHost->closeAnchoredInSubtree(id));
            } catch (...) {
                failure = std::current_exception();
            }
        }

        // Hold deferred destruction across focus/hover notifications as those
        // callbacks are allowed to request removal of this same subtree.
        beginCallback();

        const auto invokeAndRemember = [&failure](auto &&callback) {
            try {
                std::invoke(std::forward<decltype(callback)>(callback));
            } catch (...) {
                if (failure == nullptr) {
                    failure = std::current_exception();
                }
            }
        };

        std::vector<WidgetId> removedScopes;
        for (const auto &entry : m_focusScopes) {
            if (isDescendantOf(entry.scope, id)) {
                removedScopes.push_back(entry.scope);
            }
        }
        for (auto it = removedScopes.rbegin(); it != removedScopes.rend();
             ++it) {
            invokeAndRemember([this, scope = *it] {
                static_cast<void>(deactivateFocusScope(scope));
            });
        }

        if (m_focused && isDescendantOf(m_focused, id)) {
            invokeAndRemember([this] { clearFocus(); });
        }
        if (m_pointerCapture && isDescendantOf(m_pointerCapture, id)) {
            releasePointer();
        }
        if (m_hovered && isDescendantOf(m_hovered, id)) {
            invokeAndRemember([this] { updateHover({}); });
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
                invokeAndRemember([this, node, widget = *it] {
                    node->widget->onUnmount(*this, widget);
                });
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
        for (const auto widget : subtree) {
            m_removing.erase(widget);
        }
        m_invalidation |=
            WidgetInvalidation::layout | WidgetInvalidation::paint;
        invokeAndRemember([this] { endCallback(); });
        invokeAndRemember([this] { reconcileInteractionState(); });
        if (failure != nullptr) {
            std::rethrow_exception(failure);
        }
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
        std::exception_ptr failure;
        for (const auto id : pending) {
            if (contains(id)) {
                try {
                    static_cast<void>(removeWidgetNow(id));
                } catch (...) {
                    if (failure == nullptr) {
                        failure = std::current_exception();
                    }
                }
            }
        }
        if (failure != nullptr) {
            std::rethrow_exception(failure);
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
        try {
            node->widget->arrange(context);
        } catch (...) {
            endCallback();
            throw;
        }
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
        const float layoutZ =
            layout != nullptr && std::isfinite(layout->getZVal())
                ? layout->getZVal()
                : 0.f;
        const float combinedZ = layoutZ + node->arrangedZOffset;
        painter.pushLayer(std::isfinite(combinedZ) ? combinedZ : layoutZ);
        bool layerPushed = true;
        const bool clip = node->widget->traits().clipChildren;
        const WidgetBounds childClip =
            clip
                ? intersectBounds(bounds, node->widget->childClipBounds(bounds))
                : bounds;
        bool clipPushed = false;
        try {
            node->widget->paint(context);
            if (clip) {
                painter.pushClip(childClip);
                clipPushed = true;
            }
            if (!clip || !childClip.empty()) {
                for (const auto child : children) {
                    paintSubtree(child, painter);
                }
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
        const float layoutZ =
            layout != nullptr && std::isfinite(layout->getZVal())
                ? layout->getZVal()
                : 0.f;
        const float combinedZ = layoutZ + node->arrangedZOffset;
        const float localZ = std::isfinite(combinedZ) ? combinedZ : layoutZ;
        const float summedZ = ancestorZ + localZ;
        const float zIndex = std::isfinite(summedZ) ? summedZ : ancestorZ;
        const auto bounds = getBounds(id);
        HitTestResult frontmost;
        if (node->widget->hitTest(bounds, position) &&
            node->properties.hitTestVisible &&
            node->widget->traits().hitTestVisible) {
            frontmost = {.id = id, .zIndex = zIndex};
        }

        const bool clipChildren = node->widget->traits().clipChildren;
        const WidgetBounds childClip =
            clipChildren
                ? intersectBounds(bounds, node->widget->childClipBounds(bounds))
                : bounds;
        const bool insideChildClip =
            !clipChildren ||
            (!childClip.empty() && childClip.contains(position));
        if (insideChildClip) {
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
        m_invalidation |= WidgetInvalidation::paint;
        if (previous && contains(previous)) {
            dispatchDirect(previous, UIPointerCrossingEvent{.entered = false});
        }
        if (hovered && contains(hovered)) {
            m_hovered = hovered;
            dispatchDirect(hovered, UIPointerCrossingEvent{.entered = true});
        }
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
        try {
            bool stopped = false;
            for (auto it = route.rbegin(); it != route.rend() && !stopped;
                 ++it) {
                if (*it == target) {
                    continue;
                }
                const auto reply = invokeEvent(
                    *it, target, UIEventPhase::capture, event, pointer);
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
        } catch (...) {
            endCallback();
            throw;
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
        try {
            invokeEvent(target,
                        target,
                        UIEventPhase::target,
                        event,
                        m_lastPointerPosition);
        } catch (...) {
            endCallback();
            throw;
        }
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
