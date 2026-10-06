# Build-time plugins

Plugins are statically discovered by directory convention and enabled at build time. There is no dynamic loading.

A plugin may provide either or both application-language implementations:

```text
plugins/<name>/
├── Kconfig
├── cpp/
│   └── init.hpp
└── lua/
    └── init.lua
```

The only public entry point is `init`:

- C++: `plugins::<name>::init(app_context)` returning `bool`.
- Lua: module `plugins.<name>.lua.init` returning a table with `init()`.

Everything else below the plugin directory is private to that plugin. Routes, pages, services and widgets may be organized however the plugin prefers.

Enable plugins in `.config` with `CONFIG_PLUGIN_<NAME>=y`, or for one-off builds with `make PLUGINS="foo bar"`.

The build scans `plugins/*`, generates the C++ and Lua registries, and initializes the selected implementation automatically. Adding a plugin requires no edit to app/core/runtime source or to a central plugin list.
