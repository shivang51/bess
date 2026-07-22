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
- Widget subtree Z values are inherited through painter layers, so overlays
  and floating content keep their internal draw order as one unit.
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

## Containers

`row` and `column` create sequential flex layouts. `stack` creates an overlay:
all children share one padded content slot and later children paint and hit-test
above earlier children at the same Z value. Horizontal and vertical alignment
can independently use `start`, `center`, `end`, or `stretch`; child margins are
applied inside the slot.

```cpp
ui.stack(
    {.horizontalAlignment = Bess::UI::StackAlignment::center,
     .verticalAlignment = Bess::UI::StackAlignment::center},
    [](Bess::UI::UIComposer &overlay) {
        auto background = overlay.surface();
        background.updateLayout([](Bess::UI::LayoutNode &layout) {
            layout.setWidth(180.f);
            layout.setHeight(60.f);
        });
        overlay.label("Drawn above the surface");
    });
```

The stack clips children by default. Set `clipChildren = false` for intentional
overflow such as badges or effects. An explicit child layout Z value overrides
declaration order for both painting depth and hit testing.

## Writing a control

Derive from `Widget` and override only the required hooks:

- configure the associated `LayoutNode` in `onMount`;
- refresh mutable intrinsic dimensions in `updateLayout` (the context reports
  theme changes);
- arrange direct children in `arrange`;
- emit renderer-neutral commands in `paint`;
- use `paintOverlay` for adorners that must appear above descendants, such as
  focus rings, selection handles, and docking previews;
- override `hitTest` when a retained popup intentionally extends outside the
  widget's layout box; hit testing honors subtree Z just like painting does;
- return a `UIEventReply` from `onEvent` to request focus, pointer capture,
  invalidation, handling, or propagation changes.

Events follow capture, target, and bubble phases. `Pressable` contains the
shared mouse/keyboard activation state machine used by buttons and tabs; new
clickable controls should compose it instead of duplicating input logic.

## Scrolling

`ScrollView` owns one content root and adds horizontal or vertical scrollbars
only when that root's descendant constraints cannot fit the viewport. Use a
`FlexContainer` as the content root when a viewport contains multiple controls:

```cpp
ui.scrollView([](Bess::UI::UIComposer &viewport) {
    viewport.column([](Bess::UI::UIComposer &content) {
        content.label("First row");
        content.label("Second row");
    });
});
```

The composer overload validates the single-root contract transactionally.
Wheel input supports nested scroll chaining, Shift+wheel scrolls horizontally,
and scrollbar-thumb drags retain pointer capture. Scrollbar gutters are
reserved from layout; descendant painting and hit testing are clipped to the
remaining viewport. `DockPanel` uses this control internally, so docked and
floating panel content gains the same overflow behavior automatically.

## Menus

`MenuModel` is a renderer-neutral command hierarchy with stable `MenuId` and
`MenuItemId` values. A command can provide an icon glyph, name, displayed
shortcut, enabled/checked state, callback, and arbitrary nested children;
separators are explicit items.

```cpp
auto menus = std::make_shared<Bess::UI::MenuModel>();
menus->addMenu({
    .name = "File",
    .items = {
        {.icon = "+",
         .name = "New",
         .shortcut = "Ctrl+N",
         .activated = [] { /* create document */ }},
        {.name = "Recent",
         .children = {
             {.name = "project.bess",
              .activated = [] { /* open document */ }},
         }},
        Bess::UI::MenuItem::separator(),
    },
});

ui.row([&](Bess::UI::UIComposer &applicationBar) {
    applicationBar.label("B");                 // app icon/branding
    applicationBar.menuBar(menus);              // intrinsic-width control
    applicationBar.spacer();                    // flexible middle region
    applicationBar.button("Account", onAccount); // arbitrary actions
});
```

`MenuBar` is content-sized and background-neutral by default, leaving the
surrounding row or surface in charge of application-bar chrome. Set
`MenuBarOptions::stretchWidth` or `drawBackground` for standalone use.

The shortcut string is presentation metadata; application command routing
remains the authority for global accelerators. Menu placement, painting, and
input use `MenuBarLayoutSolver`, including viewport-edge flipping and submenu
overlap. Pointer and keyboard navigation share the same retained open path.

## Tabs and docking

`BasicTabModel<Id>` is an ordered, single-selection model with stable IDs,
move-only detach/attach transfers, and one notification per completed
mutation. `TabStripLayout` is a pure layout/hit-test solver shared by `TabBar`
and `DockSpace`.

`BasicTabItem::closable` controls the shared trailing close affordance. A
standalone `TabBar` removes the item from its model after a confirmed
press-and-release on that affordance; dock tabs use the same geometry and
chrome while preserving their panel recovery state through `hide()`/`show()`.

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
Bess::UI::DockPanelHandle explorer;
ui.dockSpace([&explorer](Bess::UI::DockComposer &dock) {
    explorer = dock.panel(
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

explorer.hide(); // Retains the panel widget and its complete child subtree.
explorer.show(); // Restores its previous stack, or floats if it no longer exists.
```

Keep the returned `DockPanelHandle` when application code needs to control a
panel later. The tab and floating-title-bar close buttons perform the same
non-destructive hide operation. `hide()` and `show()` are idempotent, and the
handle becomes empty when either the panel or its owning `DockSpace` is
destroyed. Use `DockSpace::removePanel()` only for permanent removal.
Set `DockPanelPlacement::closable` to `false` to suppress the close affordance;
application code may still control that panel through its handle.

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

Dock tabs have a small drag threshold. Dragging beyond it transfers the item
into a floating dock host without recreating its widget subtree; releasing
away from a guide leaves it floating. A floating host owns another
`DockSpaceModel`, so tabs can dock into floating windows and form tab stacks or
side splits there using the same transfer path as the main host. Docking a
multi-stack floating host through a side guide grafts its complete topology;
it does not flatten the source splits into one tab stack.

Floating hosts can be resized from each edge and corner. Resizing retains
pointer capture, uses platform resize cursors, preserves the opposite edge,
and respects the theme's minimum and optional maximum dimensions.

During a drag, terminal stacks expose node-local center/side guides. Every host
also exposes four outer-edge guides that wrap the complete existing topology;
the root guide intentionally has no center target. The preview is the exact
destination region. Floating windows may move beyond the `DockSpace` and are
clipped by it, while a small themed title-bar grip remains recoverable.
`DockDropGuideLayoutSolver` is renderer-free so custom themes and tests consume
the same hit regions used by the preview.

The older `DockManager` remains temporarily for compatibility. New retained UI
code should use `DockSpaceModel` and `DockSpace`; once its remaining call sites
are migrated, the legacy model can be removed independently.
