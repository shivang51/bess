# UI Core

`ui_core` is a retained UI layer for a `UITarget`. It deliberately separates
renderer/device ownership, widget lifetime, interaction state, control models,
and high-level composition.

## Ownership

- `UITarget` owns one render target and one `WidgetTree`.
- `UIViewHost`, owned by `UITarget`, mounts application views into content,
  overlay, and modal layers.
- `WidgetTree` exclusively owns widgets, layout nodes, stable widget IDs,
  focus, pointer capture, hit testing, and picking-ID lookup.
- `Widget` is a small behavior object. It does not own a renderer, a layout
  node, a runtime picking ID, or its children.
- `UIPainter` is the renderer-neutral drawing contract.
  `RendererUIPainter` is the adapter for `IRenderer2D`.
- `DockSpace` is an optional widget. A target has docking only when a
  `DockSpace` is inserted into its tree.

The tree owns `std::unique_ptr<Widget>` objects and callers retain `WidgetId`
or typed `WidgetRef<T>` handles. IDs are never reused. A `WidgetRef` becomes
empty when its widget is removed or its tree is destroyed.
Destruction requested from a widget callback is deferred until the active
dispatch/update/layout/paint traversal has completed.

## Creating a UI

Application UI should normally be expressed as a `UIView` and mounted directly
on its target:

```cpp
class ProjectView final : public Bess::UI::UIView {
  public:
    void compose(Bess::UI::UIComposer &ui) override {
        ui.column([this](Bess::UI::UIComposer &column) {
            m_title = column.label("Project");
            column.button("Create", [this] {
                m_title.update([](Bess::UI::Label &label) {
                    label.setText("Created");
                });
            });
        });
    }

  private:
    Bess::UI::WidgetRef<Bess::UI::Label> m_title;
};

auto project = uiTarget.setContent<ProjectView>();
auto tooltip = uiTarget.mountOverlay<MyTooltipView>();
auto dialog = uiTarget.mountModal<MyDialogView>();
```

`setContent` replaces the previous content transactionally. Failed composition
leaves the old content mounted. Nested builders also roll back the subtree they
started. Views and their callback captures remain alive through callback-time
unmount, then are released after event dispatch completes.

Reusable controls are ordinary functions that receive a composer:

```cpp
void composeToolbar(Bess::UI::UIComposer &ui) {
    ui.row([](Bess::UI::UIComposer &row) {
        row.button("Open");
        row.spacer();
        row.button("Save");
    });
}
```

The low-level tree remains available as an escape hatch for control internals,
tests, and incremental migration:

```cpp
auto &tree = uiTarget.getWidgetTree();

const auto root = tree.emplaceWidget<Bess::UI::FlexContainer>(
    Bess::UI::FlexContainerOptions{
        .direction = Bess::UI::LayoutDirection::vertical,
    });

tree.emplaceChild<Bess::UI::Label>(root, "Project");
tree.emplaceChild<Bess::UI::Button>(root, "Create", [] {
    // Application action.
});
```

Use `WidgetRef::update` (or `WidgetTree::mutateWidget` at the low level) when
changing a retained control, so invalidation remains explicit and
callback-time deletion remains safe:

```cpp
tree.mutateWidget<Bess::UI::Label>(
    labelId,
    Bess::UI::WidgetInvalidation::layout |
        Bess::UI::WidgetInvalidation::paint,
    [](Bess::UI::Label &label) { label.setText("Updated"); });
```

## Writing a control

Derive from `Widget` and override only the required hooks:

- configure the associated `LayoutNode` in `onMount`;
- refresh mutable intrinsic dimensions in `updateLayout` (the context reports
  theme changes);
- arrange direct children in `arrange`;
- emit renderer-neutral commands in `paint`;
- return a `UIEventReply` from `onEvent` to request focus, pointer capture,
  invalidation, handling, or propagation changes.

Events follow capture, target, and bubble phases. `Pressable` contains the
shared mouse/keyboard activation state machine used by buttons and tabs; new
clickable controls should compose it instead of duplicating input logic.

## Tabs and docking

`BasicTabModel<Id>` is an ordered, single-selection model with stable IDs,
move-only detach/attach transfers, and one notification per completed
mutation. `TabStripLayout` is a pure layout/hit-test solver shared by `TabBar`
and `DockSpace`.

`DockSpaceModel` has two node kinds:

- a terminal `DockStackNode`, which contains one or more dock items;
- a `DockSplitNode`, which contains exactly two dock-node IDs.

There is no leaf-to-tab node conversion. A one-item stack is the leaf case,
and adding another item to its main zone naturally makes it a tab stack.
`DockItemId`, `DockNodeId`, and `WidgetId` are distinct types, so model identity
does not change when the topology changes.

Views normally create dockable content with `DockComposer`. Each call produces
a `DockPanel`, a stable `DockItemId`, and an ordinary composer for its content:

```cpp
ui.dockSpace([](Bess::UI::DockComposer &dock) {
    const auto explorer = dock.panel(
        "Explorer", [](Bess::UI::UIComposer &panel) {
            panel.label("Files");
        });

    dock.panel(
        "Inspector",
        Bess::UI::DockPanelPlacement{
            .target = dock.stackFor(explorer.item),
            .zone = Bess::UI::DockZone::right,
        },
        [](Bess::UI::UIComposer &panel) {
            panel.label("Properties");
        });
});
```

The lower-level equivalent remains available:

```cpp
const auto dockId = tree.emplaceWidget<Bess::UI::DockSpace>();
auto *dock = tree.getWidget<Bess::UI::DockSpace>(dockId);

const auto explorer = dock->createPanel(
    tree,
    dockId,
    "Explorer",
    std::make_unique<MyExplorerWidget>());

dock->createPanel(tree,
                  dockId,
                  "Inspector",
                  std::make_unique<MyInspectorWidget>(),
                  dock->model().stackForItem(explorer.item),
                  Bess::UI::DockZone::right);
```

Only the active panel in each stack is arranged, painted, and hit-tested.
Empty stacks collapse transactionally, preserving the IDs of surviving nodes
and items. `DockSpaceModel::validate` is available for tests, deserialization,
and debug assertions at topology boundaries.

The older `DockManager` remains temporarily for compatibility. New retained UI
code should use `DockSpaceModel` and `DockSpace`; once its remaining call sites
are migrated, the legacy model can be removed independently.
