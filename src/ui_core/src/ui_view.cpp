#include "ui_view.h"

#include "widget.h"

#include <algorithm>
#include <utility>

namespace Bess::UI {
    namespace {
        // Each mounted view owns a complete local depth interval. Widgets use
        // small positive Z offsets for their chrome, text and overlays; giving
        // every view a separate interval prevents content from a lower view
        // leaking through the background of a later overlay or popup.
        constexpr float kViewRootDepthStride = 16.f;

        class UIViewRoot final : public Widget {
          public:
            explicit UIViewRoot(UIViewLayer layer) : m_layer(layer) {
            }

            std::string_view typeName() const noexcept override {
                return "UIViewRoot";
            }

            WidgetTraits traits() const noexcept override {
                return {.focusable = false,
                        .hitTestVisible = m_layer == UIViewLayer::modal,
                        .clipChildren = true};
            }

            void onMount(WidgetMountContext &context) override {
                context.layout.setDirection(LayoutDirection::vertical);
                context.layout.setMainAxisAlignment(LayoutAlignment::start);
                context.layout.setCrossAxisAlignment(LayoutAlignment::start);
                context.layout.setWidthStretch();
                context.layout.setHeightStretch();
            }

            void arrange(WidgetArrangeContext &context) override {
                // Every mounted view owns a viewport-sized layer. A view may
                // choose its own internal layout, but its top-level content is
                // always given the complete target boundary.
                for (const auto child : context.children()) {
                    static_cast<void>(
                        context.setChildBounds(child, context.bounds));
                }
            }

          private:
            UIViewLayer m_layer;
        };

        constexpr uint8_t layerRank(UIViewLayer layer) noexcept {
            return static_cast<uint8_t>(layer);
        }
    } // namespace

    UIView::~UIView() = default;

    void UIView::onMounted(UIViewContext &) {
    }

    void UIView::onUnmounting(UIViewContext &) noexcept {
    }

    UIViewHost::UIViewHost(WidgetTree &tree)
        : m_tree(tree),
          m_control(std::make_shared<Detail::UIViewHostControl>(
              Detail::UIViewHostControl{.host = this})) {
    }

    UIViewHost::~UIViewHost() {
        clear();
        m_control->host = nullptr;
        m_control.reset();
    }

    UIViewRef<UIView> UIViewHost::setContent(std::unique_ptr<UIView> view) {
        const ViewId previous = m_content;
        auto mounted = mountOwned(std::move(view), UIViewLayer::content);
        if (!mounted) {
            return {};
        }

        m_content = mounted.id();
        if (previous && previous != m_content) {
            static_cast<void>(unmount(previous));
        }
        return mounted;
    }

    UIViewRef<UIView> UIViewHost::mountOverlay(std::unique_ptr<UIView> view) {
        return mountOwned(std::move(view), UIViewLayer::overlay);
    }

    UIViewRef<UIView> UIViewHost::mountModal(std::unique_ptr<UIView> view) {
        return mountOwned(std::move(view), UIViewLayer::modal);
    }

    UIViewRef<UIView> UIViewHost::mountPopup(std::unique_ptr<UIView> view) {
        return mountOwned(std::move(view), UIViewLayer::popup);
    }

    UIViewRef<UIView> UIViewHost::mountOwned(std::unique_ptr<UIView> view,
                                             UIViewLayer layer) {
        if (view == nullptr) {
            return {};
        }

        ViewId id;
        do {
            id = ViewId::generate();
        } while (!id || m_entries.contains(id));

        const size_t orderIndex = orderIndexFor(layer);
        const size_t rootIndex = rootIndexFor(orderIndex);
        const WidgetId root = m_tree.addWidget(
            std::make_unique<UIViewRoot>(layer), {}, rootIndex);
        if (!root) {
            return {};
        }

        try {
            UIComposer composer{m_tree, root};
            view->compose(composer);
        } catch (...) {
            static_cast<void>(m_tree.removeWidget(root));
            if (m_tree.contains(root)) {
                m_pendingUnmounts.push_back(Entry{.view = std::move(view),
                                                  .root = root,
                                                  .layer = layer,
                                                  .unmounting = true});
            }
            throw;
        }

        auto [entryIt, inserted] = m_entries.emplace(
            id, Entry{.view = std::move(view), .root = root, .layer = layer});
        if (!inserted) {
            static_cast<void>(m_tree.removeWidget(root));
            return {};
        }
        m_order.insert(m_order.begin() + static_cast<ptrdiff_t>(orderIndex),
                       id);
        refreshRootDepths();

        if (layer == UIViewLayer::modal) {
            static_cast<void>(m_tree.activateFocusScope(
                root,
                {.trapFocus = true, .autoFocus = true, .restoreFocus = true}));
        }

        try {
            UIViewContext context{
                .host = *this,
                .tree = m_tree,
                .id = id,
                .root = WidgetRef<Widget>{m_tree, root},
            };
            entryIt->second.view->onMounted(context);
        } catch (...) {
            rollbackMount(id, root);
            throw;
        }

        // onMounted is permitted to synchronously unmount the view.
        return m_entries.contains(id) ? UIViewRef<UIView>{m_control, id}
                                      : UIViewRef<UIView>{};
    }

    bool UIViewHost::unmount(ViewId id) {
        auto it = m_entries.find(id);
        if (it == m_entries.end() || it->second.unmounting) {
            return false;
        }

        it->second.unmounting = true;
        UIViewContext context{
            .host = *this,
            .tree = m_tree,
            .id = id,
            .root = WidgetRef<Widget>{m_tree, it->second.root},
        };
        it->second.view->onUnmounting(context);

        const WidgetId root = it->second.root;
        Entry entry = std::move(it->second);
        m_entries.erase(it);
        eraseOrder(id);
        refreshRootDepths();
        if (m_content == id) {
            m_content = {};
        }

        static_cast<void>(m_tree.removeWidget(root));
        if (m_tree.contains(root)) {
            // WidgetTree deferred removal because one of its callbacks is
            // active. Keep callback captures in the view alive until the
            // subtree has actually disappeared.
            m_pendingUnmounts.push_back(std::move(entry));
        }
        return true;
    }

    void UIViewHost::clear() noexcept {
        const auto order = m_order;
        for (auto it = order.rbegin(); it != order.rend(); ++it) {
            static_cast<void>(unmount(*it));
        }

        // Defensive cleanup for entries whose order metadata was damaged.
        while (!m_entries.empty()) {
            static_cast<void>(unmount(m_entries.begin()->first));
        }
        flushPendingUnmounts();
    }

    void UIViewHost::flushPendingUnmounts() noexcept {
        std::erase_if(m_pendingUnmounts, [this](const Entry &entry) {
            return !m_tree.contains(entry.root);
        });

        // Reconcile direct low-level removal/clear through getWidgetTree().
        std::vector<ViewId> detached;
        detached.reserve(m_entries.size());
        for (const auto &[id, entry] : m_entries) {
            if (!m_tree.contains(entry.root)) {
                detached.push_back(id);
            }
        }
        for (const auto id : detached) {
            static_cast<void>(unmount(id));
        }
    }

    UIView *UIViewHost::getView(ViewId id) noexcept {
        const auto it = m_entries.find(id);
        return it != m_entries.end() ? it->second.view.get() : nullptr;
    }

    const UIView *UIViewHost::getView(ViewId id) const noexcept {
        const auto it = m_entries.find(id);
        return it != m_entries.end() ? it->second.view.get() : nullptr;
    }

    WidgetId UIViewHost::rootOf(ViewId id) const noexcept {
        const auto it = m_entries.find(id);
        return it != m_entries.end() ? it->second.root : WidgetId{};
    }

    std::optional<UIViewLayer> UIViewHost::layerOf(ViewId id) const noexcept {
        const auto it = m_entries.find(id);
        return it != m_entries.end()
                   ? std::optional<UIViewLayer>{it->second.layer}
                   : std::nullopt;
    }

    ViewId UIViewHost::content() const noexcept {
        return m_content;
    }

    size_t UIViewHost::size() const noexcept {
        return m_entries.size();
    }

    WidgetTree &UIViewHost::tree() const noexcept {
        return m_tree;
    }

    size_t UIViewHost::orderIndexFor(UIViewLayer layer) const noexcept {
        const auto it = std::find_if(
            m_order.begin(), m_order.end(), [this, layer](ViewId id) {
                const auto entry = m_entries.find(id);
                return entry != m_entries.end() &&
                       layerRank(entry->second.layer) > layerRank(layer);
            });
        return static_cast<size_t>(it - m_order.begin());
    }

    size_t UIViewHost::rootIndexFor(size_t orderIndex) const noexcept {
        if (orderIndex >= m_order.size()) {
            return WidgetTree::append;
        }
        const WidgetId before = rootOf(m_order[orderIndex]);
        const auto roots = m_tree.getRoots();
        const auto it = std::find(roots.begin(), roots.end(), before);
        return it != roots.end() ? static_cast<size_t>(it - roots.begin())
                                 : WidgetTree::append;
    }

    void UIViewHost::refreshRootDepths() noexcept {
        for (size_t index = 0; index < m_order.size(); ++index) {
            const auto entry = m_entries.find(m_order[index]);
            if (entry == m_entries.end()) {
                continue;
            }
            auto *layout = m_tree.getLayout(entry->second.root);
            if (layout == nullptr) {
                continue;
            }

            // The layer rank leaves the corresponding empty intervals in
            // front of unmanaged/content roots even when this host currently
            // contains only a popup. The ordered index then makes every view
            // in the same layer strictly front-to-back deterministic.
            const float depth =
                static_cast<float>(index + layerRank(entry->second.layer)) *
                kViewRootDepthStride;
            layout->setZVal(depth);
        }
    }

    void UIViewHost::eraseOrder(ViewId id) noexcept {
        const auto it = std::find(m_order.begin(), m_order.end(), id);
        if (it != m_order.end()) {
            m_order.erase(it);
        }
    }

    void UIViewHost::rollbackMount(ViewId id, WidgetId root) noexcept {
        eraseOrder(id);
        auto it = m_entries.find(id);
        if (it == m_entries.end()) {
            static_cast<void>(m_tree.removeWidget(root));
            return;
        }
        Entry entry = std::move(it->second);
        m_entries.erase(it);
        refreshRootDepths();
        static_cast<void>(m_tree.removeWidget(root));
        if (m_tree.contains(root)) {
            m_pendingUnmounts.push_back(std::move(entry));
        }
    }

} // namespace Bess::UI
