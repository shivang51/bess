#pragma once

#include "behaviors/pressable.h"
#include "controls/dock_drop.h"
#include "controls/scroll_view.h"
#include "models/dock_model.h"
#include "ui_style.h"
#include "widget.h"
#include "widget_ref.h"

#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace Bess::UI {

    class DockPanelHandle;
    struct TabStripRegion;

    struct DockPanelOptions {
        std::optional<UIBoxStyle> background;
        bool clipContent = true;
    };

    // Semantic wrapper for arbitrary dockable content. The DockSpace model
    // refers to this widget by WidgetId; its actual content remains an ordinary
    // child in WidgetTree and has no dependency on docking.
    class BESS_API DockPanel : public ScrollView {
      public:
        explicit DockPanel(std::string title, DockPanelOptions options = {});

        [[nodiscard]] std::string_view typeName() const noexcept override;
        void onMount(WidgetMountContext &context) override;
        void paint(WidgetPaintContext &context) const override;

        [[nodiscard]] DockItemId itemId() const noexcept;
        [[nodiscard]] const std::string &title() const noexcept;
        void setTitle(std::string title);

      private:
        DockItemId m_itemId = DockItemId::generate();
        std::string m_title;
        DockPanelOptions m_options;
    };

    struct DockSpaceOptions {
        std::optional<UIDockStyle> dockStyle;
        std::optional<UITabStyle> tabStyle;
    };

    class BESS_API DockSpace : public Widget {
      public:
        explicit DockSpace(DockSpaceOptions options = {});

        [[nodiscard]] std::string_view typeName() const noexcept override;
        [[nodiscard]] WidgetTraits traits() const noexcept override;
        void onMount(WidgetMountContext &context) override;
        void onUnmount(WidgetTree &state, WidgetId id) override;
        void arrange(WidgetArrangeContext &context) override;
        void paint(WidgetPaintContext &context) const override;
        void paintOverlay(WidgetPaintContext &context) const override;
        [[nodiscard]] CursorIcon
        cursor(const WidgetCursorContext &context) const noexcept override;
        UIEventReply onEvent(WidgetEventContext &context,
                             const UIEvent &event) override;

        [[nodiscard]] DockSpaceModel &model() noexcept;
        [[nodiscard]] const DockSpaceModel &model() const noexcept;

        DockPanelHandle createPanel(WidgetTree &state,
                                    WidgetId dockSpace,
                                    std::string title,
                                    std::unique_ptr<Widget> content = {},
                                    DockNodeId target = {},
                                    DockZone zone = DockZone::main,
                                    bool closable = true);
        bool removePanel(WidgetTree &state, DockItemId item);
        bool
        setPanelTitle(WidgetTree &state, DockItemId item, std::string title);
        bool hidePanel(DockItemId item);
        bool showPanel(DockItemId item);
        [[nodiscard]] bool isPanelVisible(DockItemId item) const noexcept;
        [[nodiscard]] bool isPanelHidden(DockItemId item) const noexcept;
        [[nodiscard]] size_t hiddenPanelCount() const noexcept;

        // Floating panels remain owned by this DockSpace and its WidgetTree.
        // Each floating window is another dock host, so it can receive tabs
        // and side splits through the same model operations as the main host.
        bool floatItem(DockItemId item, WidgetBounds bounds);
        bool dockFloatingItem(DockItemId item,
                              DockNodeId target = {},
                              DockZone zone = DockZone::main,
                              size_t tabIndex = DockTabModel::npos);
        [[nodiscard]] bool isItemFloating(DockItemId item) const noexcept;
        [[nodiscard]] size_t floatingItemCount() const noexcept;
        [[nodiscard]] size_t floatingWindowCount() const noexcept;
        [[nodiscard]] std::optional<WidgetBounds>
        floatingItemBounds(DockItemId item) const noexcept;

      private:
        struct HitTab {
            DockHostId host;
            DockNodeId stack;
            DockItemId item;

            bool operator==(const HitTab &) const noexcept = default;
        };

        struct HiddenPanel {
            DetachedDockItem item;
            DockHostId originHost;
            DockNodeId stack;
            size_t tabIndex = DockTabModel::npos;
            WidgetBounds fallbackBounds;
        };

        struct StackOwner {
            DockSpaceModel *model = nullptr;
            DockHostId host;
        };

        struct FloatingHost {
            DockHostId id = DockHostId::generate();
            DockSpaceModel model;
            WidgetBounds bounds;
        };

        struct TabDrag {
            DockItemId item;
            DockHostId host;
            glm::vec2 pressPosition{0.f};
            glm::vec2 grabOffset{0.f};
            bool window = false;
            bool started = false;
        };

        struct SplitHit {
            DockHostId host;
            DockNodeId node;

            bool operator==(const SplitHit &) const noexcept = default;
        };

        struct FloatingResizeEdges {
            bool left = false;
            bool right = false;
            bool top = false;
            bool bottom = false;

            [[nodiscard]] bool any() const noexcept {
                return left || right || top || bottom;
            }

            bool
            operator==(const FloatingResizeEdges &) const noexcept = default;
        };

        struct FloatingResizeHit {
            DockHostId host;
            FloatingResizeEdges edges;

            [[nodiscard]] explicit operator bool() const noexcept {
                return host && edges.any();
            }

            bool operator==(const FloatingResizeHit &) const noexcept = default;
        };

        struct FloatingResizeDrag {
            FloatingResizeHit hit;
            WidgetBounds initialBounds;
            glm::vec2 pressPosition{0.f, 0.f};
        };

        struct DropGuide {
            DockHostId host;
            bool root = false;
            DockDropGuideLayout layout;
        };

        struct DropDestination {
            DockHostId host;
            DockNodeId node;
            DockZone zone = DockZone::main;
            bool root = false;
            size_t tabIndex = DockTabModel::npos;

            bool operator==(const DropDestination &) const noexcept = default;
        };

        [[nodiscard]] const UIDockStyle &
        dockStyle(const WidgetTree &state) const;
        [[nodiscard]] const UITabStyle &tabStyle(const WidgetTree &state) const;
        [[nodiscard]] DockLayoutResult
        calculateLayout(WidgetBounds bounds, const WidgetTree &state) const;
        [[nodiscard]] DockLayoutResult
        calculateLayout(const DockSpaceModel &model,
                        WidgetBounds bounds,
                        const WidgetTree &state,
                        bool hideSingleTab = false) const;
        [[nodiscard]] DockLayoutResult
        calculateFloatingLayout(const FloatingHost &host,
                                const WidgetTree &state) const;
        [[nodiscard]] HitTab hitTab(WidgetBounds bounds,
                                    const WidgetTree &state,
                                    glm::vec2 position) const;
        [[nodiscard]] HitTab hitTab(const DockSpaceModel &model,
                                    DockHostId host,
                                    const DockLayoutResult &layout,
                                    const WidgetTree &state,
                                    glm::vec2 position) const;
        [[nodiscard]] HitTab hitClose(WidgetBounds bounds,
                                      const WidgetTree &state,
                                      glm::vec2 position) const;
        [[nodiscard]] HitTab hitClose(const DockSpaceModel &model,
                                      DockHostId host,
                                      const DockLayoutResult &layout,
                                      const WidgetTree &state,
                                      glm::vec2 position) const;
        [[nodiscard]] TabStripRegion
        floatingHeaderRegion(const FloatingHost &host,
                             const WidgetTree &state,
                             bool reserveClose) const noexcept;
        [[nodiscard]] FloatingHost *findFloating(DockItemId item) noexcept;
        [[nodiscard]] const FloatingHost *
        findFloating(DockItemId item) const noexcept;
        [[nodiscard]] FloatingHost *findFloatingHost(DockHostId host) noexcept;
        [[nodiscard]] const FloatingHost *
        findFloatingHost(DockHostId host) const noexcept;
        [[nodiscard]] DockSpaceModel *modelForHost(DockHostId host) noexcept;
        [[nodiscard]] const DockSpaceModel *
        modelForHost(DockHostId host) const noexcept;
        [[nodiscard]] StackOwner findStackOwner(DockNodeId stack) noexcept;
        [[nodiscard]] DockHostId
        floatingHeaderAt(glm::vec2 position,
                         const WidgetTree &state) const noexcept;
        [[nodiscard]] FloatingResizeHit
        floatingResizeAt(glm::vec2 position,
                         const WidgetTree &state) const noexcept;
        [[nodiscard]] DockItemId
        floatingTitleItem(const FloatingHost &host) const noexcept;
        [[nodiscard]] WidgetBounds
        floatingHeaderBounds(const FloatingHost &host,
                             const WidgetTree &state) const noexcept;
        [[nodiscard]] WidgetBounds
        floatingClientBounds(const FloatingHost &host,
                             const WidgetTree &state) const noexcept;
        [[nodiscard]] WidgetBounds
        normalizedFloatingBounds(WidgetBounds requested,
                                 WidgetBounds dockBounds,
                                 const WidgetTree &state) const noexcept;
        bool beginTabDrag(WidgetEventContext &context, WidgetBounds dockBounds);
        void updateFloatingDrag(WidgetEventContext &context);
        void updateFloatingResize(WidgetEventContext &context);
        bool finishFloatingDrag();
        void refreshDropGuides(WidgetBounds bounds,
                               const WidgetTree &state,
                               glm::vec2 position);
        bool transferItem(DockItemId item,
                          DockHostId source,
                          const DropDestination &destination);
        bool transferHost(DockHostId source,
                          const DropDestination &destination);
        bool attachDetached(DockSpaceModel &destination,
                            DetachedDockItem &&item,
                            const DropDestination &drop,
                            DockNodeId overrideTarget = {});
        bool restoreHiddenAsFloating(HiddenPanel &hidden);
        [[nodiscard]] WidgetBounds
        dockedFallbackBounds(DockNodeId stack, const WidgetTree &state) const;
        void removeFloatingHostIfEmpty(DockHostId host);
        void clearTabInteraction() noexcept;
        void clearCloseInteraction() noexcept;
        void clearResizeInteraction() noexcept;
        void bringFloatingToFront(WidgetEventContext &context, DockHostId host);
        UIEventReply beginFloatingResize(WidgetEventContext &context,
                                         FloatingResizeHit hit);
        UIEventReply beginFloatingHeaderPress(WidgetEventContext &context,
                                              DockHostId host);
        [[nodiscard]] DockDropGuideMetrics
        dropGuideMetrics(const WidgetTree &state) const noexcept;

        DockSpaceModel m_model;
        DockSpaceOptions m_options;
        WidgetTree *m_mountedState = nullptr;
        WidgetId m_mountedId;
        std::vector<std::unique_ptr<FloatingHost>> m_floatingHosts;
        HashMap<DockItemId, HiddenPanel> m_hiddenPanels;
        std::optional<TabDrag> m_tabDrag;
        std::vector<DropGuide> m_dropGuides;
        std::optional<DropDestination> m_hoveredDrop;
        Pressable m_tabPressable;
        DockItemId m_hoveredItem;
        DockItemId m_pressedItem;
        HitTab m_hoveredClose;
        HitTab m_pressedClose;
        DockHostId m_focusedHost;
        DockNodeId m_focusedStack;
        SplitHit m_hoveredSplit;
        SplitHit m_draggedSplit;
        FloatingResizeHit m_hoveredResize;
        std::optional<FloatingResizeDrag> m_resizeDrag;
        DockSpaceModel::ChangedSignal::Connection m_modelConnection;
    };

    // Copyable, lifetime-safe reference to a retained dock panel. Hiding only
    // detaches its DockItem; the panel widget and all composed child state stay
    // alive until removePanel() or WidgetTree teardown.
    class BESS_API DockPanelHandle {
      public:
        DockPanelHandle() = default;

        DockItemId item;
        WidgetId panel;

        [[nodiscard]] explicit operator bool() const noexcept;
        [[nodiscard]] WidgetRef<DockPanel> widget() const noexcept;
        bool hide() const;
        bool show() const;
        [[nodiscard]] bool isVisible() const noexcept;
        [[nodiscard]] bool isHidden() const noexcept;

      private:
        friend class DockSpace;
        DockPanelHandle(WidgetTree &state,
                        WidgetId dockSpace,
                        DockItemId item,
                        WidgetId panel) noexcept;

        WidgetRef<DockSpace> m_dockSpace;
    };

} // namespace Bess::UI
