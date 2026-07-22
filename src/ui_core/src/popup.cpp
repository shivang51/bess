#include "popup.h"

#include "ui_view.h"
#include "widget_tree.h"

#include <algorithm>
#include <cmath>
#include <utility>

namespace Bess::UI {
    namespace {
        constexpr float kPopupContentDepth = 0.01f;

        float safeNonNegative(float value) noexcept {
            return std::isfinite(value) ? std::max(0.f, value) : 0.f;
        }

        glm::vec2 safeSize(glm::vec2 value) noexcept {
            return {safeNonNegative(value.x), safeNonNegative(value.y)};
        }

        WidgetBounds safeBounds(WidgetBounds value) noexcept {
            if (!std::isfinite(value.center.x) ||
                !std::isfinite(value.center.y)) {
                value.center = {};
            }
            value.size = safeSize(value.size);
            return value;
        }

        PopupSide opposite(PopupSide side) noexcept {
            switch (side) {
            case PopupSide::bottom:
                return PopupSide::top;
            case PopupSide::top:
                return PopupSide::bottom;
            case PopupSide::right:
                return PopupSide::left;
            case PopupSide::left:
                return PopupSide::right;
            }
            return PopupSide::bottom;
        }

        bool verticalSide(PopupSide side) noexcept {
            return side == PopupSide::bottom || side == PopupSide::top;
        }

        float availableOnSide(PopupSide side,
                              WidgetBounds viewport,
                              WidgetBounds anchor,
                              float gap) noexcept {
            switch (side) {
            case PopupSide::bottom:
                return viewport.bottomRight().y -
                       (anchor.bottomRight().y + gap);
            case PopupSide::top:
                return anchor.topLeft().y - gap - viewport.topLeft().y;
            case PopupSide::right:
                return viewport.bottomRight().x -
                       (anchor.bottomRight().x + gap);
            case PopupSide::left:
                return anchor.topLeft().x - gap - viewport.topLeft().x;
            }
            return 0.f;
        }

        float alignedStart(float anchorStart,
                           float anchorExtent,
                           float popupExtent,
                           PopupAlignment alignment) noexcept {
            switch (alignment) {
            case PopupAlignment::start:
                return anchorStart;
            case PopupAlignment::center:
                return anchorStart + (anchorExtent - popupExtent) * 0.5f;
            case PopupAlignment::end:
                return anchorStart + anchorExtent - popupExtent;
            }
            return anchorStart;
        }

        BoxPaint
        popupPaint(WidgetBounds bounds, const UIBoxStyle &style, PickingId id) {
            return {.bounds = bounds,
                    .color = style.background,
                    .borderColor = style.border,
                    .cornerRadius = style.cornerRadius,
                    .borderThickness = style.borderThickness,
                    .shadow = style.shadow,
                    .zIndex = 0.001f,
                    .pickingId = id};
        }
    } // namespace

    PopupAnchor PopupAnchor::forWidget(WidgetId id) noexcept {
        return {.widget = id};
    }

    PopupAnchor PopupAnchor::forBounds(WidgetBounds value) noexcept {
        return {.bounds = value};
    }

    PopupAnchor PopupAnchor::forPoint(glm::vec2 value) noexcept {
        return {.point = value};
    }

    PopupPlacementResult PopupPlacementSolver::calculate(
        WidgetBounds rawViewport,
        WidgetBounds rawAnchor,
        glm::vec2 rawDesiredSize,
        const AnchoredPopupOptions &options) noexcept {
        WidgetBounds viewport = safeBounds(rawViewport);
        const WidgetBounds anchor = safeBounds(rawAnchor);
        const float margin = std::min(
            safeNonNegative(options.viewportMargin),
            std::max(0.f, std::min(viewport.size.x, viewport.size.y) * 0.5f));
        viewport = viewport.inset(margin);

        glm::vec2 desired = safeSize(rawDesiredSize);
        glm::vec2 minimum = safeSize(options.minimumSize);
        minimum = glm::min(minimum, viewport.size);
        desired = glm::max(desired, minimum);
        if (std::isfinite(options.maximumSize.x) &&
            options.maximumSize.x > 0.f) {
            desired.x =
                std::min(desired.x, std::max(minimum.x, options.maximumSize.x));
        }
        if (std::isfinite(options.maximumSize.y) &&
            options.maximumSize.y > 0.f) {
            desired.y =
                std::min(desired.y, std::max(minimum.y, options.maximumSize.y));
        }
        if (options.matchAnchorWidth && verticalSide(options.preferredSide)) {
            desired.x = std::max(desired.x, anchor.size.x);
        }
        desired = glm::min(desired, viewport.size);

        PopupSide side = options.preferredSide;
        const float gap = safeNonNegative(options.gap);
        const float required = verticalSide(side) ? desired.y : desired.x;
        const float preferredSpace =
            std::max(0.f, availableOnSide(side, viewport, anchor, gap));
        const PopupSide alternative = opposite(side);
        const float alternativeSpace =
            std::max(0.f, availableOnSide(alternative, viewport, anchor, gap));
        if (options.allowFlip && preferredSpace < required &&
            alternativeSpace > preferredSpace) {
            side = alternative;
        }

        const float sideSpace =
            std::max(0.f, availableOnSide(side, viewport, anchor, gap));
        if (verticalSide(side)) {
            desired.y = std::min(desired.y, sideSpace);
        } else {
            desired.x = std::min(desired.x, sideSpace);
        }
        desired = glm::max(glm::min(desired, viewport.size),
                           glm::min(minimum, viewport.size));

        glm::vec2 topLeft;
        switch (side) {
        case PopupSide::bottom:
            topLeft = {alignedStart(anchor.topLeft().x,
                                    anchor.size.x,
                                    desired.x,
                                    options.alignment) +
                           options.offset.x,
                       anchor.bottomRight().y + gap + options.offset.y};
            break;
        case PopupSide::top:
            topLeft = {alignedStart(anchor.topLeft().x,
                                    anchor.size.x,
                                    desired.x,
                                    options.alignment) +
                           options.offset.x,
                       anchor.topLeft().y - gap - desired.y + options.offset.y};
            break;
        case PopupSide::right:
            topLeft = {anchor.bottomRight().x + gap + options.offset.x,
                       alignedStart(anchor.topLeft().y,
                                    anchor.size.y,
                                    desired.y,
                                    options.alignment) +
                           options.offset.y};
            break;
        case PopupSide::left:
            topLeft = {anchor.topLeft().x - gap - desired.x + options.offset.x,
                       alignedStart(anchor.topLeft().y,
                                    anchor.size.y,
                                    desired.y,
                                    options.alignment) +
                           options.offset.y};
            break;
        }

        const glm::vec2 minimumTopLeft = viewport.topLeft();
        const glm::vec2 maximumTopLeft =
            glm::max(minimumTopLeft, viewport.bottomRight() - desired);
        topLeft = glm::clamp(topLeft, minimumTopLeft, maximumTopLeft);
        return {
            .bounds = {.center = topLeft + desired * 0.5f, .size = desired},
            .side = side,
        };
    }

    PopupHandle::PopupHandle(std::weak_ptr<Detail::PopupHostControl> control,
                             PopupId id) noexcept
        : m_control(std::move(control)),
          m_id(id) {
    }

    PopupId PopupHandle::id() const noexcept {
        return m_id;
    }

    bool PopupHandle::isOpen() const noexcept {
        const auto control = m_control.lock();
        return control != nullptr && control->host != nullptr &&
               control->host->contains(m_id);
    }

    PopupHandle::operator bool() const noexcept {
        return isOpen();
    }

    bool PopupHandle::close() const {
        const auto control = m_control.lock();
        return control != nullptr && control->host != nullptr &&
               control->host->close(m_id);
    }

    WidgetRef<Widget> PopupHandle::layer() const noexcept {
        const auto control = m_control.lock();
        return control != nullptr && control->host != nullptr
                   ? control->host->layer(m_id)
                   : WidgetRef<Widget>{};
    }

    WidgetRef<FlexContainer> PopupHandle::content() const noexcept {
        const auto control = m_control.lock();
        return control != nullptr && control->host != nullptr
                   ? control->host->content(m_id)
                   : WidgetRef<FlexContainer>{};
    }

    AnchoredPopup::AnchoredPopup(AnchoredPopupOptions options,
                                 Dismissed dismissed)
        : m_options(std::move(options)),
          m_dismissed(std::move(dismissed)) {
    }

    std::string_view AnchoredPopup::typeName() const noexcept {
        return "AnchoredPopup";
    }

    WidgetTraits AnchoredPopup::traits() const noexcept {
        return {.focusable = false,
                .hitTestVisible =
                    m_options.interactive || m_options.dismissOnOutsidePress,
                .clipChildren = false};
    }

    void AnchoredPopup::onMount(WidgetMountContext &context) {
        context.layout.setWidthPercent(1.f);
        context.layout.setHeightPercent(1.f);
        m_state = &context.state;
        m_id = context.id;
        if (m_options.interactive) {
            static_cast<void>(
                m_state->activateFocusScope(m_id, m_options.focus));
        }
    }

    void AnchoredPopup::onUnmount(WidgetTree &state, WidgetId id) {
        static_cast<void>(state.deactivateFocusScope(id));
        m_state = nullptr;
        m_id = {};
    }

    void AnchoredPopup::arrange(WidgetArrangeContext &context) {
        const auto children = context.children();
        if (children.empty()) {
            m_popupBounds = {};
            return;
        }
        const WidgetId content = children.front();
        const auto placement = PopupPlacementSolver::calculate(
            context.bounds,
            resolveAnchor(context.state),
            context.state.getBounds(content).size,
            m_options);
        m_popupBounds = placement.bounds;
        m_resolvedSide = placement.side;
        static_cast<void>(context.setChildBounds(content, m_popupBounds));
        // The popup frame is painted by this full-viewport parent. Reserve a
        // local depth interval for all content, including plain Labels whose
        // own paint depth is zero, so the frame can never cover its children.
        static_cast<void>(context.setChildZOffset(content, kPopupContentDepth));
        static_cast<void>(
            context.setChildVisible(content, !m_popupBounds.empty()));
        for (size_t index = 1; index < children.size(); ++index) {
            static_cast<void>(context.setChildVisible(children[index], false));
        }
    }

    void AnchoredPopup::paint(WidgetPaintContext &context) const {
        if (m_popupBounds.empty()) {
            return;
        }
        const auto &style =
            m_options.style.value_or(context.state.theme().popup.panel);
        context.painter.drawBox(
            popupPaint(m_popupBounds, style, context.pickingId));
    }

    bool AnchoredPopup::hitTest(WidgetBounds bounds,
                                glm::vec2 position) const noexcept {
        if (!bounds.contains(position)) {
            return false;
        }
        return !m_options.passThroughAnchor || m_state == nullptr ||
               !resolveAnchor(*m_state).contains(position);
    }

    UIEventReply AnchoredPopup::onEvent(WidgetEventContext &context,
                                        const UIEvent &event) {
        if (!m_options.interactive || !m_dismissed) {
            return {};
        }
        if (const auto *key = event.getIf<Input::KeyEvent>();
            key != nullptr && m_options.dismissOnEscape &&
            context.phase == UIEventPhase::capture &&
            key->key == KeyCode::escape && key->action == KeyAction::press) {
            m_dismissed();
            return {.handled = true, .stopPropagation = true};
        }
        if (const auto *button = event.getIf<Input::MouseButtonEvent>();
            button != nullptr && m_options.dismissOnOutsidePress &&
            context.phase == UIEventPhase::target &&
            button->action == MouseButtonAction::press &&
            context.hasPointerPosition &&
            !m_popupBounds.contains(context.pointerPosition) &&
            (!m_options.passThroughAnchor || m_state == nullptr ||
             !resolveAnchor(*m_state).contains(context.pointerPosition))) {
            m_dismissed();
            return {.handled = true, .stopPropagation = true};
        }
        return {};
    }

    WidgetBounds AnchoredPopup::popupBounds() const noexcept {
        return m_popupBounds;
    }

    PopupSide AnchoredPopup::resolvedSide() const noexcept {
        return m_resolvedSide;
    }

    const AnchoredPopupOptions &AnchoredPopup::options() const noexcept {
        return m_options;
    }

    WidgetBounds AnchoredPopup::resolveAnchor(const WidgetTree &state) const {
        if (m_options.anchor.widget &&
            state.contains(m_options.anchor.widget)) {
            return state.getBounds(m_options.anchor.widget);
        }
        if (m_options.anchor.bounds.has_value()) {
            return safeBounds(*m_options.anchor.bounds);
        }
        const glm::vec2 point = m_options.anchor.point.value_or(glm::vec2{});
        return {.center = point, .size = {0.f, 0.f}};
    }

    class PopupHost::PopupView final : public UIView {
      public:
        PopupView(PopupHost &host,
                  PopupId id,
                  AnchoredPopupOptions options,
                  Builder build)
            : m_host(host),
              m_id(id),
              m_options(std::move(options)),
              m_build(std::move(build)) {
        }

        void compose(UIComposer &ui) override {
            m_layer = ui.emplace<AnchoredPopup>(
                m_options, [this] { static_cast<void>(m_host.close(m_id)); });
            UIComposer layerComposer{ui.tree(), m_layer.id()};
            m_content = layerComposer.emplace<FlexContainer>(m_options.content);
            UIComposer contentComposer{ui.tree(), m_content.id()};
            if (m_build) {
                m_build(contentComposer);
            }
        }

        void onUnmounting(UIViewContext &context) noexcept override {
            m_host.detached(m_id, context.id);
        }

        [[nodiscard]] WidgetId layerId() const noexcept {
            return m_layer.id();
        }

        [[nodiscard]] WidgetId contentId() const noexcept {
            return m_content.id();
        }

      private:
        PopupHost &m_host;
        PopupId m_id;
        AnchoredPopupOptions m_options;
        Builder m_build;
        WidgetRef<AnchoredPopup> m_layer;
        WidgetRef<FlexContainer> m_content;
    };

    PopupHost::PopupHost(UIViewHost &views)
        : m_views(views),
          m_control(std::make_shared<Detail::PopupHostControl>(
              Detail::PopupHostControl{.host = this})) {
        m_views.tree().setPopupHost(this);
    }

    PopupHost::~PopupHost() {
        clear();
        if (m_views.tree().popupHost() == this) {
            m_views.tree().setPopupHost(nullptr);
        }
        m_control->host = nullptr;
        m_control.reset();
    }

    PopupHandle PopupHost::open(AnchoredPopupOptions options, Builder build) {
        PopupId id;
        do {
            id = PopupId::generate();
        } while (!id || m_entries.contains(id));

        const WidgetId anchor = options.anchor.widget;
        const bool closeWhenAnchorGone = options.closeWhenAnchorGone;
        const bool dismissOnEscape = options.dismissOnEscape;
        const bool interactive = options.interactive;
        auto view = std::make_unique<PopupView>(
            *this, id, std::move(options), std::move(build));
        auto *raw = view.get();
        auto mounted = m_views.mountPopup(std::move(view));
        if (!mounted) {
            return {};
        }

        auto [it, inserted] =
            m_entries.emplace(id,
                              Entry{.view = mounted.id(),
                                    .layer = raw->layerId(),
                                    .content = raw->contentId(),
                                    .anchor = anchor,
                                    .closeWhenAnchorGone = closeWhenAnchorGone,
                                    .dismissOnEscape = dismissOnEscape,
                                    .interactive = interactive});
        if (!inserted) {
            static_cast<void>(mounted.unmount());
            return {};
        }
        m_order.push_back(id);
        return PopupHandle{m_control, id};
    }

    bool PopupHost::close(PopupId id) {
        const auto it = m_entries.find(id);
        if (it == m_entries.end()) {
            return false;
        }
        const ViewId view = it->second.view;
        m_entries.erase(it);
        m_order.erase(std::remove(m_order.begin(), m_order.end(), id),
                      m_order.end());
        static_cast<void>(m_views.unmount(view));
        return true;
    }

    bool PopupHost::closeTopmost() {
        return !m_order.empty() && close(m_order.back());
    }

    bool PopupHost::dismissTopmostOnEscape() {
        for (auto it = m_order.rbegin(); it != m_order.rend(); ++it) {
            const auto entry = m_entries.find(*it);
            if (entry == m_entries.end() || !entry->second.interactive) {
                continue;
            }
            return entry->second.dismissOnEscape && close(*it);
        }
        return false;
    }

    size_t PopupHost::closeAnchoredInSubtree(WidgetId subtree) {
        if (!subtree || m_clearing) {
            return 0;
        }

        const auto belongsToSubtree = [this, subtree](WidgetId widget) {
            while (widget) {
                if (widget == subtree) {
                    return true;
                }
                widget = m_views.tree().getParent(widget);
            }
            return false;
        };

        std::vector<PopupId> closing;
        closing.reserve(m_order.size());
        for (auto it = m_order.rbegin(); it != m_order.rend(); ++it) {
            const auto entry = m_entries.find(*it);
            if (entry != m_entries.end() && entry->second.closeWhenAnchorGone &&
                entry->second.anchor &&
                belongsToSubtree(entry->second.anchor)) {
                closing.push_back(*it);
            }
        }

        size_t count = 0;
        for (const auto id : closing) {
            count += close(id) ? 1u : 0u;
        }
        return count;
    }

    void PopupHost::clear() noexcept {
        if (m_clearing) {
            return;
        }
        m_clearing = true;
        while (!m_order.empty()) {
            static_cast<void>(close(m_order.back()));
        }
        m_entries.clear();
        m_clearing = false;
    }

    void PopupHost::update() {
        std::vector<PopupId> stale;
        for (const auto id : m_order) {
            const auto it = m_entries.find(id);
            if (it == m_entries.end()) {
                continue;
            }
            const auto &entry = it->second;
            if (!entry.view || m_views.getView(entry.view) == nullptr ||
                !m_views.tree().contains(entry.layer) ||
                (entry.closeWhenAnchorGone && entry.anchor &&
                 !m_views.tree().contains(entry.anchor))) {
                stale.push_back(id);
            }
        }
        for (const auto id : stale) {
            static_cast<void>(close(id));
        }
    }

    bool PopupHost::contains(PopupId id) const noexcept {
        return m_entries.contains(id);
    }

    size_t PopupHost::size() const noexcept {
        return m_entries.size();
    }

    PopupHandle PopupHost::topmost() const noexcept {
        return m_order.empty() ? PopupHandle{}
                               : PopupHandle{m_control, m_order.back()};
    }

    WidgetRef<Widget> PopupHost::layer(PopupId id) const noexcept {
        const auto it = m_entries.find(id);
        return it != m_entries.end()
                   ? WidgetRef<Widget>{m_views.tree(), it->second.layer}
                   : WidgetRef<Widget>{};
    }

    WidgetRef<FlexContainer> PopupHost::content(PopupId id) const noexcept {
        const auto it = m_entries.find(id);
        return it != m_entries.end()
                   ? WidgetRef<FlexContainer>{m_views.tree(),
                                              it->second.content}
                   : WidgetRef<FlexContainer>{};
    }

    void PopupHost::detached(PopupId id, ViewId view) noexcept {
        const auto it = m_entries.find(id);
        if (it == m_entries.end() || it->second.view != view) {
            return;
        }
        m_entries.erase(it);
        m_order.erase(std::remove(m_order.begin(), m_order.end(), id),
                      m_order.end());
    }

} // namespace Bess::UI
