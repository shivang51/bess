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

`UITarget` uses `WidgetTree` CPU hit testing for input picking by default. This
matches event routing and avoids synchronously stalling the frame on a GPU
texture readback. Integrations that genuinely need per-fragment
`PickingId::info` values can explicitly select
`UITargetPickingStrategy::synchronousGpuReadback`; it is a compatibility path,
not the recommended interactive path.

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
                m_title.mutate([](Bess::UI::Label &label) {
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

Use `WidgetRef::mutate` (the clearer alias of the existing `update` API), or
`WidgetTree::mutateWidget` at the low level, when changing a retained control.
Both keep invalidation explicit and callback-time deletion safe:

```cpp
tree.mutateWidget<Bess::UI::Label>(
    labelId,
    Bess::UI::WidgetInvalidation::layout |
        Bess::UI::WidgetInvalidation::paint,
    [](Bess::UI::Label &label) { label.setText("Updated"); });
```

Common retained-state and layout changes do not require mutation lambdas:

```cpp
auto save = ui.button("Save");
save.setLayout({
    .width = 84.f, // plain dimensions are pixels
    .height = 24.f,
    .margin = Bess::Core::Style::Margin::fromHorizontal(4.f),
    .flexShrink = 0.f,
});

save.setEnabled(canSave);
save.focus();
save.hide();     // keeps its layout slot
save.collapse(); // removes its layout slot
save.show();
```

`LayoutSpec` is a patch: omitted fields remain unchanged. Use
`LayoutLength::percent(50.f)`, `fraction(0.5f)`, `autoSize()`, `fitContent()`,
`maxContent()`, or `stretch()` when pixel sizing is not appropriate.
`WidgetRef::updateLayout` remains the escape hatch for uncommon `LayoutNode`
operations and has the same deferred-removal safety as widget mutations.

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
        background.setLayout({.width = 180.f, .height = 60.f});
        overlay.label("Drawn above the surface");
    });
```

The stack clips children by default. Set `clipChildren = false` for intentional
overflow such as badges or effects. An explicit child layout Z value overrides
declaration order for both painting depth and hit testing.

## Trees, images, and renderer-backed views

`TreeNode` is a lightweight disclosure control for small retained hierarchies.
Its builder composes into a private content host, so collapsing a branch never
overwrites visibility chosen by descendants. Nested children indent so their
leading edge lines up with the parent label text (after the disclosure and
optional icon slots). Large data sets should build a virtualized model-backed
tree on the same interaction vocabulary.

```cpp
ui.treeNode("Assets", [](Bess::UI::UIComposer &branch) {
    branch.treeNode("Textures", [](Bess::UI::UIComposer &textures) {
        textures.label("logo.png");
    });
});
```

`Card` is a Flutter-style elevated surface with padding and a private flex
content host. `ListView` is a scrollable retained list; compose initial items
declaratively, then mutate them later through `WidgetRef::mutate` helpers such
as `emplaceItem`, `removeItem`, and `clearItems`.

```cpp
auto list = ui.listView([](Bess::UI::UIComposer &items) {
    items.label("First");
});
list.mutate([](Bess::UI::ListView &view) {
    view.emplaceItem<Bess::UI::Label>("Second");
});

ui.card([](Bess::UI::UIComposer &card) {
    card.label("Title");
    card.label("Supporting text");
});
```

`intInput` and `floatInput` are single-line numeric text boxes (shared
`NumericModel`, partial-edit filtering, Enter/blur commit, optional range
clamp, and arrow-key stepping). They follow the same interaction model as the
scene-layer scalar input.

`Image` passively presents any shared renderer texture with fill, contain,
cover, intrinsic, or scale-down fitting. It owns no renderer or device state.
Use `dynamicImage` with a provider when presenting a render-target attachment:
`RenderSurface::resize()` may replace its texture objects, so a texture returned
by `colorTexture()` or `pickingTexture()` is only a snapshot. The provider
should reacquire the current attachment instead of retaining that snapshot.

`RenderView` is the active counterpart for scene previews and viewports: its
`IRenderViewDelegate` updates application state, records offscreen draw calls,
and handles input while `RenderSurface` owns only target attachments. Exactly
one mounted `RenderView` may produce a given surface; additional passive
presentations may consume its color texture through `dynamicImage`.
Offscreen work runs in `WidgetTree::prepareRender`, before the target's main
frame begins; `paint` only composites the completed color texture.

```cpp
class PreviewRenderer final : public Bess::UI::IRenderViewDelegate {
  public:
    void render(Bess::UI::RenderViewFrameContext &frame) override {
        frame.renderer.drawQuad({
            .position = {},
            .size = {float(frame.extent.width), float(frame.extent.height)},
            .color = {0.1f, 0.2f, 0.4f, 1.f},
            .transformMode =
                Bess::Core::Renderer::RenderTransformMode::Screen,
        });
    }
};

ui.renderView(std::make_shared<PreviewRenderer>(),
              {.policy = Bess::UI::RenderPolicy::whileVisible});
```

Use `onDemand` for expensive previews and call `requestRender()` after model
changes. `whileVisible` is appropriate for ordinary scene viewports;
`continuous` deliberately keeps rendering while the widget is hidden.

`RenderView` and `RenderSurface` exclusively own frame begin/end and target
resize. A delegate may adjust the supplied frame description in
`configureFrame()` and issue draw commands in `render()`; it must not begin or
end a renderer frame, recursively render the same surface, or mutate that
surface's attachments from those callbacks. Sampling a color or picking
texture while it is attached to the active frame is a render feedback loop and
is prohibited. Use a separate input surface or ping-pong attachments for
post-processing. Delegate attach/detach callbacks follow the mounted view, and
`setDelegate()` safely reconciles replacements made from lifecycle callbacks.

## Scene viewports that own their own frame

Some producers — notably `Scene::draw` today — already call
`IRenderer2D::beginFrame` / `endFrame` against color and picking
`TextureHandle`s. Nesting that path inside `RenderView` is incorrect.

Use `SceneView` for those producers. It still:

- creates and resizes offscreen color/picking attachments;
- runs `ISceneViewDelegate::render` during `prepareRender`;
- composites the color texture in `paint`;
- routes input, pointer capture, and cursor through the delegate.

It deliberately **does not** begin or end the renderer frame. The frame
context supplies handles for the application path:

```cpp
class SceneViewportDelegate final : public Bess::UI::ISceneViewDelegate {
  public:
    void render(Bess::UI::SceneViewFrameContext &frame) override {
        // Keep the same shared_ptr the rest of the app already owns; the
        // frame only needs the handles + a non-owning renderer reference.
        Canvas::View2D view{
            .camera = m_camera,
            .renderer = m_renderer,
            .drawRenderTarget = frame.colorTarget,
            .pickingRenderTarget = frame.pickingTarget,
            .viewportCtx = m_viewportCtx,
        };
        // Scene::draw owns beginFrame/endFrame against those handles.
        m_scene->draw(view);
    }

    Bess::UI::UIEventReply
    onEvent(Bess::UI::SceneView &,
            Bess::UI::WidgetEventContext &context,
            const Bess::UI::UIEvent &event) override {
        // Map context.localPointerPosition() into ViewportInputContext and
        // forward to the scene. Use capturePointer while dragging.
        return {};
    }
};

ui.sceneView(std::make_shared<SceneViewportDelegate>(),
             {.policy = Bess::UI::RenderPolicy::whileVisible});
```

Async hover and marquee picking use the same renderer APIs as the ImGui
panel: `SceneView::requestPickingId` / `requestPickingIds` /
`tryGetPickingIds`, or the renderer methods directly with
`frame.pickingTarget`. Prefer those over the synchronous
`readPickingId` helper for interactive use.

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

## Focus and modal interaction

`WidgetTree` owns keyboard focus. `Tab` and `Shift+Tab` traverse enabled,
visible, focusable widgets in retained tree order. `FocusScope` adds an
optional default target, autofocus, traversal trapping, and restoration of the
previously focused widget:

```cpp
Bess::UI::WidgetRef<Bess::UI::Button> primary;
auto scope = ui.focusScope(
    {.focus = {.trapFocus = true,
               .autoFocus = true,
               .restoreFocus = true}},
    [&primary](Bess::UI::UIComposer &dialog) {
        dialog.textBox({}, {}, {}, {.placeholder = "Name"});
        primary = dialog.button("Create");
        dialog.button("Cancel");
    });

scope.mutate([primary](Bess::UI::FocusScope &value) {
    value.setDefaultFocus(primary.id());
});
```

`UIViewHost::mountModal` supplies the trapping/autofocus/restoration policy at
the view boundary automatically. Modal view roots also block pointer input to
lower layers. Popups opened from a modal form a nested focus scope, so their
controls can receive focus without exposing background controls.

## Actions and shortcuts

`ActionRegistry` is the shared command authority for action-bound controls,
keyboard shortcuts, programmatic dispatch, command palettes, and plugins.
Actions use semantic namespaced IDs, cached presentation and availability
state, and ordered global, window, panel, editor, popup, and modal scopes. A
shortcut is dispatched only after the focused widget declines it; Tab
traversal and popup/drag Escape handling retain precedence.

```cpp
const Bess::UI::ActionId save{"project.save"};
ui.tree().actions().registerAction({
    .id = save,
    .state = {.label = "Save", .description = "Save the project"},
    .shortcuts = {{.key = Bess::KeyCode::s,
                   .modifiers = Bess::UI::KeyChordModifier::control}},
    .invoked = [](const Bess::UI::ActionInvocation &) { /* save */ },
});
ui.actionButton(save);
```

An `ActionButton` tracks label, enabled state, scope availability, and
visibility through registry notifications without polling during paint. Views
that register callbacks capturing themselves must unregister those actions (or
remove their private scope) from `onUnmounting`. `UITargetDesc::actionRegistry`
can inject one application registry across several targets; injection must
happen before widgets are mounted.

`MenuModel` remains a standalone presentation hierarchy for now: its item
labels, enabled/check state, shortcut text, and activation callbacks are not
implicitly synchronized with `ActionRegistry`. A menu may invoke a registry
action explicitly from its callback, but applications that need live
action-backed menus should add that adapter before treating the menu as another
view of action state. Keeping this boundary explicit avoids two competing
sources of truth masquerading as one.

Scope activation belongs to the registry, not to a `UITarget`. Consequently,
all active window, panel, editor, popup, and modal scopes in a shared registry
participate in shortcut dispatch from every attached target. Share a registry
directly for application-global actions and state. For target-local contextual
shortcuts, either use one registry per target or have the host activate and
deactivate scopes on target focus so only the intended context is active. The
registry does not infer that coordination from widget focus.

## Value controls

`CheckBox`, `ToggleSwitch`, `RadioButton`, and `Slider` use renderer-neutral,
shareable models. They all use the same `Pressable` state machine as buttons;
sliders additionally retain pointer capture for dragging. Keyboard activation,
arrow navigation, Home/End, and Page Up/Page Down are handled by the controls.

```cpp
auto enabled = std::make_shared<Bess::UI::BoolModel>(true);
auto mode = std::make_shared<Bess::UI::RadioGroupModel>();
auto opacity =
    std::make_shared<Bess::UI::RangeModel>(0.0, 1.0, 0.75, 0.05);

ui.toggle("Enabled", enabled);
ui.radio("Fast", mode);
ui.radio("Accurate", mode);
ui.slider(opacity);
```

Models emit a scoped `Signal` connection and may be shared by multiple views.
`RangeModel` separately reports value, range, and step changes, including a
range change that leaves the current value unchanged.

## Popups and anchored controls

Every `UITarget` owns one `PopupHost`. `AnchoredPopupOptions` can anchor to a
widget, explicit bounds, or a point; choose a preferred side and alignment,
minimum/maximum size, viewport margin, anchor-width matching, flipping,
outside-click behavior, and focus policy. Placement is calculated by the pure
`PopupPlacementSolver`, then clamped to the target viewport.

```cpp
auto popup = target.getPopupHost().open(
    {.anchor = Bess::UI::PopupAnchor::forWidget(anchor.id()),
     .preferredSide = Bess::UI::PopupSide::bottom,
     .allowFlip = true,
     .focus = {.trapFocus = true,
               .autoFocus = true,
               .restoreFocus = true}},
    [](Bess::UI::UIComposer &content) {
        content.button("Popup action");
    });

popup.close();
```

Outside presses and Escape dismiss the topmost eligible popup. Dismissal is
safe during event dispatch, and focus returns to the opener when it still
exists. `Dropdown`, `ContextMenu`, nested submenus, `Tooltip`, and
`Autocomplete` all build on this host instead of maintaining independent
overlay or dismissal systems. Long dropdown, autocomplete, and context-menu
lists scroll within their viewport.

## Drag and drop

`DragDropService` owns one active drag session. A target uses a private service
by default. Payloads are immutable, cheaply copied, and may expose several
typed representations at once. MIME-like names make file/text formats
interoperable; namespaced custom formats keep application data strongly typed.

```cpp
Bess::UI::DragPayloadBuilder payload;
payload.set(Bess::UI::DragFormats::plainText, std::string{"Asset 42"});

ui.dropZone(
    {.callbacks = {
         .propose = [](const Bess::UI::DragTargetEvent &event) {
             return event.payload.has(Bess::UI::DragFormats::plainText.id())
                        ? Bess::UI::DragProposal{Bess::UI::DragOperation::copy}
                        : Bess::UI::DragProposal{};
         },
         .onDrop = [](const Bess::UI::DropEvent &event) {
             return event.payload.get(Bess::UI::DragFormats::plainText) !=
                    nullptr;
         },
     }},
    [&](Bess::UI::UIComposer &target) {
        target.draggable(
            {.payload = std::move(payload).build(),
             .allowedOperations = Bess::UI::DragOperation::copy},
            [](Bess::UI::UIComposer &source) {
                source.label("Drag this asset");
            });
    });
```

The service delays expensive payload creation until the pointer crosses the
drag threshold, negotiates copy/move/link operations, searches nested targets
from deepest to ancestor, and guarantees enter/over/leave/completion cleanup.
Pointer capture is canceled when dragging takes ownership, so a pressed child
cannot activate on release. Escape cancels the active drag before dismissing a
popup.

`ReorderableList` builds on the same service. Its callback receives stable item
IDs plus a `before` boundary and mutates the application model; the control
does not secretly reorder model state. `ReorderListComposer` enforces direct
item structure and supplies generated or explicit IDs.

Native window drags use the same lifecycle through `ExternalDragEvent`; file
lists, URI lists, plain text, and raw native MIME representations are available
to ordinary `DropZone`s. Native adapters must ask the retained target
synchronously whether each offer is accepted. The application X11 bridge uses
that answer for XDND status/completion and suppresses the legacy
`WindowDropEvent` when a retained `DropZone` has already committed the drop, so
one offer is not imported twice. Unhandled offers can still use the legacy
compatibility route.

Inject `UITargetDesc::dragDropService` before mounting widgets when several
targets need one drag authority. Sharing the service does not route pointer or
native protocol events, identify the window under the pointer, or convert
coordinates. The host must dispatch each update once to the appropriate
`WidgetTree` in that target's top-left coordinate space and deliver terminal
leave/drop/cancel transitions.

## Text editing

`TextEditModel` is independent of widgets and renderers. It owns UTF-8 text,
grapheme-aligned selection and caret offsets, grapheme/word navigation,
bounded undo/redo, maximum-length enforcement, and an IME composition range.
`TextBox` adds single-line painting, selection, pointer dragging, shortcuts,
horizontal scrolling, placeholder text, submission, and caret blinking.

```cpp
auto text = std::make_shared<Bess::UI::TextEditModel>("initial value");
ui.textBox(
    text,
    [](const std::string &value) { /* value changed */ },
    [](const std::string &value) { /* Enter pressed */ },
    {.placeholder = "Name"});

auto count = std::make_shared<Bess::UI::NumericModel>(0.0);
ui.intInput(count, [](double value) { /* committed integer */ });
ui.floatInput(std::make_shared<Bess::UI::NumericModel>(0.5),
              {},
              {},
              {.precision = 3, .step = 0.05, .minimum = 0.0, .maximum = 1.0});

ui.autocomplete(
    text,
    [](std::string_view query) {
        return std::vector<Bess::UI::AutocompleteItem>{
            {.label = "Result", .replacement = "Result"},
        };
    });
```

Clipboard access and native text-input lifecycle are provided per target by
`UIPlatformServices`; `TextBox` contains no GLFW, Win32, Cocoa, X11, or Wayland
code. Hosts deliver committed scalar input as `TextInputEvent` and pre-edit
state as `TextCompositionEvent`. Native IME adapters use
`beginTextInput`/`updateTextInputArea`/`endTextInput` to position and control
their candidate UI. Offscreen targets receive an inert service by default.

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
