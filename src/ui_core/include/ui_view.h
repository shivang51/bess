#pragma once

#include "common/bess_api.h"
#include "common/types.h"
#include "ui_composer.h"

#include <concepts>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <type_traits>
#include <utility>
#include <vector>

namespace Bess::UI {

    class UIViewHost;
    template <typename T> class UIViewRef;

    namespace Detail {
        struct UIViewHostControl {
            UIViewHost *host = nullptr;
        };
    } // namespace Detail

    enum class UIViewLayer : uint8_t { content, overlay, modal, popup };

    struct UIViewContext {
        UIViewHost &host;
        WidgetTree &tree;
        ViewId id;
        WidgetRef<Widget> root;
    };

    // Mountable unit of UI composition. A view owns presentation state and
    // callbacks, while its widgets remain exclusively owned by WidgetTree.
    class BESS_API UIView {
      public:
        virtual ~UIView();

        virtual void compose(UIComposer &ui) = 0;
        virtual void onMounted(UIViewContext &context);
        virtual void onUnmounting(UIViewContext &context) noexcept;
    };

    // Owns views mounted into one WidgetTree and maintains their paint/hit-test
    // order by layer. UIViewHost is single-threaded, matching WidgetTree.
    class BESS_API UIViewHost {
      public:
        explicit UIViewHost(WidgetTree &tree);
        UIViewHost(const UIViewHost &) = delete;
        UIViewHost(UIViewHost &&) = delete;
        ~UIViewHost();
        UIViewHost &operator=(const UIViewHost &) = delete;
        UIViewHost &operator=(UIViewHost &&) = delete;

        UIViewRef<UIView> setContent(std::unique_ptr<UIView> view);
        UIViewRef<UIView> mountOverlay(std::unique_ptr<UIView> view);
        UIViewRef<UIView> mountModal(std::unique_ptr<UIView> view);
        UIViewRef<UIView> mountPopup(std::unique_ptr<UIView> view);

        template <typename T, typename... Args>
            requires std::derived_from<T, UIView> &&
                     std::constructible_from<T, Args...>
        UIViewRef<T> setContent(Args &&...args) {
            const auto mounted =
                setContent(std::make_unique<T>(std::forward<Args>(args)...));
            return UIViewRef<T>{m_control, mounted.id()};
        }

        template <typename T, typename... Args>
            requires std::derived_from<T, UIView> &&
                     std::constructible_from<T, Args...>
        UIViewRef<T> mountOverlay(Args &&...args) {
            const auto mounted =
                mountOverlay(std::make_unique<T>(std::forward<Args>(args)...));
            return UIViewRef<T>{m_control, mounted.id()};
        }

        template <typename T, typename... Args>
            requires std::derived_from<T, UIView> &&
                     std::constructible_from<T, Args...>
        UIViewRef<T> mountModal(Args &&...args) {
            const auto mounted =
                mountModal(std::make_unique<T>(std::forward<Args>(args)...));
            return UIViewRef<T>{m_control, mounted.id()};
        }

        bool unmount(ViewId id);
        void clear() noexcept;

        // Completes destruction that had to be deferred because a view was
        // unmounted from inside a WidgetTree callback. UITarget calls this at
        // traversal boundaries; direct hosts may call it after dispatching.
        void flushPendingUnmounts() noexcept;

        [[nodiscard]] UIView *getView(ViewId id) noexcept;
        [[nodiscard]] const UIView *getView(ViewId id) const noexcept;

        template <typename T>
            requires std::derived_from<T, UIView>
        [[nodiscard]] T *getView(ViewId id) noexcept {
            return dynamic_cast<T *>(getView(id));
        }

        template <typename T>
            requires std::derived_from<T, UIView>
        [[nodiscard]] const T *getView(ViewId id) const noexcept {
            return dynamic_cast<const T *>(getView(id));
        }

        [[nodiscard]] WidgetId rootOf(ViewId id) const noexcept;
        [[nodiscard]] std::optional<UIViewLayer>
        layerOf(ViewId id) const noexcept;
        [[nodiscard]] ViewId content() const noexcept;
        [[nodiscard]] size_t size() const noexcept;
        [[nodiscard]] WidgetTree &tree() const noexcept;

      private:
        struct Entry {
            std::unique_ptr<UIView> view;
            WidgetId root;
            UIViewLayer layer = UIViewLayer::content;
            bool unmounting = false;
        };

        UIViewRef<UIView> mountOwned(std::unique_ptr<UIView> view,
                                     UIViewLayer layer);
        [[nodiscard]] size_t orderIndexFor(UIViewLayer layer) const noexcept;
        [[nodiscard]] size_t rootIndexFor(size_t orderIndex) const noexcept;
        void refreshRootDepths() noexcept;
        void eraseOrder(ViewId id) noexcept;
        void rollbackMount(ViewId id, WidgetId root) noexcept;

        WidgetTree &m_tree;
        std::shared_ptr<Detail::UIViewHostControl> m_control;
        NodeHashMap<ViewId, Entry> m_entries;
        std::vector<ViewId> m_order;
        std::vector<Entry> m_pendingUnmounts;
        ViewId m_content;
    };

    // Safe, typed access to a mounted UIView. Like WidgetRef, it is non-owning
    // and becomes empty after unmount or host destruction.
    template <typename T = UIView> class UIViewRef {
        static_assert(std::derived_from<T, UIView>,
                      "UIViewRef<T> requires T to derive from UIView");

      public:
        UIViewRef() = default;

        [[nodiscard]] ViewId id() const noexcept {
            return m_id;
        }

        [[nodiscard]] T *get() const noexcept {
            auto control = m_control.lock();
            return control != nullptr && control->host != nullptr
                       ? control->host->getView<T>(m_id)
                       : nullptr;
        }

        [[nodiscard]] WidgetRef<Widget> root() const noexcept {
            auto control = m_control.lock();
            if (control == nullptr || control->host == nullptr) {
                return {};
            }
            const WidgetId root = control->host->rootOf(m_id);
            return root ? WidgetRef<Widget>{control->host->tree(), root}
                        : WidgetRef<Widget>{};
        }

        [[nodiscard]] bool exists() const noexcept {
            return get() != nullptr;
        }

        [[nodiscard]] explicit operator bool() const noexcept {
            return exists();
        }

        bool unmount() const {
            auto control = m_control.lock();
            return control != nullptr && control->host != nullptr &&
                   control->host->unmount(m_id);
        }

        friend bool operator==(const UIViewRef &lhs,
                               const UIViewRef &rhs) noexcept {
            const bool sameOwner = !lhs.m_control.owner_before(rhs.m_control) &&
                                   !rhs.m_control.owner_before(lhs.m_control);
            return sameOwner && lhs.m_id == rhs.m_id;
        }

      private:
        friend class UIViewHost;

        UIViewRef(std::weak_ptr<Detail::UIViewHostControl> control,
                  ViewId id) noexcept
            : m_control(std::move(control)),
              m_id(id) {
        }

        std::weak_ptr<Detail::UIViewHostControl> m_control;
        ViewId m_id;
    };

} // namespace Bess::UI
