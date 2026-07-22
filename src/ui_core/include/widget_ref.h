#pragma once

#include "widget_tree.h"

#include <concepts>
#include <functional>
#include <memory>
#include <type_traits>
#include <utility>

namespace Bess::UI {

    // Copyable, non-owning access to a retained widget. The stable ID prevents
    // aliasing a replacement widget, while the weak tree control makes a ref
    // safely become empty when either the widget or its WidgetTree is gone.
    // Returned pointers are borrowed and must not be kept across UI callbacks.
    template <typename T = Widget> class WidgetRef {
        static_assert(std::derived_from<T, Widget>,
                      "WidgetRef<T> requires T to derive from Widget");

      public:
        WidgetRef() = default;

        WidgetRef(WidgetTree &tree, WidgetId id) noexcept
            : m_control(tree.m_control),
              m_id(tree.getWidget<T>(id) != nullptr ? id : WidgetId{}) {
        }

        [[nodiscard]] WidgetId id() const noexcept {
            return m_id;
        }

        [[nodiscard]] T *get() const noexcept {
            auto control = m_control.lock();
            return control != nullptr && control->tree != nullptr
                       ? control->tree->template getWidget<T>(m_id)
                       : nullptr;
        }

        [[nodiscard]] WidgetTree *tree() const noexcept {
            auto control = m_control.lock();
            return control != nullptr ? control->tree : nullptr;
        }

        [[nodiscard]] bool exists() const noexcept {
            return get() != nullptr;
        }

        [[nodiscard]] explicit operator bool() const noexcept {
            return exists();
        }

        template <typename Mutation>
            requires std::invocable<Mutation, T &>
        bool update(WidgetInvalidation invalidation,
                    Mutation &&mutation) const {
            auto *owner = tree();
            return owner != nullptr &&
                   owner->template mutateWidget<T>(
                       m_id, invalidation, std::forward<Mutation>(mutation));
        }

        template <typename Mutation>
            requires std::invocable<Mutation, T &>
        bool update(Mutation &&mutation) const {
            return update(WidgetInvalidation::layout |
                              WidgetInvalidation::paint,
                          std::forward<Mutation>(mutation));
        }

        // "mutate" is the explicit form for application code; "update" is
        // retained as a compatibility alias.
        template <typename Mutation>
            requires std::invocable<Mutation, T &>
        bool mutate(WidgetInvalidation invalidation,
                    Mutation &&mutation) const {
            return update(invalidation, std::forward<Mutation>(mutation));
        }

        template <typename Mutation>
            requires std::invocable<Mutation, T &>
        bool mutate(Mutation &&mutation) const {
            return update(std::forward<Mutation>(mutation));
        }

        template <typename Mutation>
            requires std::invocable<Mutation, LayoutNode &>
        bool updateLayout(Mutation &&mutation) const {
            auto *owner = tree();
            return owner != nullptr &&
                   owner->template getWidget<T>(m_id) != nullptr &&
                   owner->mutateLayout(m_id, std::forward<Mutation>(mutation));
        }

        bool setLayout(const LayoutSpec &spec) const {
            return updateLayout(
                [&spec](LayoutNode &layout) { spec.apply(layout); });
        }

        // Fluent composition helper. A failed/dead handle stays visibly empty
        // rather than silently returning the original stale reference.
        [[nodiscard]] WidgetRef withLayout(const LayoutSpec &spec) const {
            return setLayout(spec) ? *this : WidgetRef{};
        }

        bool setEnabled(bool enabled) const {
            auto *owner = tree();
            return owner != nullptr && owner->setEnabled(m_id, enabled);
        }

        [[nodiscard]] bool isEnabled() const noexcept {
            const auto *owner = tree();
            return owner != nullptr && owner->isEnabled(m_id);
        }

        bool setVisibility(WidgetVisibility visibility) const {
            auto *owner = tree();
            return owner != nullptr && owner->setVisibility(m_id, visibility);
        }

        [[nodiscard]] WidgetVisibility visibility() const noexcept {
            const auto *owner = tree();
            return owner != nullptr ? owner->getVisibility(m_id)
                                    : WidgetVisibility::collapsed;
        }

        // Hidden widgets retain their layout slot; collapsed widgets do not.
        bool show() const {
            return setVisibility(WidgetVisibility::visible);
        }

        bool hide() const {
            return setVisibility(WidgetVisibility::hidden);
        }

        bool collapse() const {
            return setVisibility(WidgetVisibility::collapsed);
        }

        bool setHitTestVisible(bool visible) const {
            auto *owner = tree();
            return owner != nullptr && owner->setHitTestVisible(m_id, visible);
        }

        bool focus() const {
            auto *owner = tree();
            return owner != nullptr && owner->setFocus(m_id);
        }

        bool blur() const {
            auto *owner = tree();
            if (owner == nullptr || owner->getFocusedWidget() != m_id) {
                return false;
            }
            owner->clearFocus();
            return !owner->getFocusedWidget();
        }

        [[nodiscard]] bool isFocused() const noexcept {
            const auto *owner = tree();
            return owner != nullptr && owner->getFocusedWidget() == m_id;
        }

        bool invalidate(WidgetInvalidation invalidation) const {
            auto *owner = tree();
            if (owner == nullptr || !owner->contains(m_id)) {
                return false;
            }
            owner->invalidate(m_id, invalidation);
            return true;
        }

        bool remove() const {
            auto *owner = tree();
            return owner != nullptr && owner->removeWidget(m_id);
        }

        template <typename U>
            requires std::derived_from<U, Widget>
        [[nodiscard]] WidgetRef<U> as() const noexcept {
            auto *owner = tree();
            return owner != nullptr ? WidgetRef<U>{*owner, m_id}
                                    : WidgetRef<U>{};
        }

        friend bool operator==(const WidgetRef &lhs,
                               const WidgetRef &rhs) noexcept {
            const bool sameOwner = !lhs.m_control.owner_before(rhs.m_control) &&
                                   !rhs.m_control.owner_before(lhs.m_control);
            return sameOwner && lhs.m_id == rhs.m_id;
        }

      private:
        std::weak_ptr<Detail::WidgetTreeControl> m_control;
        WidgetId m_id;
    };

} // namespace Bess::UI
