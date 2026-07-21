#pragma once

#include "behaviors/pressable.h"
#include "controls/dock_drop.h"
#include "models/dock_model.h"
#include "ui_style.h"
#include "widget.h"

#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace Bess::UI {

    struct DockPanelOptions {
        std::optional<UIBoxStyle> background;
        bool clipContent = true;
    };

    // Semantic wrapper for arbitrary dockable content. The DockSpace model
    // refers to this widget by WidgetId; its actual content remains an ordinary
    // child in WidgetTree and has no dependency on docking.
    class BESS_API DockPanel : public Widget {
      public:
        explicit DockPanel(std::string title, DockPanelOptions options = {});

        [[nodiscard]] std::string_view typeName() const noexcept override;
        [[nodiscard]] WidgetTraits traits() const noexcept override;
        void onMount(WidgetMountContext &context) override;
        void arrange(WidgetArrangeContext &context) override;
        void paint(WidgetPaintContext &context) const override;

        [[nodiscard]] DockItemId itemId() const noexcept;
        [[nodiscard]] const std::string &title() const noexcept;
        void setTitle(std::string title);

      private:
        DockItemId m_itemId = DockItemId::generate();
        std::string m_title;
        DockPanelOptions m_options;
    };

    struct DockPanelHandle {
        DockItemId item;
        WidgetId panel;

        [[nodiscard]] explicit operator bool() const noexcept {
            return item && panel;
        }
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
        [[nodiscard]] FloatingHost *findFloating(DockItemId item) noexcept;
        [[nodiscard]] const FloatingHost *
        findFloating(DockItemId item) const noexcept;
        [[nodiscard]] FloatingHost *findFloatingHost(DockHostId host) noexcept;
        [[nodiscard]] const FloatingHost *
        findFloatingHost(DockHostId host) const noexcept;
        [[nodiscard]] DockSpaceModel *modelForHost(DockHostId host) noexcept;
        [[nodiscard]] const DockSpaceModel *
        modelForHost(DockHostId host) const noexcept;
        [[nodiscard]] DockHostId
        floatingHeaderAt(glm::vec2 position,
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
        void removeFloatingHostIfEmpty(DockHostId host);
        void clearTabInteraction() noexcept;
        void bringFloatingToFront(WidgetEventContext &context, DockHostId host);
        UIEventReply beginFloatingHeaderPress(WidgetEventContext &context,
                                              DockHostId host);
        [[nodiscard]] DockDropGuideMetrics
        dropGuideMetrics(const WidgetTree &state) const noexcept;

        DockSpaceModel m_model;
        DockSpaceOptions m_options;
        WidgetTree *m_mountedState = nullptr;
        WidgetId m_mountedId;
        std::vector<std::unique_ptr<FloatingHost>> m_floatingHosts;
        std::optional<TabDrag> m_tabDrag;
        std::vector<DropGuide> m_dropGuides;
        std::optional<DropDestination> m_hoveredDrop;
        Pressable m_tabPressable;
        DockItemId m_hoveredItem;
        DockItemId m_pressedItem;
        DockHostId m_focusedHost;
        DockNodeId m_focusedStack;
        SplitHit m_hoveredSplit;
        SplitHit m_draggedSplit;
        DockSpaceModel::ChangedSignal::Connection m_modelConnection;
    };

} // namespace Bess::UI
