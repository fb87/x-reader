# Application implementation structure

Each supported application language follows the same conceptual layout.

```text
app/<language>/
├── init.*
├── main.*
├── model.*
├── routes.*
└── pages/
```

Current implementations:

```text
app/cpp/                         app/lua/
├── init.hpp                     ├── init.lua
├── main.hpp                     ├── main.lua
├── model.hpp                    ├── model.lua
├── routes.hpp                   ├── routes.lua
└── pages/                       └── pages/
    ├── splash.hpp                   ├── splash.lua
    ├── home.hpp                     ├── home.lua
    ├── library.hpp                  ├── library.lua
    ├── favorites.hpp                ├── favorites.lua
    ├── files.hpp                    ├── files.lua
    ├── reader.hpp                   ├── reader.lua
    ├── settings.hpp                 ├── settings.lua
    ├── connectivity.hpp             ├── connectivity.lua
    └── sleep.hpp                    └── sleep.lua
```

Responsibilities:

- `init`: application bootstrap against framework services.
- `main`: lifecycle dispatch (`render`, `event`, `tick`, `pump`).
- `model`: app state vocabulary and small app-level helpers.
- `routes`: product URI registration/wiring only.
- `pages/<name>`: page-local rendering and behavior.

The app layer consumes reusable framework code; it does not define framework mechanisms.

A new language should preserve these boundaries so a developer can move between implementations without relearning the product structure.

## Optional plugins

Reusable optional features live under `plugins/<name>/`, not under either language's base app tree. The build discovers and initializes enabled plugins automatically. See `docs/PLUGINS.md`.
