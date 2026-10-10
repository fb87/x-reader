# Lua Scenes

The Lua application uses retained scene stages and a stack-based navigator.
Scenes are not recreated when covered or popped; their selection, scroll, focus,
and internal mode remain available for the application session.

## Scenes

| Scene | Chrome | Purpose |
| --- | --- | --- |
| `loader` | Fullscreen | Initialization, restore, and failure recovery |
| `launcher` | Status/content/dock | Resume, Library, Files, and Settings shortcuts |
| `library` | Status/content/dock | Imported EPUB publications |
| `file_manager` | Status/content/dock | Sandboxed storage browsing |
| `settings` | Status/content/dock | Root categories and retained subpages |
| `reading` | Fullscreen | EPUB pages and bottom reader controls |
| `sleep` | Fullscreen | Static sleep image and wake handling |

Loader is replaced by Launcher after initialization. Sleep is pushed over the
current scene and popped on wake, restoring the exact previous stage.

## Retained Stage

Each scene is created once and owns a stage containing mutable UI state:

```lua
{
    focus_scope = "content",
    content_focus = "stable-id",
    dock_focus = "back",
    scroll = 0,
}
```

Dynamic scenes use stable IDs rather than indexes. If an item disappears,
selection moves to the nearest valid item.

## Scene Contract

```lua
return {
    id = "library",
    chrome = "standard", -- or "fullscreen"
    create = function(context) return {} end,
    show = function(stage, params, reason) end,
    pause = function(stage) end,
    resume = function(stage) end,
    event = function(stage, event) end,
    update = function(stage, now) end,
    draw = function(stage, content_rect) end,
    title = function(stage) return "Library" end,
    dock = function(stage) return {} end,
    snapshot = function(stage) return {} end,
    restore = function(stage, snapshot) end,
}
```

`app.lua` owns the registry, stack, transitions, active scene, retained stages,
and snapshot serialization. Scenes request `push`, `pop`, `replace`, `pop_to`,
or `reset` through their context.

## Stack Rules

- A scene ID appears at most once in the stack.
- Pushing an existing scene performs `pop_to` rather than duplicating it.
- `replace` removes the current scene before entering the target.
- Back closes an overlay first, then pops the active scene.
- Launcher is the root and has no Back action.
- Popping resumes the previous retained stage without resetting focus.

Typical flow:

```text
loader --replace--> launcher --push--> library --push--> reading
                                                  |
                                                  +--push--> settings(reading)
```

## Standard Chrome

Standard scenes share fixed geometry:

```text
status bar: 0..55
content:    56..879
dock:       880..959
```

Only `chrome.lua` draws the status bar and dock. Scenes receive the content
rectangle and cannot draw over shared chrome.

Status values come from services. Unknown clock, battery, or storage values
render as unavailable; fabricated values are not allowed.

The dock is returned by the active scene as zero to three actions. Chrome owns
slot geometry, enabled state, focus, touch hit testing, and invocation.

## Input

The application supports three physical keys: `up`, `down`, and `push`.
Touch is optional. SDL mouse input is converted to logical touch coordinates.

Short Up and Down wrap within the current focus scope. Long Up or Down toggles
between content and dock. Short Push activates the highlighted item. Long Push
toggles Sleep/wake.

The input controller uses a 30 ms debounce and 500 ms long-press threshold. SDL
key-repeat events are ignored. Input names are stable Lua values; SDL and
hardware constants never reach scenes.

Touch selects and immediately activates content or dock items. Reading touch
zones turn pages or toggle the bottom reader panel.

## Fullscreen Scenes

Loader, Reading, and Sleep use the complete 540 x 960 display.

Reading has no global chrome. Its center interaction opens a bottom action panel
with Contents, Bookmarks, Reading Settings, Book Information, Close Book, and
Sleep. The panel is internal to Reading and does not enter the global stack.

Sleep renders a minimal logo, clock, battery, and wake hint once, then requests
platform sleep. Deep-sleep restore persists the stack and stage data before the
Lua VM is destroyed.

## Snapshot Data

Hot reload and deep sleep serialize bounded data only:

```lua
{
    version = 1,
    stack = { "launcher", "library", "reading" },
    stages = {
        launcher = { content_focus = "library" },
        library = { content_focus = "book-id", scroll = 2 },
        reading = { book_id = "book-id", spine = 1, offset = 0 },
    },
}
```

Native sessions, files, parsers, and userdata are reopened from stable IDs.

## Production Requirements

- No demo books, fabricated status values, or fake storage in production scenes.
- Library and File Manager consume bounded C service results.
- Reading uses pixel-width text measurement and C word wrapping.
- Scene transitions invalidate the full display; page updates use quality or
  partial refresh according to the platform policy.
- Every scene has empty, loading, error, and unavailable states where relevant.
