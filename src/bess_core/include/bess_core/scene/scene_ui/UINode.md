# UINode

> Generated using LLM (I audited some of it)

`UINode` is the low-level layout node used by scene UI components. It wraps a
Yoga node and exposes the subset of layout behavior used by Bess UI code:
tree ownership, sizing, padding, margin, flex direction, alignment, and cached
draw coordinates.

Most application code should access a node through `UISceneComponent::getUINode()`.
Create standalone nodes through `UINodeRegistry`.

```cpp
#include "bess_core/scene/scene_ui/layout.h"

using namespace Bess::Canvas::UI;
```

## Registry And Ownership

Every `UINode` should be registered in a `UINodeRegistry`. The registry owns
the shared Yoga config and stores nodes by `UUID`.

```cpp
UINodeRegistry registry;

auto *root = registry.addNode(Bess::UUID());
auto *child = registry.addNode(Bess::UUID());

root->addChild(child);
```

Notes:

- `addNode(UUID)` creates a node with that id and returns `nullptr` if it already
  exists.
- `addNode(const UINode&)` copies the node into the registry.
- `removeNode(id)` detaches the node from its parent and clears child parent ids.
- `addChild(child)` reparents `child` if it already belongs to another node.
- `clearChildren()` only clears the layout tree links; it does not remove nodes
  from the registry.

## Layout Lifecycle

Layout is calculated from root nodes. 

```cpp
root->layout(registry, Bess::UUID::null);
```

`layout()` uses Yoga internally and then syncs Bess cached draw values. Call it after
node properties or tree structure changes and before drawing.

For scene components this is usually handled by `ComponentsLayer`, which prepares
dirty UI components, then lays out every root node in the scene UI registry.

## Coordinate And Box Model

`UINode` uses center-based draw coordinates because the renderer draws quads from
their center. Yoga internally uses top-left coordinates; `UINode` converts them
when syncing layout.

Important values after layout:

- `getDrawPos()` returns the rendered box center plus z.
- `getDrawSize()` returns the rendered box size, excluding margin.
- `getCachedSize()` returns the layout footprint, including margin.
- `getCachedPos()` returns the rendered box center without z.

Box model:

- Size and `getDrawSize()` describe the rendered box.
- Padding is inside the rendered box.
- Margin is outside the rendered box and contributes to `getCachedSize()`.

Padding and margin values are `top, right, bottom, left`.

```cpp
node->setPadding(Core::Style::Padding(8.f, 12.f, 8.f, 12.f));
node->setMargin(Core::Style::Margin::onlyRight(16.f));
```

## Sizing

Sizing is configured per axis and maps directly to Yoga dimensions:

```cpp
node->setWidth(120.f);
node->setHeight(40.f);

node->setWidthPercent(50.f); // also accepts 0.5f
node->setHeightAuto();

node->setWidthFitContent();
node->setHeightFitContent();

node->setWidthStretch();
node->setHeightStretch();
```

Rules of thumb:

- Use `setWidth()` / `setHeight()` for fixed point sizes.
- Use `setWidthPercent()` / `setHeightPercent()` when the parent has a definite
  size.
- Use `setWidthFitContent()` / `setHeightFitContent()` for wrap-content behavior.
- Use `setAlignSelf(LayoutSelfAlignment::stretch)` or stretch sizing when a child
  should fill the parent cross axis.
- Use `setMinSize()` and `setMaxSize()` for bounds. A negative axis means
  "unset" for that axis.

Each axis is independent, so mixed modes are explicit:

```cpp
node->setWidthAuto();
node->setHeightFitContent();
```

## Direction And Alignment

`UINode` maps directly to Yoga flexbox concepts.

```cpp
node->setDirection(LayoutDirection::horizontal);
node->setMainAxisAlignment(LayoutAlignment::center);
node->setCrossAxisAlignment(LayoutAlignment::end);
```

- `setDirection()` maps to `flex-direction`.
- `setMainAxisAlignment()` maps to `justify-content`.
- `setCrossAxisAlignment()` maps to `align-items`.
- `setAlignSelf()` maps to `align-self`.

Supported directions:

- `horizontal`
- `vertical`
- `horizontalReverse`
- `verticalReverse`

Supported container alignment values:

- `start`
- `center`
- `end`
- `spaceBetween` (main axis only)
- `spaceAround` (main axis only)
- `spaceEvenly` (main axis only)

`spaceBetween`, `spaceAround`, and `spaceEvenly` map to `justify-content`.
Cross-axis alignment maps to `align-items`, so it supports only start, center,
and end placement.

Supported self alignment values:

- `auto_`
- `start`
- `center`
- `end`
- `stretch`

## Flex

Use flex when children should divide available space. This is usually more robust
than percentage widths inside wrap-content parents.

```cpp
row->setDirection(LayoutDirection::horizontal);
row->setAlignSelf(LayoutSelfAlignment::stretch);
row->setWidthAuto();
row->setHeightFitContent();

leftColumn->setFlex(1.f, 0.f, 0.f);
leftColumn->setMinSize({100.f, -1.f});

rightColumn->setFlex(1.f, 0.f, 0.f);
rightColumn->setMinSize({100.f, -1.f});
```

Flex APIs:

```cpp
node->setFlexGrow(1.f);
node->setFlexShrink(0.f);
node->setFlex(1.f, 0.f, 0.f);

node->setFlexBasis(120.f);
node->setFlexBasis(50.f, Unit::relative); // 50%
node->setFlexBasisAuto();
node->setFlexBasisFitContent();
node->setFlexBasisMaxContent();
node->setFlexBasisStretch();
```

## Common Patterns

### Fixed Box

```cpp
auto *box = registry.addNode(Bess::UUID());

box->setWidth(200.f);
box->setHeight(80.f);
box->setPadding(Core::Style::Padding(8.f));
```

### Wrap-Content Container

```cpp
auto *container = registry.addNode(Bess::UUID());
container->setDirection(LayoutDirection::vertical);
container->setWidthFitContent();
container->setHeightFitContent();
container->setPadding(Core::Style::Padding(8.f));

auto *label = registry.addNode(Bess::UUID());
label->setWidth(100.f);
label->setHeight(20.f);

container->addChild(label);
container->layout(registry, Bess::UUID::null);
```

### Two Slot Columns With Right-Aligned Output Rows

This is the pattern used by simulation component slots. The row stretches to the
component width, each column gets equal flex space, and the output column aligns
its rows to the right edge.

```cpp
slotsRow->setDirection(LayoutDirection::horizontal);
slotsRow->setWidthAuto();
slotsRow->setHeightFitContent();
slotsRow->setAlignSelf(LayoutSelfAlignment::stretch);

inputColumn->setDirection(LayoutDirection::vertical);
inputColumn->setFlex(1.f, 0.f, 0.f);
inputColumn->setMinSize({SIM_COMP_SLOT_COLUMN_SIZE, -1.f});
inputColumn->setMargin(Core::Style::Margin::onlyRight(paddingX));
inputColumn->setCrossAxisAlignment(LayoutAlignment::start);

outputColumn->setDirection(LayoutDirection::vertical);
outputColumn->setFlex(1.f, 0.f, 0.f);
outputColumn->setMinSize({SIM_COMP_SLOT_COLUMN_SIZE, -1.f});
outputColumn->setCrossAxisAlignment(LayoutAlignment::end);
```

## Working From UISceneComponent

`UISceneComponent::prepareUI()` initializes the node, applies style, and attaches
it to the current parent.

```cpp
void MyComponent::prepareUI(SceneUIPrepareCtx &ctx) {
    UISceneComponent::prepareUI(ctx);

    auto *node = getUINode();
    node->setDirection(LayoutDirection::horizontal);
    node->setWidthFitContent();
    node->setHeightFitContent();
}
```

When preparing nested UI components, set `ctx.parentNode` before calling each
child component's `prepareUI()`.

```cpp
ctx.parentNode = container->getUINode();
child->prepareUI(ctx);
```

Restore the previous parent when returning to the caller.

## Drawing

Always draw from cached layout values after layout has run:

```cpp
Core::Renderer::QuadProps props;
props.position = node->getDrawPos();
props.size = node->getDrawSize();
props.zIndex = node->getDrawPos().z;
```

Do not use component transform scale as a substitute for UI size. The layout
system calculates rendered dimensions.

## Dirty Flags

Most setters mark the node dirty and propagate invalidation to ancestors.

Use explicit dirty flags when external state changes without going through a
setter:

```cpp
node->setSizeDirty();
node->setPosDirty();
```

`getSizeDirty()` and `getPosDirty()` are mainly useful for tests and diagnostics.
