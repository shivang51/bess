#include "ui_composer.h"

#include <utility>

namespace Bess::UI {

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
        if (auto *dock = m_dockSpace.get(); dock != nullptr) {
            static_cast<void>(dock->removePanel(m_tree, panel.item));
        } else if (panel.panel) {
            static_cast<void>(m_tree.removeWidget(panel.panel));
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

    WidgetRef<Label> UIComposer::label(std::string text, LabelOptions options) {
        return emplace<Label>(std::move(text), std::move(options));
    }

    WidgetRef<Button> UIComposer::button(std::string label,
                                         Button::Activated activated,
                                         ButtonOptions options) {
        return emplace<Button>(
            std::move(label), std::move(activated), std::move(options));
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
