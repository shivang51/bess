#include "ui_composer.h"

#include <cmath>
#include <utility>

namespace Bess::UI {

    ReorderListComposer::ReorderListComposer(WidgetTree &tree,
                                             WidgetRef<ReorderableList> list)
        : m_tree(tree),
          m_list(std::move(list)) {
        if (m_list.tree() != &m_tree || !m_list) {
            throw std::invalid_argument(
                "ReorderListComposer requires a live ReorderableList in its "
                "WidgetTree");
        }
    }

    WidgetRef<ReorderableList> ReorderListComposer::widget() const noexcept {
        return m_list;
    }

    ReorderListId ReorderListComposer::listId() const noexcept {
        const auto *list = m_list.get();
        return list != nullptr ? list->listId() : ReorderListId{};
    }

    ReorderListItemHandle
    ReorderListComposer::item(ReorderItemId id,
                              DraggableListItemOptions options) {
        if (!m_list ||
            m_tree.getWidget<ReorderableList>(m_list.id()) == nullptr) {
            throw std::logic_error(
                "Cannot add an item to an unmounted ReorderableList");
        }
        if (id && containsItem(id)) {
            throw std::invalid_argument(
                "ReorderableList item IDs must be unique within the list");
        }
        if (!id) {
            do {
                id = ReorderItemId::generate();
            } while (containsItem(id));
        }

        const WidgetId widget = m_tree.addWidget(
            std::make_unique<DraggableListItem>(
                m_tree.dragDrop(), listId(), id, std::move(options)),
            m_list.id());
        if (!widget) {
            throw std::runtime_error(
                "Failed to compose a ReorderableList item");
        }
        return {
            .id = id,
            .widget = WidgetRef<DraggableListItem>{m_tree, widget},
        };
    }

    ReorderListItemHandle
    ReorderListComposer::item(DraggableListItemOptions options) {
        return item({}, std::move(options));
    }

    bool ReorderListComposer::containsItem(ReorderItemId id) const noexcept {
        if (!id || !m_list) {
            return false;
        }
        for (const auto child : m_tree.getChildren(m_list.id())) {
            const auto *item = m_tree.getWidget<DraggableListItem>(child);
            if (item != nullptr && item->itemId() == id) {
                return true;
            }
        }
        return false;
    }

    DockComposer::DockComposer(WidgetTree &tree, WidgetRef<DockSpace> dockSpace)
        : m_tree(tree),
          m_dockSpace(std::move(dockSpace)) {
        if (m_dockSpace.tree() != &m_tree || !m_dockSpace) {
            throw std::invalid_argument(
                "DockComposer requires a live DockSpace in its WidgetTree");
        }
    }

    WidgetRef<DockSpace> DockComposer::widget() const noexcept {
        return m_dockSpace;
    }

    DockSpaceModel &DockComposer::model() {
        auto *dock = m_dockSpace.get();
        if (dock == nullptr) {
            throw std::logic_error("DockSpace is no longer mounted");
        }
        return dock->model();
    }

    const DockSpaceModel &DockComposer::model() const {
        auto *dock = m_dockSpace.get();
        if (dock == nullptr) {
            throw std::logic_error("DockSpace is no longer mounted");
        }
        return dock->model();
    }

    DockNodeId DockComposer::stackFor(DockItemId item) const noexcept {
        auto *dock = m_dockSpace.get();
        return dock != nullptr ? dock->model().stackForItem(item)
                               : DockNodeId{};
    }

    DockPanelHandle DockComposer::panel(std::string title,
                                        DockPanelPlacement placement) {
        auto *dock = m_dockSpace.get();
        if (dock == nullptr) {
            return {};
        }
        return dock->createPanel(
            m_tree,
            m_dockSpace.id(),
            std::move(title),
            std::make_unique<FlexContainer>(std::move(placement.content)),
            placement.target,
            placement.zone,
            placement.closable);
    }

    WidgetId
    DockComposer::contentRoot(const DockPanelHandle &panel) const noexcept {
        if (!panel || m_tree.getParent(panel.panel) != m_dockSpace.id()) {
            return {};
        }
        const auto children = m_tree.getChildren(panel.panel);
        return children.size() == 1 ? children.front() : WidgetId{};
    }

    void DockComposer::rollback(const DockPanelHandle &panel) noexcept {
        try {
            if (auto *dock = m_dockSpace.get(); dock != nullptr) {
                static_cast<void>(dock->removePanel(m_tree, panel.item));
            } else if (panel.panel) {
                static_cast<void>(m_tree.removeWidget(panel.panel));
            }
        } catch (...) {
            // Rollback is cleanup for an already-failing composition. Widget
            // removal is structurally total even when an unmount callback
            // throws, so preserve the builder's primary exception.
        }
    }

    UIComposer::UIComposer(WidgetTree &tree, WidgetId parent)
        : m_tree(tree),
          m_parent(parent) {
        if (m_parent && !m_tree.contains(m_parent)) {
            throw std::invalid_argument(
                "UIComposer parent does not belong to its WidgetTree");
        }
    }

    WidgetTree &UIComposer::tree() const noexcept {
        return m_tree;
    }

    WidgetId UIComposer::parent() const noexcept {
        return m_parent;
    }

    WidgetRef<FlexContainer> UIComposer::row(FlexContainerOptions options) {
        options.direction = LayoutDirection::horizontal;
        return emplace<FlexContainer>(std::move(options));
    }

    WidgetRef<FlexContainer> UIComposer::column(FlexContainerOptions options) {
        options.direction = LayoutDirection::vertical;
        return emplace<FlexContainer>(std::move(options));
    }

    WidgetRef<StackContainer> UIComposer::stack(StackContainerOptions options) {
        return emplace<StackContainer>(std::move(options));
    }

    WidgetRef<FocusScope> UIComposer::focusScope(FocusScopeOptions options) {
        return emplace<FocusScope>(std::move(options));
    }

    WidgetRef<Surface> UIComposer::surface(SurfaceOptions options) {
        return emplace<Surface>(std::move(options));
    }

    WidgetRef<Card> UIComposer::card(CardOptions options) {
        return emplace<Card>(std::move(options));
    }

    WidgetRef<ListView> UIComposer::listView(ListViewOptions options) {
        return emplace<ListView>(std::move(options));
    }

    WidgetRef<DropZone> UIComposer::dropZone(DropZoneOptions options) {
        return emplace<DropZone>(m_tree.dragDrop(), std::move(options));
    }

    WidgetRef<Draggable> UIComposer::draggable(DraggableOptions options) {
        return emplace<Draggable>(m_tree.dragDrop(), std::move(options));
    }

    WidgetRef<ReorderableList>
    UIComposer::reorderableList(ReorderableListOptions options,
                                ReorderListId id) {
        return emplace<ReorderableList>(
            m_tree.dragDrop(), std::move(options), id);
    }

    WidgetRef<ReorderableList> UIComposer::reorderableList(ReorderListId id) {
        return reorderableList(ReorderableListOptions{}, id);
    }

    WidgetRef<Image>
    UIComposer::image(std::shared_ptr<Core::Renderer::ITexture> texture,
                      ImageOptions options) {
        return emplace<Image>(std::move(texture), std::move(options));
    }

    WidgetRef<Image>
    UIComposer::dynamicImage(ImageTextureProvider textureProvider,
                             ImageOptions options) {
        return emplace<Image>(std::move(textureProvider), std::move(options));
    }

    WidgetRef<RenderView>
    UIComposer::renderView(std::shared_ptr<IRenderViewDelegate> delegate,
                           RenderViewOptions options,
                           std::shared_ptr<RenderSurface> surface) {
        return emplace<RenderView>(
            std::move(delegate), std::move(options), std::move(surface));
    }

    WidgetRef<SceneView>
    UIComposer::sceneView(std::shared_ptr<ISceneViewDelegate> delegate,
                          SceneViewOptions options) {
        return emplace<SceneView>(std::move(delegate), std::move(options));
    }

    WidgetRef<TreeNode>
    UIComposer::treeNode(std::string label,
                         TreeNodeOptions options,
                         TreeNode::ExpandedChanged expandedChanged) {
        return emplace<TreeNode>(
            std::move(label), std::move(options), std::move(expandedChanged));
    }

    WidgetRef<Label> UIComposer::label(std::string text, LabelOptions options) {
        return emplace<Label>(std::move(text), std::move(options));
    }

    WidgetRef<Button> UIComposer::button(std::string label,
                                         Button::Activated activated,
                                         ButtonOptions options) {
        return emplace<Button>(
            std::move(label), std::move(activated), std::move(options));
    }

    WidgetRef<Button> UIComposer::textButton(std::string label,
                                             Button::Activated activated,
                                             ButtonOptions options) {
        options.variant = ButtonVariant::text;
        return emplace<Button>(
            std::move(label), std::move(activated), std::move(options));
    }

    WidgetRef<ActionButton>
    UIComposer::actionButton(ActionId action, ActionButtonOptions options) {
        return emplace<ActionButton>(
            m_tree.actionRegistry(), std::move(action), std::move(options));
    }

    WidgetRef<CheckBox>
    UIComposer::checkBox(std::string label,
                         std::shared_ptr<CheckStateModel> model,
                         CheckBox::Changed changed,
                         CheckBoxOptions options) {
        return emplace<CheckBox>(std::move(label),
                                 std::move(model),
                                 std::move(changed),
                                 std::move(options));
    }

    WidgetRef<ToggleSwitch> UIComposer::toggle(std::string label,
                                               std::shared_ptr<BoolModel> model,
                                               ToggleSwitch::Changed changed,
                                               ToggleSwitchOptions options) {
        return emplace<ToggleSwitch>(std::move(label),
                                     std::move(model),
                                     std::move(changed),
                                     std::move(options));
    }

    WidgetRef<RadioButton>
    UIComposer::radio(std::string label,
                      std::shared_ptr<RadioGroupModel> group,
                      RadioId id,
                      RadioButton::Selected selected,
                      RadioButtonOptions options) {
        return emplace<RadioButton>(std::move(label),
                                    std::move(group),
                                    id,
                                    std::move(selected),
                                    std::move(options));
    }

    WidgetRef<Slider> UIComposer::slider(std::shared_ptr<RangeModel> model,
                                         Slider::Changed changed,
                                         SliderOptions options) {
        return emplace<Slider>(
            std::move(model), std::move(changed), std::move(options));
    }

    WidgetRef<Dropdown>
    UIComposer::dropdown(std::shared_ptr<DropdownModel> model,
                         Dropdown::Changed changed,
                         DropdownOptions options) {
        return emplace<Dropdown>(
            std::move(model), std::move(changed), std::move(options));
    }

    WidgetRef<TextBox> UIComposer::textBox(std::shared_ptr<TextEditModel> model,
                                           TextBox::Changed changed,
                                           TextBox::Submitted submitted,
                                           TextBoxOptions options) {
        return emplace<TextBox>(std::move(model),
                                std::move(changed),
                                std::move(submitted),
                                std::move(options));
    }

    WidgetRef<NumericInput>
    UIComposer::numericInput(NumericInputKind kind,
                             std::shared_ptr<NumericModel> model,
                             NumericInput::Changed changed,
                             NumericInput::Submitted submitted,
                             NumericInputOptions options) {
        return emplace<NumericInput>(kind,
                                     std::move(model),
                                     std::move(changed),
                                     std::move(submitted),
                                     std::move(options));
    }

    WidgetRef<NumericInput>
    UIComposer::intInput(std::shared_ptr<NumericModel> model,
                         NumericInput::Changed changed,
                         NumericInput::Submitted submitted,
                         NumericInputOptions options) {
        options.precision = 0;
        if (!std::isfinite(options.step) || options.step <= 0.0) {
            options.step = 1.0;
        }
        return numericInput(NumericInputKind::integer,
                            std::move(model),
                            std::move(changed),
                            std::move(submitted),
                            std::move(options));
    }

    WidgetRef<NumericInput>
    UIComposer::floatInput(std::shared_ptr<NumericModel> model,
                           NumericInput::Changed changed,
                           NumericInput::Submitted submitted,
                           NumericInputOptions options) {
        if (!std::isfinite(options.step) || options.step < 0.0) {
            options.step = 0.1;
        }
        return numericInput(NumericInputKind::floatingPoint,
                            std::move(model),
                            std::move(changed),
                            std::move(submitted),
                            std::move(options));
    }

    WidgetRef<Autocomplete>
    UIComposer::autocomplete(std::shared_ptr<TextEditModel> model,
                             AutocompleteProvider provider,
                             Autocomplete::Changed changed,
                             Autocomplete::Submitted submitted,
                             Autocomplete::Completed completed,
                             AutocompleteOptions options) {
        return emplace<Autocomplete>(std::move(model),
                                     std::move(provider),
                                     std::move(changed),
                                     std::move(submitted),
                                     std::move(completed),
                                     std::move(options));
    }

    WidgetRef<Tooltip> UIComposer::tooltip(std::string text,
                                           TooltipOptions options) {
        return emplace<Tooltip>(std::move(text), std::move(options));
    }

    WidgetRef<ContextMenuRegion>
    UIComposer::contextMenu(std::shared_ptr<MenuModel> model,
                            MenuId menu,
                            ContextMenuOptions options) {
        return emplace<ContextMenuRegion>(
            std::move(model), menu, std::move(options));
    }

    WidgetRef<Spacer> UIComposer::spacer(SpacerOptions options) {
        return emplace<Spacer>(std::move(options));
    }

    WidgetRef<Gap> UIComposer::gap(float pixels) {
        return emplace<Gap>(pixels);
    }

    WidgetRef<ScrollView> UIComposer::scrollView(ScrollViewOptions options) {
        return emplace<ScrollView>(std::move(options));
    }

    WidgetRef<TabBar> UIComposer::tabBar(std::shared_ptr<TabModel> model,
                                         TabBarOptions options) {
        return emplace<TabBar>(std::move(model), std::move(options));
    }

    WidgetRef<DockSpace> UIComposer::dockSpace(DockSpaceOptions options) {
        return emplace<DockSpace>(std::move(options));
    }

    WidgetRef<MenuBar> UIComposer::menuBar(std::shared_ptr<MenuModel> model,
                                           MenuBarOptions options) {
        return emplace<MenuBar>(std::move(model), std::move(options));
    }

} // namespace Bess::UI
