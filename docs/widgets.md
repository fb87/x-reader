# Lua Widgets

The Lua UI uses retained widgets. A scene stage owns serializable state; its
widget tree owns runtime layout, drawing, focus, and callbacks.

```text
scene stage: selected IDs, scroll, focus scope, dock focus
widget tree: bounds, children, callbacks, measured layout
```

Widget trees are built once per retained scene stage and relaid out when the
viewport, theme, font settings, or row model changes. They are not rebuilt from
inside `draw()` and are never serialized into hot-reload or sleep snapshots.

## Common Widgets

- `label`: measured text, alignment, wrapping, ellipsis, and semantic font size
- `button`: icon plus label, enabled state, focus state, and activation
- `list`: stable row IDs, scrolling, selection, wrapping, and hit testing
- `action_bar`: the shared bottom Dock with one to three actions
- `status_bar`: clock, title, battery, and storage status
- `progress`: bounded progress indicator for Loader and long operations
- `dialog`: modal overlay that captures focus without entering the scene stack

## Layout

`layout.lua` provides viewport-aware rectangles and measured positioning.
Widgets use constraints and measured ink bounds rather than fixed baseline
offsets. Icon/text groups are centered as one row group, and multiline label
groups are centered as a block.

The design viewport is 540 x 960, but layout must also work at other aspect
ratios. Standard scenes receive status/content/Dock regions from the shared
chrome layout. Fullscreen scenes receive the complete viewport.

## Standard Row Model

Lists use data rows rather than scene-specific drawing code:

```lua
{
    id = "book:identifier",
    icon = icons.book,
    title = "Book title",
    subtitle = "Author",
    trailing = "42%",
    enabled = true,
}
```

The List widget owns row geometry, focus, scroll, stable-ID reconciliation,
empty states, and activation. Scenes provide rows and business callbacks.

## Action Bar

The existing external name `Dock` is retained. `action_bar.lua` implements it.
It owns equal slot geometry, icon/text alignment, enabled state, focus styling,
touch activation, and Up/Down/Push routing.

Reading uses the same Action Bar when its controls are visible. It does not
implement a separate reader footer.

## Input

`app.lua` routes events in this order:

1. Modal dialog
2. Active widget tree
3. Active content or Action Bar focus scope
4. Fullscreen scene behavior
5. Global long-Push sleep action

Scenes must not compare raw screen coordinates for standard controls. Widgets
receive logical coordinates through hit testing.

## Text And Icons

C provides `canvas.measure()` using generated font metrics. Lua widgets use that
measurement for vertical and horizontal alignment. Text uses dedicated small,
medium, and large Alegreya assets. Icons use generated Material Symbols assets
with the same size profiles.

Runtime TTF or icon-font loading is forbidden. Generated arrays and their
licenses are checked in.

## Migration Rule

When migrating a scene, remove its duplicated row, Dock, status, icon, and hit
testing code in the same change. Do not leave two competing focus systems.
