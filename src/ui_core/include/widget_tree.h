#pragma once

#include "common/bess_api.h"
#include "common/types.h"
#include "drag_drop.h"
#include "layout.h"
#include "models/action_registry.h"
#include "ui_platform_services.h"
#include "ui_style.h"
#include "widget.h"

#include <cstddef>
#include <cstdint>
#include <exception>
#include <functional>
#include <memory>
#include <optional>
#include <span>
#include <type_traits>
#include <utility>
#include <vector>

namespace Bess::UI {
    class UIPainter;
    class PopupHost;
    class WidgetTree;
    template <typename T> class WidgetRef;

    namespace Detail {
        // Shared only as a lifetime sentinel. WidgetTree and WidgetRef remain
        // single-threaded; the indirection prevents a retained handle from
        // dereferencing a tree that has already been destroyed.
        struct WidgetTreeControl {
            WidgetTree *tree = nullptr;
        };
    } // namespace Detail

    struct WidgetProperties {
        WidgetVisibility visibility = WidgetVisibility::visible;
        bool enabled = true;
        bool hitTestVisible = true;
    };

    struct UIDispatchResult {
        WidgetId target;
        bool handled = false;
    };

    // WidgetTree is the retained tree and interaction authority for one target.
    // It owns widgets and layout nodes, while callers retain only stable IDs.
    class BESS_API WidgetTree {
        template <typename T> friend class WidgetRef;

      public:
        static constexpr size_t append = static_cast<size_t>(-1);

        WidgetTree();
        WidgetTree(const WidgetTree &) = delete;
        WidgetTree(WidgetTree &&) = delete;
        ~WidgetTree();
        WidgetTree &operator=(const WidgetTree &) = delete;
        WidgetTree &operator=(WidgetTree &&) = delete;

        WidgetId addWidget(std::unique_ptr<Widget> widget,
                           WidgetId parent = {},
                           size_t index = append);

        template <typename T, typename... Args>
            requires std::derived_from<T, Widget>
        WidgetId emplaceWidget(Args &&...args) {
            return addWidget(std::make_unique<T>(std::forward<Args>(args)...));
        }

        template <typename T, typename... Args>
            requires std::derived_from<T, Widget>
        WidgetId emplaceChild(WidgetId parent, Args &&...args) {
            return addWidget(std::make_unique<T>(std::forward<Args>(args)...),
                             parent);
        }

        bool removeWidget(WidgetId id);
        void clear();
        bool
        reparentWidget(WidgetId id, WidgetId newParent, size_t index = append);

        [[nodiscard]] bool contains(WidgetId id) const noexcept;
        [[nodiscard]] Widget *getWidget(WidgetId id) noexcept;
        [[nodiscard]] const Widget *getWidget(WidgetId id) const noexcept;

        template <typename T>
            requires std::derived_from<T, Widget>
        [[nodiscard]] T *getWidget(WidgetId id) noexcept {
            return dynamic_cast<T *>(getWidget(id));
        }

        template <typename T>
            requires std::derived_from<T, Widget>
        [[nodiscard]] const T *getWidget(WidgetId id) const noexcept {
            return dynamic_cast<const T *>(getWidget(id));
        }

        // Applies a mutation while keeping callback-time destruction safe and
        // invalidation explicit. This is the preferred way to call setters on
        // retained controls whose intrinsic size or appearance may change.
        template <typename T, typename Mutation>
            requires std::derived_from<T, Widget> &&
                     std::invocable<Mutation, T &>
        bool mutateWidget(WidgetId id,
                          WidgetInvalidation invalidation,
                          Mutation &&mutation) {
            auto *widget = getWidget<T>(id);
            if (widget == nullptr) {
                return false;
            }
            beginCallback();
            std::exception_ptr failure;
            try {
                std::invoke(std::forward<Mutation>(mutation), *widget);
            } catch (...) {
                failure = std::current_exception();
            }
            try {
                endCallback();
            } catch (...) {
                if (failure == nullptr) {
                    failure = std::current_exception();
                }
            }
            if (contains(id)) {
                invalidate(id, invalidation);
            }
            if (failure != nullptr) {
                std::rethrow_exception(failure);
            }
            return true;
        }

        // Layout mutations use the same callback lifetime contract as widget
        // mutations. In particular, removing the widget from inside the
        // mutation is deferred until the borrowed LayoutNode is no longer in
        // use.
        template <typename Mutation>
            requires std::invocable<Mutation, LayoutNode &>
        bool mutateLayout(WidgetId id, Mutation &&mutation) {
            if (!contains(id)) {
                return false;
            }
            auto *layout = getLayout(id);
            if (layout == nullptr) {
                return false;
            }
            beginCallback();
            std::exception_ptr failure;
            try {
                std::invoke(std::forward<Mutation>(mutation), *layout);
            } catch (...) {
                failure = std::current_exception();
            }
            try {
                endCallback();
            } catch (...) {
                if (failure == nullptr) {
                    failure = std::current_exception();
                }
            }
            if (contains(id)) {
                invalidate(
                    id, WidgetInvalidation::layout | WidgetInvalidation::paint);
            }
            if (failure != nullptr) {
                std::rethrow_exception(failure);
            }
            return true;
        }

        [[nodiscard]] LayoutNode *getLayout(WidgetId id) noexcept;
        [[nodiscard]] const LayoutNode *getLayout(WidgetId id) const noexcept;
        [[nodiscard]] LayoutNodeRegistry &layoutRegistry() noexcept;
        [[nodiscard]] const LayoutNodeRegistry &layoutRegistry() const noexcept;

        [[nodiscard]] WidgetId getParent(WidgetId id) const noexcept;
        [[nodiscard]] std::span<const WidgetId>
        getChildren(WidgetId id) const noexcept;
        [[nodiscard]] std::span<const WidgetId> getRoots() const noexcept;

        bool setVisibility(WidgetId id, WidgetVisibility visibility);
        [[nodiscard]] WidgetVisibility
        getVisibility(WidgetId id) const noexcept;
        bool setEnabled(WidgetId id, bool enabled);
        [[nodiscard]] bool isEnabled(WidgetId id) const noexcept;
        bool setHitTestVisible(WidgetId id, bool visible);

        void setViewportSize(glm::vec2 size);
        [[nodiscard]] glm::vec2 getViewportSize() const noexcept;
        [[nodiscard]] const UITheme &theme() const noexcept;
        void setTheme(UITheme theme);
        void setPlatformServices(std::shared_ptr<UIPlatformServices> services);
        [[nodiscard]] UIPlatformServices &platformServices() noexcept;
        [[nodiscard]] UIPlatformServices &platformServices() const noexcept;
        // Registry replacement is allowed only while the tree is empty;
        // mounted action bindings retain signal connections to their registry.
        void setActionRegistry(std::shared_ptr<ActionRegistry> registry);
        [[nodiscard]] std::shared_ptr<ActionRegistry>
        actionRegistry() const noexcept;
        [[nodiscard]] ActionRegistry &actions() noexcept;
        [[nodiscard]] const ActionRegistry &actions() const noexcept;
        // Like ActionRegistry, replacement is allowed only before mounting
        // widgets because drag source/target registrations retain the service.
        void setDragDropService(std::shared_ptr<DragDropService> service);
        [[nodiscard]] std::shared_ptr<DragDropService>
        dragDropService() const noexcept;
        [[nodiscard]] DragDropService &dragDrop() noexcept;
        [[nodiscard]] const DragDropService &dragDrop() const noexcept;

        void performLayout();
        [[nodiscard]] WidgetBounds getBounds(WidgetId id) const noexcept;

        void update(TimeMs deltaTime);
        void prepareRender(
            const std::shared_ptr<Core::Renderer::IRenderer2D> &renderer,
            TimeMs deltaTime,
            float contentScale = 1.f);
        void paint(UIPainter &painter);
        [[nodiscard]] UIDispatchResult dispatchEvent(const UIEvent &event);

        [[nodiscard]] WidgetId hitTest(glm::vec2 uiPosition) const;
        [[nodiscard]] WidgetId getFocusedWidget() const noexcept;
        bool setFocus(WidgetId id);
        void clearFocus();
        bool moveFocus(FocusTraversalDirection direction =
                           FocusTraversalDirection::forward);
        bool activateFocusScope(WidgetId scope, FocusScopePolicy policy = {});
        bool deactivateFocusScope(WidgetId scope);
        bool setDefaultFocus(WidgetId scope, WidgetId widget);
        bool focusDefault(WidgetId scope);
        [[nodiscard]] WidgetId activeFocusScope() const noexcept;
        [[nodiscard]] WidgetId getPointerCapture() const noexcept;
        bool capturePointer(WidgetId id);
        void releasePointer(WidgetId id = {});
        [[nodiscard]] WidgetId getHoveredWidget() const noexcept;
        [[nodiscard]] std::optional<glm::vec2>
        getPointerPosition() const noexcept;
        [[nodiscard]] CursorIcon getCursorShape() const noexcept;
        void setPopupHost(PopupHost *host) noexcept;
        [[nodiscard]] PopupHost *popupHost() noexcept;
        [[nodiscard]] const PopupHost *popupHost() const noexcept;

        [[nodiscard]] PickingId getPickingId(WidgetId id,
                                             uint32_t info = 0) const noexcept;
        [[nodiscard]] WidgetId
        resolvePickingId(PickingId pickingId) const noexcept;

        void invalidate(WidgetId id, WidgetInvalidation invalidation);
        [[nodiscard]] WidgetInvalidation pendingInvalidation() const noexcept;
        WidgetInvalidation consumeInvalidation() noexcept;

        // Used only by WidgetArrangeContext. A widget may arrange its direct
        // children, never arbitrary nodes elsewhere in the tree.
        bool
        setArrangedBounds(WidgetId owner, WidgetId child, WidgetBounds bounds);
        bool setArrangedVisible(WidgetId owner, WidgetId child, bool visible);
        bool setArrangedZOffset(WidgetId owner, WidgetId child, float offset);

      private:
        struct Node {
            std::unique_ptr<Widget> widget;
            WidgetId parent;
            std::vector<WidgetId> children;
            WidgetProperties properties;
            std::optional<WidgetBounds> arrangedBounds;
            bool arrangedVisible = true;
            float arrangedZOffset = 0.f;
            uint32_t runtimeId = PickingId::invalidRuntimeId;
        };

        using Nodes = NodeHashMap<WidgetId, Node>;

        struct HitTestResult {
            WidgetId id;
            float zIndex = 0.f;
        };

        [[nodiscard]] Node *findNode(WidgetId id) noexcept;
        [[nodiscard]] const Node *findNode(WidgetId id) const noexcept;
        [[nodiscard]] bool isDescendantOf(WidgetId id,
                                          WidgetId ancestor) const noexcept;
        [[nodiscard]] bool isEffectivelyEnabled(WidgetId id) const noexcept;
        [[nodiscard]] bool isEffectivelyVisible(WidgetId id) const noexcept;
        [[nodiscard]] bool isFocusable(WidgetId id) const noexcept;
        [[nodiscard]] bool
        focusAllowedByActiveScope(WidgetId id) const noexcept;
        void collectFocusable(WidgetId root,
                              std::vector<WidgetId> &result) const;
        void resolvePendingAutoFocus();
        void reconcileInteractionState();
        void syncLayoutChildren(WidgetId parent);

        bool removeWidgetNow(WidgetId id);
        void collectSubtree(WidgetId id, std::vector<WidgetId> &ids) const;
        void flushPendingRemovals();

        void arrangeSubtree(WidgetId id);
        void updateLayoutSubtree(WidgetId id, bool themeChanged);
        void updateSubtree(WidgetId id, TimeMs deltaTime);
        void paintSubtree(WidgetId id, UIPainter &painter);
        [[nodiscard]] HitTestResult hitTestSubtree(WidgetId id,
                                                   glm::vec2 position,
                                                   bool ancestorsEnabled,
                                                   float ancestorZ) const;

        [[nodiscard]] std::optional<glm::vec2>
        pointerPosition(const UIEvent &event) const noexcept;
        [[nodiscard]] std::vector<DropTargetCandidate>
        collectDropTargets(WidgetId pointed, glm::vec2 position) const;
        UIDispatchResult
        dispatchExternalDrag(const ExternalDragEvent &event,
                             const Input::Modifiers &modifiers);
        void updateHover(WidgetId hovered);
        UIDispatchResult dispatchToTarget(WidgetId target,
                                          const UIEvent &event,
                                          std::optional<glm::vec2> pointer);
        UIEventReply invokeEvent(WidgetId current,
                                 WidgetId target,
                                 UIEventPhase phase,
                                 const UIEvent &event,
                                 std::optional<glm::vec2> pointer);
        void applyReply(WidgetId current, const UIEventReply &reply);
        void dispatchDirect(WidgetId target, const UIEvent &event);

        void beginCallback() noexcept;
        void endCallback();
        [[nodiscard]] uint32_t allocateRuntimeId();
        void releaseRuntimeId(uint32_t runtimeId);

        Nodes m_nodes;
        std::vector<WidgetId> m_roots;
        LayoutNodeRegistry m_layoutRegistry;
        HashMap<uint32_t, WidgetId> m_runtimeToWidget;
        std::vector<WidgetId> m_pendingRemovals;
        // Stable insertion order gives offscreen producers deterministic
        // sequencing without a full-tree traversal on every frame.
        std::vector<WidgetId> m_renderPrepareWidgets;
        HashSet<WidgetId> m_removing;
        uint32_t m_nextRuntimeId = 1;
        uint32_t m_callbackDepth = 0;
        bool m_destroying = false;
        bool m_clearPending = false;
        glm::vec2 m_viewportSize{0.f, 0.f};
        UITheme m_theme = UITheme::dark();
        std::shared_ptr<UIPlatformServices> m_platformServices =
            nullUIPlatformServices();
        std::shared_ptr<ActionRegistry> m_actionRegistry;
        std::shared_ptr<DragDropService> m_dragDropService;
        uint64_t m_themeRevision = 1;
        uint64_t m_layoutThemeRevision = 0;
        std::optional<glm::vec2> m_lastPointerPosition;
        WidgetId m_focused;
        WidgetId m_pointerCapture;
        WidgetId m_hovered;
        struct FocusScopeEntry {
            WidgetId scope;
            WidgetId defaultFocus;
            WidgetId previousFocus;
            FocusScopePolicy policy;
            bool pendingAutoFocus = false;
        };
        std::vector<FocusScopeEntry> m_focusScopes;
        bool m_changingFocus = false;
        std::optional<WidgetId> m_deferredFocus;
        bool m_reconcilingInteraction = false;
        WidgetInvalidation m_invalidation =
            WidgetInvalidation::layout | WidgetInvalidation::paint;
        std::shared_ptr<Detail::WidgetTreeControl> m_control;
        PopupHost *m_popupHost = nullptr;
    };

} // namespace Bess::UI
