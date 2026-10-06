# Build-time plugin system

## Goal

Plugins are reusable application features selected at build time. They are discovered by directory convention and linked into the final image; there is no dynamic loading and no runtime filesystem scan.

Adding a plugin must not require editing `app/`, `core/`, `runtime/`, or a central plugin registry.

## Directory contract

```text
plugins/<name>/
├── Kconfig                 # optional build configuration/dependencies
├── cpp/
│   └── init.hpp            # C++ app implementation entry point
└── lua/
    └── init.lua            # Lua app implementation entry point
```

Only the entry point names are framework conventions. A plugin is free to add its own `pages/`, `routes.*`, `service.*`, widgets, models, tests, and other private files below its directory.

## C++ entry point

```cpp
#pragma once

namespace plugins::example {

inline bool init(app::context& ctx) {
  // Register plugin routes/services/pages here.
  return true;
}

}  // namespace plugins::example
```

The generated registry includes the header and calls `plugins::<name>::init(ctx)`.

## Lua entry point

```lua
local M = {}

function M.init()
    -- Register plugin routes/services/pages here.
    return true
end

return M
```

The generated registry requires `plugins.<name>.lua.init` and invokes `init()`.

## Selecting plugins

Persistent build configuration uses `.config`:

```text
CONFIG_PLUGIN_WIFI=y
CONFIG_PLUGIN_OTA=y
```

For development, plugins can also be enabled directly:

```sh
make APP=lua PLUGINS="wifi ota"
```

The plugin registry is regenerated when `.config`, plugin files, or the `PLUGINS` value changes.

## Kconfig discovery

Each plugin may provide its own `Kconfig` file. `scripts/gen_plugins.py` discovers these files and generates:

```text
build/generated/plugins.Kconfig
```

The top-level `Kconfig` sources that generated catalog. No central Kconfig source list needs to be maintained.

A future Wi-Fi plugin can therefore express board dependencies locally, for example:

```text
config PLUGIN_WIFI
    bool "Wi-Fi"
    depends on BOARD_HAS_WIFI
```

## Generated files

The build produces:

```text
build/generated/enabled_plugins.hpp
build/generated/enabled_plugins.lua
build/generated/plugins.Kconfig
build/generated/plugins.mk
```

Application initialization knows only about these generated registries. It never contains plugin names.

## Initialization order

Native:

```text
board initialization
    -> app core initialization
    -> app route registration
    -> generated C++ plugin init
    -> first page/render
```

Lua:

```text
native app/domain initialization
    -> create Lua VM
    -> app/lua/routes.lua
    -> generated Lua plugin init
    -> restore route
    -> render
```

A plugin may register additional routes during `init()`.

## Hot reload

Lua plugin sources below `plugins/` are part of the simulator hot-reload fingerprint. Editing an enabled Lua plugin causes the same transactional VM reload used for application Lua code.

## Adding a plugin

The intended workflow is only:

```text
1. create plugins/foo/
2. add plugins/foo/Kconfig (if configuration is needed)
3. add cpp/init.hpp and/or lua/init.lua
4. enable CONFIG_PLUGIN_FOO=y (or PLUGINS=foo)
5. build
```

No central source registration step is required.

## Responsibility boundary

```text
boards/       hardware implementation/capabilities
core/         generic framework
runtime/      language bindings
app/          base product application
plugins/      optional reusable product features
```

Board-specific hardware should not be implemented in a plugin. A plugin consumes capabilities exposed by the selected board. For example, the future Wi-Fi plugin will consume the generic Wi-Fi capability instead of depending on M5Paper directly.

## Wi-Fi migration example

The Wi-Fi feature is now the first real plugin using this mechanism:

```text
plugins/wifi/
├── Kconfig
├── cpp/
│   ├── init.hpp       # route/menu registration
│   ├── service.hpp    # capability-backed Wi-Fi operations
│   ├── page.hpp       # native plugin page
│   └── ui.hpp         # optional native UI dispatch hooks
└── lua/
    ├── init.lua       # route/menu registration
    └── pages/
        └── wifi.lua   # Lua plugin page
```

The board still owns only the generic `wifi::device` capability. The plugin consumes that capability and contributes `/settings/network` plus its Settings menu entry.

Native plugins that own pages may additionally provide `cpp/ui.hpp` with:

```cpp
bool render(app::context&);
bool event(app::context&, const event::value&);
void tick(app::context&, std::uint32_t);
```

The generated registry dispatches these hooks automatically. Plugins without UI do not need `ui.hpp`.

Lua optional hardware bindings are exposed outside `shell`. The Wi-Fi plugin uses:

```lua
local wifi = require("modules.wifi")
```

This keeps `shell` limited to generic application-framework APIs.

## Header include convention

Project headers are included from the repository root. The top-level Makefile owns the include search path through `CPPFLAGS`:

```make
CPPFLAGS += -I$(PROJECT_ROOT) -I$(PROJECT_ROOT)/$(BUILD) -I$(PROJECT_ROOT)/$(GENERATED)
```

Use:

```cpp
#include "core/router.hpp"
#include "app/cpp/model.hpp"
#include "plugins/wifi/cpp/pages/wifi.hpp"
```

Do not use `../` or `../../` traversal in project includes. This keeps plugins movable and prevents include paths from depending on directory depth.

## Optional plugin guards

Every optional native plugin header must be safe when the plugin is disabled:

```cpp
#pragma once

#if defined(CONFIG_PLUGIN_FOO)
// plugin implementation
#endif
```

`scripts/gen_plugins.py` generates `build/generated/plugin_config.hpp` from the selected plugin set. The build force-includes that file for every C++ translation unit and the generated plugin registry also includes it explicitly.

Disabled plugins therefore are not referenced by the generated registry, and an accidental direct include remains harmless.

## Plugin structure

Plugins integrate through routes and own their pages. There is no plugin `ui.hpp` dispatch layer.

```text
plugins/foo/
├── Kconfig
├── cpp/
│   ├── init.hpp
│   ├── routes.hpp
│   ├── service.hpp        # optional
│   └── pages/
│       └── foo.hpp
└── lua/
    ├── init.lua
    ├── routes.lua         # optional; may be folded into init.lua
    └── pages/
        └── foo.lua
```

The C++ `init.hpp` is the only entry point known by the generated registry. The plugin can register its routes from there. A route can bind a plugin-owned page into the normal application page lifecycle.
