#pragma once

#include "common/bess_api.h"
#include "common/types.h"
#include "ui_event.h"
#include "ui_types.h"

#include <span>
#include <string_view>

namespace Bess::UI {
    class LayoutNode;
    class UIPainter;
    class WidgetTree;

    struct WidgetMountContext {
        WidgetTree &state;
        WidgetId id;
        LayoutNode &layout;
    };

    struct WidgetUpdateContext {
        WidgetTree &state;
        WidgetId id;
        TimeMs deltaTime;
    };

    struct WidgetLayoutContext {
        WidgetTree &state;
        WidgetId id;
        LayoutNode &layout;
        bool themeChanged = false;
    };

    struct WidgetArrangeContext {
        WidgetTree &state;
        WidgetId id;
        WidgetBounds bounds;

        [[nodiscard]] std::span<const WidgetId> children() const noexcept;
        [[nodiscard]] bool isDirectChild(WidgetId child) const noexcept;
        bool setChildBounds(WidgetId child, WidgetBounds childBounds);
        bool setChildVisible(WidgetId child, bool visible);
    };

    struct WidgetPaintContext {
        const WidgetTree &state;
        UIPainter &painter;
        WidgetId id;
        WidgetBounds bounds;
        PickingId pickingId = PickingId::invalid();
        bool enabled = true;
        bool hovered = false;
        bool focused = false;
    };

    struct UIEventReply {
        bool handled = false;
        bool stopPropagation = false;
        bool requestFocus = false;
        bool clearFocus = false;
        bool capturePointer = false;
        bool releasePointer = false;
        WidgetInvalidation invalidate = WidgetInvalidation::none;

        [[nodiscard]] static constexpr UIEventReply handledEvent() noexcept {
            return {.handled = true};
        }
    };

    struct WidgetEventContext {
        WidgetTree &state;
        WidgetId id;
        WidgetId target;
        UIEventPhase phase = UIEventPhase::target;
        WidgetBounds bounds;
        glm::vec2 pointerPosition{0.f, 0.f};
        bool hasPointerPosition = false;
        bool enabled = true;
        bool hovered = false;
        bool focused = false;

        [[nodiscard]] bool pointerInside() const noexcept {
            return hasPointerPosition && bounds.contains(pointerPosition);
        }

        [[nodiscard]] glm::vec2 localPointerPosition() const noexcept {
            return pointerPosition - bounds.topLeft();
        }
    };

    // Widget is intentionally a narrow behavior object. WidgetTree owns its
    // identity, hierarchy, layout node, focus/capture, and lifetime; the
    // painter owns renderer interaction. This keeps controls independently
    // testable and prevents per-widget GPU/device ownership.
    class BESS_API Widget {
      public:
        virtual ~Widget();

        [[nodiscard]] virtual std::string_view typeName() const noexcept;
        [[nodiscard]] virtual WidgetTraits traits() const noexcept;

        virtual void onMount(WidgetMountContext &context);
        virtual void onUnmount(WidgetTree &state, WidgetId id);
        virtual void updateLayout(WidgetLayoutContext &context);
        virtual void update(WidgetUpdateContext &context);
        virtual void arrange(WidgetArrangeContext &context);
        virtual void paint(WidgetPaintContext &context) const;
        // Painted after descendants while the widget's child clip is still
        // active. This is intended for adorners such as selection outlines,
        // drag previews, focus rings, and docking guides.
        virtual void paintOverlay(WidgetPaintContext &context) const;
        // Controls with retained overflow (menus, popovers, color pickers)
        // can extend their interactive shape beyond their layout box. Parent
        // clipping still governs whether descendants are considered.
        [[nodiscard]] virtual bool hitTest(WidgetBounds bounds,
                                           glm::vec2 position) const noexcept;
        virtual UIEventReply onEvent(WidgetEventContext &context,
                                     const UIEvent &event);
    };

} // namespace Bess::UI
