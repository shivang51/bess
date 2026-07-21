#pragma once

#include "dock.h"
#include "models/signal.h"
#include "models/tab_model.h"
#include "ui_types.h"

#include <optional>
#include <span>
#include <string>
#include <variant>
#include <vector>

namespace Bess::UI {

    enum class DockSplitAxis : uint8_t {
        horizontal, // left/right children
        vertical,   // top/bottom children
    };

    using DockItem = BasicTabItem<DockItemId>;
    using DockTabModel = BasicTabModel<DockItemId>;

    struct DockStackNode {
        DockNodeId id;
        DockNodeId parent;
        DockTabModel tabs;
    };

    struct DockSplitNode {
        DockNodeId id;
        DockNodeId parent;
        DockSplitAxis axis = DockSplitAxis::horizontal;
        float ratio = 0.5f;
        DockNodeId first;
        DockNodeId second;
    };

    enum class DockModelChangeKind : uint8_t {
        itemAdded,
        itemRemoved,
        itemMoved,
        itemActivated,
        itemUpdated,
        splitResized,
    };

    struct DockModelChange {
        DockModelChangeKind kind = DockModelChangeKind::itemAdded;
        DockItemId item;
        DockNodeId source;
        DockNodeId target;
    };

    class BESS_API DetachedDockItem {
      public:
        DetachedDockItem() = default;
        DetachedDockItem(const DetachedDockItem &) = delete;
        DetachedDockItem &operator=(const DetachedDockItem &) = delete;
        DetachedDockItem(DetachedDockItem &&) noexcept = default;
        DetachedDockItem &operator=(DetachedDockItem &&) noexcept = default;

        [[nodiscard]] explicit operator bool() const noexcept;
        [[nodiscard]] const DockItem *get() const noexcept;

      private:
        friend class DockSpaceModel;
        explicit DetachedDockItem(DockItem item);
        std::optional<DockItem> m_item;
    };

    struct DockStackLayout {
        DockNodeId node;
        WidgetBounds bounds;
        WidgetBounds tabBarBounds;
        WidgetBounds contentBounds;
        DockItemId activeItem;
    };

    struct DockSplitLayout {
        DockNodeId node;
        WidgetBounds bounds;
        WidgetBounds dividerBounds;
    };

    struct DockLayoutResult {
        std::vector<DockStackLayout> stacks;
        std::vector<DockSplitLayout> splits;

        [[nodiscard]] const DockStackLayout *
        findStack(DockNodeId id) const noexcept;
        [[nodiscard]] const DockSplitLayout *
        findSplit(DockNodeId id) const noexcept;
        [[nodiscard]] DockNodeId stackAt(glm::vec2 position) const noexcept;
        [[nodiscard]] DockNodeId dividerAt(glm::vec2 position) const noexcept;
    };

    // Pure dock topology/model. It owns no widgets or rendering resources and
    // can therefore be serialized, tested, and transferred independently.
    class BESS_API DockSpaceModel {
      public:
        using ChangedSignal = Signal<DockModelChange>;

        DockSpaceModel() = default;
        DockSpaceModel(const DockSpaceModel &) = delete;
        DockSpaceModel &operator=(const DockSpaceModel &) = delete;
        DockSpaceModel(DockSpaceModel &&) noexcept = default;
        DockSpaceModel &operator=(DockSpaceModel &&) noexcept = default;

        DockItemId addItem(DockItem item,
                           DockNodeId target = {},
                           DockZone zone = DockZone::main,
                           size_t tabIndex = DockTabModel::npos);
        bool attachItem(DetachedDockItem &&item,
                        DockNodeId target = {},
                        DockZone zone = DockZone::main,
                        size_t tabIndex = DockTabModel::npos);
        [[nodiscard]] DetachedDockItem detachItem(DockItemId item);
        bool removeItem(DockItemId item);
        bool moveItem(DockItemId item,
                      DockNodeId target,
                      DockZone zone = DockZone::main,
                      size_t tabIndex = DockTabModel::npos);

        bool activateItem(DockItemId item);
        bool setItemTitle(DockItemId item, std::string title);
        bool setSplitRatio(DockNodeId split, float ratio);

        [[nodiscard]] DockNodeId root() const noexcept;
        [[nodiscard]] DockNodeId firstStack() const noexcept;
        [[nodiscard]] const DockStackNode *
        getStack(DockNodeId id) const noexcept;
        [[nodiscard]] const DockSplitNode *
        getSplit(DockNodeId id) const noexcept;
        [[nodiscard]] DockNodeId parentOf(DockNodeId id) const noexcept;
        [[nodiscard]] DockNodeId stackForItem(DockItemId item) const noexcept;
        [[nodiscard]] const DockItem *getItem(DockItemId item) const noexcept;
        [[nodiscard]] DockItemId
        itemForContent(WidgetId content) const noexcept;
        [[nodiscard]] size_t nodeCount() const noexcept;
        [[nodiscard]] size_t itemCount() const noexcept;
        [[nodiscard]] size_t stackCount() const noexcept;
        [[nodiscard]] bool empty() const noexcept;

        [[nodiscard]] DockLayoutResult layout(WidgetBounds bounds,
                                              float tabBarHeight,
                                              float splitterThickness) const;
        [[nodiscard]] bool validate(std::string *reason = nullptr) const;

        [[nodiscard]] ChangedSignal &changed() noexcept;

      private:
        using Node = std::variant<DockStackNode, DockSplitNode>;

        DockNodeId createStack(DockItem item);
        [[nodiscard]] bool canDock(DockNodeId target,
                                   DockZone zone) const noexcept;
        DockItemId dockItemInternal(DockItem item,
                                    DockNodeId target,
                                    DockZone zone,
                                    size_t tabIndex);
        [[nodiscard]] std::optional<DockItem>
        detachItemInternal(DockItemId item, DockNodeId *source, bool collapse);
        void collapseEmptyStack(DockNodeId stack);
        bool replaceChild(DockNodeId parent,
                          DockNodeId oldChild,
                          DockNodeId newChild);
        void layoutNode(DockNodeId node,
                        WidgetBounds bounds,
                        float tabBarHeight,
                        float splitterThickness,
                        DockLayoutResult &result) const;
        [[nodiscard]] Node *getNode(DockNodeId id) noexcept;
        [[nodiscard]] const Node *getNode(DockNodeId id) const noexcept;

        NodeHashMap<DockNodeId, Node> m_nodes;
        HashMap<DockItemId, DockNodeId> m_itemLocations;
        DockNodeId m_root;
        ChangedSignal m_changed;
    };

} // namespace Bess::UI
