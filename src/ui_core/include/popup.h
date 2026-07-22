#pragma once

#include "controls/basic_widgets.h"

#include <functional>
#include <memory>
#include <optional>

namespace Bess::UI {

    enum class PopupSide : uint8_t { bottom, top, right, left };
    enum class PopupAlignment : uint8_t { start, center, end };

    struct PopupAnchor {
        WidgetId widget;
        std::optional<WidgetBounds> bounds;
        std::optional<glm::vec2> point;

        [[nodiscard]] static PopupAnchor forWidget(WidgetId id) noexcept;
        [[nodiscard]] static PopupAnchor forBounds(WidgetBounds value) noexcept;
        [[nodiscard]] static PopupAnchor forPoint(glm::vec2 value) noexcept;
    };

    struct AnchoredPopupOptions {
        PopupAnchor anchor;
        PopupSide preferredSide = PopupSide::bottom;
        PopupAlignment alignment = PopupAlignment::start;
        float gap = 4.f;
        glm::vec2 offset{0.f, 0.f};
        glm::vec2 minimumSize{0.f, 0.f};
        // Non-positive components mean that only the viewport constrains the
        // corresponding axis.
        glm::vec2 maximumSize{0.f, 0.f};
        float viewportMargin = 8.f;
        bool matchAnchorWidth = false;
        bool allowFlip = true;
        bool dismissOnOutsidePress = true;
        bool dismissOnEscape = true;
        // Keep the anchor interactive while the popup is open. Editable
        // comboboxes use this to preserve pointer selection in their field.
        bool passThroughAnchor = false;
        bool closeWhenAnchorGone = true;
        bool interactive = true;
        FocusScopePolicy focus{
            .trapFocus = false,
            .autoFocus = false,
            .restoreFocus = true,
        };
        std::optional<UIBoxStyle> style;
        FlexContainerOptions content{
            .direction = LayoutDirection::vertical,
            .mainAxisAlignment = LayoutAlignment::start,
            .crossAxisAlignment = LayoutAlignment::start,
            .stretchWidth = false,
            .stretchHeight = false,
            .clipChildren = true,
            .hitTestVisible = false,
        };
    };

    struct PopupPlacementResult {
        WidgetBounds bounds;
        PopupSide side = PopupSide::bottom;
    };

    class BESS_API PopupPlacementSolver {
      public:
        [[nodiscard]] static PopupPlacementResult
        calculate(WidgetBounds viewport,
                  WidgetBounds anchor,
                  glm::vec2 desiredSize,
                  const AnchoredPopupOptions &options) noexcept;
    };

    class PopupHost;
    class UIComposer;
    class UIView;
    class UIViewHost;
    template <typename T> class WidgetRef;

    namespace Detail {
        struct PopupHostControl {
            PopupHost *host = nullptr;
        };
    } // namespace Detail

    class BESS_API PopupHandle {
      public:
        PopupHandle() = default;

        [[nodiscard]] PopupId id() const noexcept;
        [[nodiscard]] bool isOpen() const noexcept;
        [[nodiscard]] explicit operator bool() const noexcept;
        bool close() const;
        [[nodiscard]] WidgetRef<Widget> layer() const noexcept;
        [[nodiscard]] WidgetRef<FlexContainer> content() const noexcept;

      private:
        friend class PopupHost;
        PopupHandle(std::weak_ptr<Detail::PopupHostControl> control,
                    PopupId id) noexcept;

        std::weak_ptr<Detail::PopupHostControl> m_control;
        PopupId m_id;
    };

    // Full-viewport interaction layer that owns the popup's resolved bounds.
    // Applications normally create it through PopupHost rather than directly.
    class BESS_API AnchoredPopup : public Widget {
      public:
        using Dismissed = std::function<void()>;

        AnchoredPopup(AnchoredPopupOptions options, Dismissed dismissed);

        [[nodiscard]] std::string_view typeName() const noexcept override;
        [[nodiscard]] WidgetTraits traits() const noexcept override;
        void onMount(WidgetMountContext &context) override;
        void onUnmount(WidgetTree &state, WidgetId id) override;
        void arrange(WidgetArrangeContext &context) override;
        void paint(WidgetPaintContext &context) const override;
        [[nodiscard]] bool hitTest(WidgetBounds bounds,
                                   glm::vec2 position) const noexcept override;
        UIEventReply onEvent(WidgetEventContext &context,
                             const UIEvent &event) override;

        [[nodiscard]] WidgetBounds popupBounds() const noexcept;
        [[nodiscard]] PopupSide resolvedSide() const noexcept;
        [[nodiscard]] const AnchoredPopupOptions &options() const noexcept;

      private:
        [[nodiscard]] WidgetBounds resolveAnchor(const WidgetTree &state) const;

        AnchoredPopupOptions m_options;
        Dismissed m_dismissed;
        WidgetBounds m_popupBounds;
        PopupSide m_resolvedSide = PopupSide::bottom;
        WidgetTree *m_state = nullptr;
        WidgetId m_id;
    };

    class BESS_API PopupHost {
      public:
        using Builder = std::function<void(UIComposer &)>;

        explicit PopupHost(UIViewHost &views);
        PopupHost(const PopupHost &) = delete;
        PopupHost(PopupHost &&) = delete;
        ~PopupHost();
        PopupHost &operator=(const PopupHost &) = delete;
        PopupHost &operator=(PopupHost &&) = delete;

        PopupHandle open(AnchoredPopupOptions options, Builder build);
        bool close(PopupId id);
        bool closeTopmost();
        bool dismissTopmostOnEscape();
        // Closes anchored popups before their owner subtree disappears. This
        // preserves nested focus restoration (popup -> dialog -> opener) and
        // prevents a one-frame orphaned overlay.
        size_t closeAnchoredInSubtree(WidgetId subtree);
        void clear() noexcept;
        void update();

        [[nodiscard]] bool contains(PopupId id) const noexcept;
        [[nodiscard]] size_t size() const noexcept;
        [[nodiscard]] PopupHandle topmost() const noexcept;
        [[nodiscard]] WidgetRef<Widget> layer(PopupId id) const noexcept;
        [[nodiscard]] WidgetRef<FlexContainer>
        content(PopupId id) const noexcept;

      private:
        class PopupView;
        friend class PopupView;
        friend class PopupHandle;

        struct Entry {
            ViewId view;
            WidgetId layer;
            WidgetId content;
            WidgetId anchor;
            bool closeWhenAnchorGone = true;
            bool dismissOnEscape = true;
            bool interactive = true;
        };

        void detached(PopupId id, ViewId view) noexcept;

        UIViewHost &m_views;
        std::shared_ptr<Detail::PopupHostControl> m_control;
        NodeHashMap<PopupId, Entry> m_entries;
        std::vector<PopupId> m_order;
        bool m_clearing = false;
    };

} // namespace Bess::UI
