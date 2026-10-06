# URI Routing and Feature Extension

The application framework uses URI routes as its integration boundary. Application implementations live under `app/<language>/`; reusable language runtimes live under `runtime/<language>/`.

## Layout

```text
app/
  cpp/
    app.hpp
    model.hpp
    routes.hpp
  lua/
    init.lua
    main.lua
    model.lua
    routes.lua
    pages/
runtime/
  lua/
    api.hpp
    runtime.hpp
    frontend.hpp
    shell/
core/
  router.hpp
  widgets/
```

A future Python implementation should follow the same split:

```text
app/python/
runtime/python/
```

## Routes

Current application routes are:

```text
/splash
/
/library
/library/favorites
/files
/book/:id/reader
/settings
/settings/network
/sleep
```

The router supports fixed-capacity registration, path parameters, query parameters, `push`, `replace`, `back`, current route, and history. It does not allocate dynamically.

Example route resolution:

```text
/book/42/reader?mode=night
```

matches `/book/:id/reader` with `id=42` and query `mode=night`.

## Lua registration

`app/lua/routes.lua` is the application route table. Pages are ordinary modules:

```lua
router.register {
    path = "/library",
    title = "library",
    page = require("pages.library"),
    menu = { section = "home", order = 10, icon = "library" },
}
```

Navigation uses URIs:

```lua
shell.router.push("/library")
shell.router.replace("/settings")
shell.router.back()
```

A page that should not appear in a menu simply omits `menu`.

## Native C++ registration

`app/cpp/routes.hpp` owns the native application's concrete URI-to-page wiring. It registers descriptors into the generic `core/router.hpp` mechanism. The current C++ implementation retains its page enum only as a compatibility detail for durable state/tests.

Native app code navigates through its app-owned route facade:

```cpp
app::routes::push(ctx, "/library");
app::routes::replace(ctx, "/settings");
app::routes::back(ctx);
```

The route facade delegates matching/history mechanics to `core/router.hpp`; it does not move product route definitions into the framework.

## Dynamic menus

Menus are projections of route metadata. Home asks the route registry for entries in the `home` section. It does not need to know which features exist.

This allows optional features to register routes and menu entries without editing Home. Capability gating can be added at the route descriptor layer without changing page code.

## Extension rule

A feature integrates by:

1. implementing its page module,
2. registering one or more routes,
3. optionally publishing menu metadata,
4. consuming shared state/services/widgets.

Routes are the application integration interface; widgets are the presentation interface; state and capabilities are the system integration interface.


## Ownership boundary

The routing layers intentionally have different responsibilities:

```text
core/router.hpp              generic route matching/history mechanism
runtime/lua/shell/router.lua Lua binding/framework-facing router API
app/cpp/routes.hpp           C++ product route table and page wiring
app/lua/routes.lua           Lua product route table and page wiring
```

`runtime/lua/shell/router.lua` must not know which ebook-reader routes exist. Conversely, app route files should not implement URI matching or history themselves. Future languages should follow the same split, for example `runtime/python/...` plus `app/python/routes.py`.
