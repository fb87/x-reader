# Lua binding guide

This document defines the boundary between the reusable native framework, the Lua runtime, and the Lua application.

## 1. Ownership model

The project deliberately separates **mechanism**, **language binding**, and **application wiring**.

```text
native framework                Lua runtime/binding             Lua application
----------------                -------------------             ---------------
core/router.hpp        <---->   shell.router          <---->    app/lua/routes.lua
core/widgets/*         <---->   shell.widget          <---->    app/lua/pages/*.lua
core/state.hpp         <---->   shell.state           <---->    app/lua/model.lua + pages
reader/*               <---->   shell.reader/library  <---->    reader/library pages
core/connectivity.hpp  <---->   modules.wifi            <---->    connectivity page
```

Rules:

1. `core/` and `reader/` never depend on Lua.
2. `runtime/lua/` is reusable framework binding code. It must not contain ebook-reader routes or page policy.
3. `app/lua/` is the Lua implementation of the product.
4. `app/cpp/` is the native C++ implementation of the same product contract.
5. Route registration belongs to `app/<language>/routes.*`; route matching/history belongs to the framework.
6. Lua composes pages, widgets, behavior, and styles. Native C++ performs the actual rendering and device/domain work.

## 2. Directory contract

```text
app/
├── cpp/
│   ├── init.hpp
│   ├── main.hpp
│   ├── model.hpp
│   ├── routes.hpp
│   └── pages/
│       ├── splash.hpp
│       ├── home.hpp
│       ├── library.hpp
│       ├── favorites.hpp
│       ├── files.hpp
│       ├── reader.hpp
│       ├── settings.hpp
│       ├── connectivity.hpp
│       └── sleep.hpp
└── lua/
    ├── init.lua
    ├── main.lua
    ├── model.lua
    ├── routes.lua
    └── pages/
        ├── splash.lua
        ├── home.lua
        ├── library.lua
        ├── favorites.lua
        ├── files.lua
        ├── reader.lua
        ├── settings.lua
        ├── connectivity.lua
        └── sleep.lua

runtime/lua/
├── api.hpp
├── runtime.hpp
├── frontend.hpp
└── shell/
    ├── init.lua
    ├── router.lua
    ├── page.lua
    ├── widget.lua
    ├── layout.lua
    ├── dialog.lua
    ├── theme.lua
    └── i18n.lua
```

A future language should follow the same application shape, for example `app/python/{init,main,model,routes,pages}` with reusable bindings under `runtime/python/`.

## 3. Lua bootstrap lifecycle

The native runtime creates a Lua 5.4 state, opens the standard libraries, installs a native global table named `shell`, updates `package.path`, and then loads `runtime/lua/shell/init.lua` followed by the selected application entry script.

`runtime/lua/shell/init.lua` augments the native `shell` table with the pure-Lua framework modules:

```lua
shell.theme  = require("shell.theme")
shell.page   = require("shell.page")
shell.widget = require("shell.widget")
shell.layout = require("shell.layout")
shell.dialog = require("shell.dialog")
shell.i18n   = require("shell.i18n")
shell.router = require("shell.router")
```

The application entry exposes the lifecycle functions used by the native frontend:

```lua
init(reload)
render()
event(ev)
tick(now_ms)
```

The normal application implementation delegates these to `app/lua/main.lua`.

## 4. Native `shell` modules

The following modules are created directly by `runtime/lua/runtime.hpp`.

### `shell.api`

Low-level native rendering/device bridge. Application pages should normally prefer `shell.widget` over direct drawing calls.

```text
width() -> integer
height() -> integer
clear(gray)
fill(x, y, w, h, gray)
border(x, y, w, h, width, gray)
hline(x, y, w, gray)
text(x, y, text, scale, gray)
center(x, y, w, h, text, scale, gray)
status(title, ...style)
row(x, y, w, h, primary, secondary, selected, ...style)
button(x, y, w, h, text, active, ...style)
progress(x, y, w, h, value, ...style)
book_card(x, y, w, h, eyebrow, title, active, ...style)
dialog(title, body, ...style)
present()
battery() -> integer
reload()
```

`reload()` requests a safe Lua restart after the current callback returns. It does not restart the native application.

### `shell.state`

Bridge to shared hierarchical native state.

```lua
local value = shell.state.get("reader.book.page")
shell.state.set("reader.book.page", 4)
```

Supported binding values are the scalar types currently supported by the native state store: integer, boolean, and string.

State is not owned by the Lua VM. It survives Lua hot reload.

### `shell.library`

Native library/cache operations:

```text
count() -> integer
book(index) -> table | nil
move(delta)
favorite()
delete()
```

`book(index)` returns a table containing the current native metadata, including `title`, `author`, `progress`, `favorite`, and `index`.

### `shell.reader`

Native reader/session operations:

```text
open_selected() -> boolean
next()
previous()
adjust_font(delta)
```

EPUB parsing, current-book state, pagination state, and reader-domain work stay native.

### `modules.wifi`

Native connectivity operations:

```text
scan() -> boolean
connect_first() -> boolean
```

The current simulator implementation uses this small API for exercising the capability path. Product policy remains in `app/lua/pages/connectivity.lua`.

## 5. Pure-Lua framework modules

### `shell.widget`

This is the normal application-facing presentation API. Each function maps to the same native widget renderer used by the C++ app.

```lua
shell.widget.status("HOME")

shell.widget.row {
    y = 265,
    primary = "Library",
    secondary = "12 books",
    selected = true,
}

shell.widget.button {
    x = 20, y = 800, w = 180, h = 54,
    text = "Open",
    active = true,
}
```

Available helpers currently include `status`, `label`, `center`, `row`, `button`, `progress`, `book_card`, and `dock`.

### `shell.theme`

Style resolution uses this precedence:

```text
native defaults -> global Lua theme -> per-widget override
```

Example:

```lua
shell.theme.set("row", {
    foreground = shell.theme.gray.dark,
    divider = shell.theme.gray.black,
    text_scale = 2,
})
```

Per-widget override:

```lua
shell.widget.row {
    y = 100,
    primary = "Library",
    style = {
        focus_background = shell.theme.gray.dark,
        focus_foreground = shell.theme.gray.white,
    },
}
```

With no style override, Lua and C++ use the same native widget renderer and should preserve visual parity.

### `shell.router`

`runtime/lua/shell/router.lua` implements the Lua-facing router mechanism. It contains no product route table.

Application wiring belongs in `app/lua/routes.lua`:

```lua
shell.router.register {
    path = "/library",
    title = "library",
    page = require("pages.library"),
    menu = { section = "home", order = 10, icon = "library" },
}
```

Navigation:

```lua
shell.router.push("/library")
shell.router.replace("/settings")
shell.router.back()
```

Routes support path parameters and query parameters:

```text
/book/:id/reader
/book/42/reader?mode=night
```

Resolved requests expose `uri`, `path`, `params`, `query`, `route`, and `page`.

### `shell.i18n`

Translations are stored outside the language implementation in `lang/*.txt`.

```lua
shell.i18n.t("library")
shell.i18n.t("books_count", 12)
shell.i18n.set_language("vi")
```

The selected locale is stored in shared state under `system.language`. Missing translations fall back to English.

### `shell.layout`, `shell.dialog`, `shell.page`

These are deliberately small. They provide application-building conveniences but do not duplicate the native renderer.

## 6. Event ABI

Native input is converted into a structured Lua table before `event(ev)` is called.

Key event:

```lua
{
    type = "key",
    key = "down",
    duration_ms = 0,
    long_press = false,
}
```

Touch event:

```lua
{
    type = "tap",
    x = 120,
    y = 420,
    duration_ms = 0,
    long_press = false,
}
```

Pages should consume this structured event rather than depend on native event structs.

The shell receives each normalized native event before Lua. If the shell consumes the event (for
example a reserved long press), Lua `event(ev)` is not called. This guarantees one action per input
and prevents long presses from also triggering dock/page short-press behavior.

## 7. Hot reload contract

Hot reload replaces only the Lua VM/application layer.

Kept alive:

- native application process
- display and input devices
- capability registry
- shared state
- persistent state
- reader session/library cache
- board runtime

Reload sequence:

```text
file/F5/shell.reload()
        |
        v
create candidate Lua VM
        |
load shell + application
        |
call init(true)
        |
    success?
     /   \
   yes    no
    |      |
swap VM   discard candidate
    |      |
redraw    keep old VM alive
```

A broken edited Lua file therefore does not kill the running application.

Application code should use `reload == true` to avoid resetting durable/shared state during a reload.

## 8. Adding a new native binding

When adding a binding, follow this sequence:

1. Decide whether the operation is framework/domain functionality or product policy.
2. Put framework/domain functionality in `core/`, `reader/`, or another reusable native module.
3. Add only a thin conversion function in `runtime/lua/runtime.hpp`.
4. Register it under the smallest relevant `shell.<module>` table.
5. Add a pure-Lua wrapper only when it improves ergonomics.
6. Add a regression test.
7. Do not register ebook-specific routes, menu entries, or page decisions from the runtime layer.

A native bridge should look like:

```cpp
inline int l_reader_next(lua::api::state* vm) {
  auto& host = bound(vm);
  reader::next_page(*host.app->memory);
  ++host.app->invalidations;
  return 0;
}
```

The binding should translate arguments/results; it should not reimplement application behavior.

## 9. Binding design rules

- Prefer tables/options over long positional APIs at the Lua convenience layer.
- Keep the native ABI small and stable.
- Do not expose raw C++ pointers to application Lua.
- Keep asynchronous/device callbacks out of direct Lua execution; enter Lua from the application loop.
- Keep page routing and menu registration at app level.
- Keep normal drawing through native widgets; direct `shell.api.*` drawing is the low-level escape hatch.
- Keep language-neutral services outside `app/lua`.
- Preserve C++/Lua visual parity when Lua style overrides are absent.
- Treat hot reload as a first-class development path.

## 10. Where to change what

| Goal | Change here |
| --- | --- |
| Add a product page | `app/cpp/pages/` and/or `app/lua/pages/` |
| Wire a URI | `app/<language>/routes.*` |
| Change app behavior | `app/<language>/main.*`, `model.*`, or page module |
| Add a reusable widget | `core/widgets/` + Lua wrapper in `runtime/lua/shell/widget.lua` |
| Add a native Lua capability | `runtime/lua/runtime.hpp` |
| Add Lua-only ergonomic helper | `runtime/lua/shell/` |
| Add translation | `lang/<locale>.txt` |
| Change router mechanics | `core/router.hpp` and corresponding binding behavior |
| Add a new language | `runtime/<language>/` + `app/<language>/` |

This separation is the contract to preserve as the framework grows.

## Plugins

Lua plugins are application features, not new binding primitives. Their conventional entry point is `plugins/<name>/lua/init.lua`. The build generates `build/generated/enabled_plugins.lua`; `app/lua/main.lua` invokes it after base route registration. Plugin Lua uses the same `shell` binding API as `app/lua`, and plugin files participate in simulator hot reload. See `docs/PLUGINS.md`.
