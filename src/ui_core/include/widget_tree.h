#pragma once

#include "common/bess_api.h"
#include "common/types.h"
#include "layout.h"
#include "ui_style.h"
#include "widget.h"

#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <span>
#include <type_traits>
#include <utility>
#include <vector>

namespace Bess::UI {
    class UIPainter;
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
            try {
                std::invoke(std::forward<Mutation>(mutation), *widget);
            } catch (...) {
                endCallback();
                throw;
            }
            endCallback();
            if (contains(id)) {
                invalidate(id, invalidation);
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
        [[nodiscard]] UITheme &theme() noexcept;
        [[nodiscard]] const UITheme &theme() const noexcept;
        void setTheme(UITheme theme);

        void performLayout();
        [[nodiscard]] WidgetBounds getBounds(WidgetId id) const noexcept;

        void update(TimeMs deltaTime);
        void paint(UIPainter &painter);
        [[nodiscard]] UIDispatchResult dispatchEvent(const UIEvent &event);

        [[nodiscard]] WidgetId hitTest(glm::vec2 uiPosition) const;
        [[nodiscard]] WidgetId getFocusedWidget() const noexcept;
        bool setFocus(WidgetId id);
        void clearFocus();
        [[nodiscard]] WidgetId getPointerCapture() const noexcept;
        bool capturePointer(WidgetId id);
        void releasePointer(WidgetId id = {});
        [[nodiscard]] WidgetId getHoveredWidget() const noexcept;
        [[nodiscard]] CursorIcon getCursorShape() const noexcept;

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
        uint32_t m_nextRuntimeId = 1;
        uint32_t m_callbackDepth = 0;
        bool m_destroying = false;
        bool m_clearPending = false;
        glm::vec2 m_viewportSize{0.f, 0.f};
        UITheme m_theme = UITheme::dark();
        uint64_t m_themeRevision = 1;
        uint64_t m_layoutThemeRevision = 0;
        std::optional<glm::vec2> m_lastPointerPosition;
        WidgetId m_focused;
        WidgetId m_pointerCapture;
        WidgetId m_hovered;
        bool m_changingFocus = false;
        std::optional<WidgetId> m_deferredFocus;
        WidgetInvalidation m_invalidation =
            WidgetInvalidation::layout | WidgetInvalidation::paint;
        std::shared_ptr<Detail::WidgetTreeControl> m_control;
    };

} // namespace Bess::UI
