# Design Contract

This document is the architectural contract for the refactor of this ebook-reader codebase,
tracked on branch `refactor/cpp-design`, branched from `dev/minimal`.

The goal is not to create a large general-purpose framework. The goal is a small, clean ebook-reader
codebase that can grow in three directions without becoming coupled:

1. reader/application features,
2. target boards and hardware capabilities,
3. application implementation style, including native C++ and Lua.

When implementation and this document disagree, either the implementation must be corrected or this
contract must be intentionally updated first.

## 1. Primary goals

- Preserve the product focus: this is an ebook reader.
- Keep the codebase small enough to understand without a large architecture guide.
- Separate reusable core mechanics from ebook-reader domain logic.
- Keep application code independent from concrete boards.
- Keep board code independent from application flow.
- Make `app_main.cpp` the composition root that wires the selected board to the application.
- Select one board at build time through Make/CMake/Kconfig metadata.
- Make hardware capabilities extensible without growing one monolithic board structure.
- Make the simulator a first-class development target that mirrors M5Paper behavior.
- Make every important feature autonomously testable through simulated user input.
- Keep shared state outside individual pages so page changes and Lua hot reload do not force
  expensive re-evaluation.
- Allow the application layer to be implemented either in restricted C++ or Lua over the same
  native interfaces.
- Keep the framework naming generic; no product-specific prefix belongs in framework APIs.

## 2. Non-goals

The following are explicitly not goals:

- a desktop GUI framework,
- a generic application-service framework,
- runtime dependency injection machinery,
- a plugin registry for every component,
- speculative interfaces for features that do not yet exist,
- modern-C++ object hierarchies,
- a requirement that every board implement every capability,
- making Lua mandatory for the product.

Abstractions are added only where they remove a real dependency or enable a real alternate
implementation such as simulator versus hardware.

## 3. Language model: restricted C++

The implementation uses C++ primarily for namespaces, stronger compile-time structure, and cleaner
interfaces. It intentionally keeps a C-like execution model.

The project principle is:

> C++ syntax, C execution model.

### 3.1 Encouraged features

- `namespace`
- `struct`
- aggregate initialization
- `constexpr`
- `enum class` where stronger typing is useful
- references where ownership is obvious
- small inline functions
- tiny compile-time helpers
- very small templates where they remove unsafe casts or repeated boilerplate

### 3.2 Features to avoid

- inheritance
- virtual functions
- RTTI
- exceptions
- class-oriented architecture
- smart pointers
- dynamic allocation in core code
- STL containers in embedded-facing core/domain code
- heavy template code
- metaprogramming
- hidden static initialization dependencies

Recommended compiler options are:

```text
-fno-exceptions
-fno-rtti
-fno-threadsafe-statics
```

Additional target-specific runtime-removal flags may be added when useful.

## 4. Header-only implementation

Normal component implementation lives in `.hpp` files.

`.cpp` files are reserved for composition and unavoidable platform/SDK entry points, for example:

- `app_main.cpp`,
- an ESP-IDF entry point,
- a host executable entry point,
- a board SDK bridge that cannot reasonably be header-only.

Do not move ordinary component code into `.cpp` files simply to imitate a traditional C++ library.

Keep one focused header per responsibility. Do not create one huge umbrella implementation header.

## 5. Naming and namespaces

Framework names must be generic. Do not use `xr_`, `x_reader`, or similar product prefixes in core
framework APIs.

Use namespaces instead of prefixes.

Preferred:

```cpp
shell::init(...);
shell::dispatch(...);
canvas::fill(...);
reader::next_page(...);
library::scan(...);
```

Avoid:

```cpp
xr_shell_init(...);
xr_canvas_fill_rect(...);
```

Use `snake_case` for:

- functions,
- variables,
- struct members,
- files,
- namespace members,
- state-key components.

Keep namespace hierarchies shallow. Examples:

```cpp
namespace shell {}
namespace canvas {}
namespace state {}
namespace reader {}
namespace library {}
namespace board::sim {}
namespace driver::gt911 {}
```

Avoid redundant type names. Prefer:

```cpp
display::device
shell::context
reader::context
widget::base
```

rather than names such as `display::display_t`.

## 6. Target directory structure

The target structure is intentionally compact:

```text
core/
reader/
app/
boards/
drivers/
lua/
tests/
fonts/
app_main.cpp
```

`fonts/` carries the same role the design contract elsewhere calls `assets/`: generated bitmap
font data (see `tools/gen_font.py`, `tools/generate_icon_font.py`). The name is kept as `fonts/`
rather than renamed to `assets/` because it is a generated artifact directory already wired into
the font-generation tooling; renaming it is cosmetic risk for no behavioral benefit.

Each directory exists because it represents a real dependency boundary.

## 7. `core/`

`core/` contains generic mechanics only.

Typical responsibilities:

- geometry,
- framebuffer/display interface,
- drawing/canvas,
- input events,
- input device interface,
- platform time/battery interface,
- refresh modes/scheduling,
- shell/event dispatch,
- widgets/pages/dialogs when used,
- generic storage interface,
- generic capability registry,
- generic hierarchical state store,
- generic connectivity capability contracts,
- protected secret-store contract.

Core must not know about:

- EPUB,
- books,
- library scanning,
- reading progress semantics,
- M5Paper,
- Xteink,
- ESP-IDF,
- SDL,
- application page names,
- application-specific state keys.

A useful test is that core code should make sense in a non-reader application even though supporting
non-reader products is not a project goal.

## 8. `reader/`

`reader/` contains ebook-reader domain behavior.

Typical responsibilities:

- book metadata,
- library/index cache,
- document discovery,
- EPUB handling,
- pagination,
- current-book session,
- chapter/page position,
- reading progress,
- reader settings.

Reader code may use core interfaces such as storage and state.

Reader code must not include concrete board headers or concrete drivers.

Application pages should call reader operations instead of modifying EPUB/session internals
explicitly.

Preferred:

```cpp
reader::next_page(shared_state);
reader::open_selected(reader_context, shared_state);
```

Avoid page code changing low-level fields such as an EPUB spine index directly.

## 9. `app/`

`app/` contains the user-facing application flow and pages.

The native application retains the product flows from `dev/minimal`, including:

- splash,
- home,
- library,
- favorites,
- file manager,
- reader,
- settings,
- connectivity,
- sleep/wake,
- dialogs/actions as they are migrated.

App code may use `core/` and `reader/` interfaces.

App code must not include:

- `boards/m5paper/*`,
- `boards/sim/*`,
- concrete display/touch drivers,
- ESP-IDF APIs.

The application asks for generic capabilities and behaves sensibly when optional capabilities are
absent.

## 10. `boards/`

A board implementation adapts concrete hardware/platform facilities to generic capability
interfaces.

Examples:

```text
boards/m5paper/
boards/xteink4/
boards/sim/
```

Board code may depend on:

- core interface types,
- reusable drivers,
- the target SDK/platform.

Board code must not depend on:

- app pages,
- app navigation,
- reader-specific workflow.

There is no requirement for a generic `board.hpp` object model.

The board is chosen by build metadata. The selected implementation simply provides the symbols that
`app_main.cpp` composes.

## 11. Composition root

`app_main.cpp` is the only normal layer that knows both:

- the build-selected board runtime,
- the application runtime.

The desired relationship is:

```text
             app_main.cpp
              /       \
             v         v
           app       board
            |          |
            v          v
         reader       drivers
            \          /
             v        v
                core
```

Neither `app/` nor `reader/` performs runtime board selection.

Neither application code nor reader code should contain names such as `m5paper` or `sim`.

## 12. Build-time board selection

Exactly one concrete board implementation is selected by build metadata.

Supported selectors may include:

- `BOARD=sim` in Make,
- CMake target/options,
- Kconfig options on embedded builds.

The composition root should not contain code like:

```cpp
auto display = board::m5paper::display::init();
```

Instead the build selects the implementation and `app_main.cpp` wires the selected implementation
to the app through generic types.

No runtime target probing is required.

## 13. Hardware capability model

Boards may grow capabilities over time. The architecture must not require expanding one giant board
structure whenever a feature is added.

Use small capability-specific contracts plus a stable capability lookup/registry.

Typical capabilities include:

- display,
- input,
- platform/time,
- storage/SD card,
- Wi-Fi,
- Bluetooth,
- power/battery,
- RTC,
- front light,
- USB,
- protected secret storage,
- future board-specific hardware exposed through a generic contract only when the app actually
  needs it.

### 13.1 Mandatory capabilities

Keep mandatory startup capabilities minimal. Normally these are:

- display,
- input,
- basic platform services.

### 13.2 Optional capabilities

Optional capabilities return unavailable/null when unsupported.

A board without Wi-Fi must still boot and operate as an ebook reader.

Adding a new optional capability must not force unrelated existing boards or reader code to change.

A board legitimately reporting an optional capability as unavailable (for example Bluetooth, since
neither M5Paper nor the simulator has real radio hardware) is correct behavior under this rule, not
an implementation gap to apologize for.

## 14. Drivers versus boards

Reusable low-level drivers belong under `drivers/`, for example:

```text
drivers/it8951/
drivers/gt911/
```

A driver knows its hardware protocol, not the ebook reader.

A board combines drivers and board-specific wiring such as:

- pins,
- buses,
- power sequencing,
- display orientation,
- touch coordinate transforms,
- SD-card mounting.

Drivers must not include app or reader headers.

App/reader code must not call drivers directly.

Hardware-verified behavior inside a driver — for example a coordinate remap or timing workaround
discovered only by testing against real silicon — must be preserved exactly when ported, with the
comment documenting the empirical finding carried forward unchanged.

## 15. Storage interface

Reader/library code must not depend directly on:

- `FILE*`,
- `DIR*`,
- FatFS,
- `/sdcard`,
- ESP-IDF VFS,
- host POSIX implementation details.

The board exposes a generic storage capability with operations such as:

- list directory,
- read file range,
- query file size.

The simulator maps this contract to host/in-memory storage. M5Paper maps it to SD/FatFS. Reader code
is unchanged.

## 16. Simulator

The simulator is a first-class board implementation, not a separate application.

Its primary profile mirrors M5Paper behavior closely enough that normal application development can
be done on a workstation.

Required M5Paper-compatible behavior:

- logical display: `540 x 960`,
- pixel format: 4-bit grayscale,
- portrait logical orientation,
- touchscreen input,
- rotary left/right/push input,
- e-ink refresh requests (`fast`, `quality`, `full`),
- battery value,
- deterministic monotonic time,
- storage standing in for SD card.

Interactive mappings may include:

- mouse click -> touchscreen tap,
- mouse wheel -> rotary left/right,
- Up/Down -> rotary left/right,
- Enter/Space -> rotary push.

The Wayland GUI backend is used for interactive development; autonomous tests must not require a display
server.

## 17. Input simulation

The simulator input device provides an injection queue.

Tests must send input through the same path as real hardware:

```text
simulated hardware event
        ->
input::device
        ->
shell/event dispatch
        ->
application/page
        ->
reader/domain operation
        ->
state change
        ->
render/display update
```

Tests must not bypass this path by directly invoking page handlers when they are intended to test
user interaction.

Input simulation must support at least:

- touch tap,
- rotary left,
- rotary right,
- rotary push,
- key/back/menu equivalents,
- deterministic time advancement,
- long press when the production input model uses it.

## 18. Autonomous testing

A feature is not considered complete until the simulator can exercise it autonomously through
simulated user input.

Validation priority:

1. domain/shared-state assertions,
2. framebuffer-region assertions/checksums,
3. full framebuffer image dumps for regression/debugging.

When a visual test fails, the simulator should be able to dump its framebuffer for inspection.

The standard command is:

```sh
make test
```

An optional watched/interactive form is provided as:

```sh
make cpp-gui
```

Minimum regression coverage should include:

- startup/splash,
- home navigation,
- bottom/action navigation as migrated,
- library discovery,
- library selection,
- favorites flow,
- file-manager flow,
- opening a book,
- previous/next page,
- touch page turns,
- reader chrome/menu behavior,
- font-size changes,
- refresh-setting changes,
- progress-bar setting,
- connectivity scan/connect behavior when Wi-Fi exists,
- missing optional capability behavior,
- sleep/wake,
- persistent reading position/settings,
- state observers,
- display refresh delivery.

## 19. Shared hierarchical state

Pages and services share a hierarchical key-value state tree.

State exists independently from individual page objects. Pages must not each hold their own copy of
shared values such as current book, library selection, or Wi-Fi connection state.

Keys use dot-separated namespaces.

Examples:

```text
reader.library.selected
reader.library.count
reader.library.sort
reader.library.last_path

reader.book.current
reader.book.title
reader.book.chapter
reader.book.page
reader.book.progress

reader.settings.font_size
reader.settings.full_refresh_every
reader.settings.show_progress
reader.settings.sleep_timeout_minutes

network.wifi.available
network.wifi.connected
network.wifi.ssid
network.wifi.auto_connect

network.bluetooth.enabled

app.page.current
app.menu.selected
app.dialog.current
app.reader.chrome

system.language
system.timezone
```

### 19.1 State-key rules

- lowercase,
- `snake_case` components,
- `.` separates namespaces,
- top-level roots are domain-oriented,
- key ownership belongs to the domain that defines the value.

Preferred top-level roots include:

```text
reader.*
network.*
storage.*
power.*
system.*
app.*
```

Do not create a generic `ui.*` root.

Use `app.*` for transient application/navigation state. Reader state remains under `reader.*`.

## 20. In-memory and persistent state

Use the same logical state model for two lifetimes:

- in-memory runtime state,
- persistent state that survives process/device restart.

Simulator mapping:

```text
memory      -> RAM fixed-capacity store
persistent  -> host file
```

Embedded mapping may be:

```text
memory      -> RAM fixed-capacity store
persistent  -> NVS or filesystem
```

The application should not depend on the concrete persistent backend.

Only values that need durability should be checkpointed, for example:

- current book,
- current reading page/chapter,
- progress,
- reader settings,
- last library selection if useful,
- non-secret Wi-Fi metadata.

Transient page objects must not be the only owner of durable state.

## 21. State observation

The state store supports observing an exact key or hierarchical prefix.

Example observer:

```text
reader.book
```

may receive changes to:

```text
reader.book.current
reader.book.page
reader.book.progress
```

This supports lightweight reactive behavior:

```text
service/domain update
      ->
shared state change
      ->
observer notification
      ->
minimal invalidation
```

Do not add a large reactive framework unless the small observer model proves insufficient.

## 22. State versus cache

Do not put all application data into the scalar state tree.

State is for small shared values such as:

- IDs,
- indexes,
- booleans,
- progress,
- settings,
- navigation state.

Cache/domain objects are for larger rebuildable data such as:

- library book records,
- EPUB manifest/spine metadata,
- extracted chapter text,
- Wi-Fi scan result arrays,
- cover data.

The purpose is to avoid re-scanning/re-parsing every time a page is entered while also avoiding
turning the KV store into a database.

## 23. Credentials and secrets

Wi-Fi credentials or other secrets must not be stored as ordinary debug-visible persistent state.

Use a separate secret-storage capability where supported, implemented for real on any board that
has a credential flow — a declared-but-unimplemented secret contract is not an acceptable final
state for a board that actually stores credentials.

Normal state may contain:

```text
network.wifi.ssid
network.wifi.auto_connect
network.wifi.connected
```

Passwords/credentials belong in the protected secret store.

## 24. Lua application mode

Lua is an optional alternative way to implement application pages and flow, and is out of scope
for this migration pass. The interfaces below are designed so Lua can be added later without
restructuring the ported reader; no bindings are implemented now.

The relationship is:

```text
Lua application
      ->
small Lua binding
      ->
reader + core
```

Native C++ app mode remains supported.

Lua and native C++ should consume the same conceptual APIs for:

- page/widget operations,
- reader operations,
- library operations,
- state access,
- storage access,
- generic connectivity features.

Do not expose to Lua:

```text
board.m5paper.*
driver.gt911.*
driver.it8951.*
```

A Lua application should run unchanged on simulator and hardware as long as required capabilities
exist.

## 25. Lua hot reload

Out of scope for this migration pass; see §24. Recorded here only so a future Lua effort has a
target shape:

```text
shared state remains alive
      ->
destroy disposable Lua page/module instances
      ->
reload changed Lua modules
      ->
recreate pages
      ->
restore visible state from shared state/domain caches
```

Important state such as current book, reading position, library selection, settings, and
connectivity state must live outside disposable Lua page instances.

## 26. Native/Lua parity

Not applicable until §24/§25 are implemented. Recorded here only so native interfaces are designed
without closing off this possibility later.

## 27. Formatting

Use `clang-format` for all C++ code and base the repository configuration on the
**Google C++ Style Guide**. The checked-in `.clang-format` file is the formatting
source of truth. Avoid local style overrides unless this document is updated first.

Formatting is mechanical. Do not manually maintain a competing style.

## 28. Doxygen documentation

Every public:

- struct,
- enum,
- alias/interface callback,
- function,
- capability contract

must have a Doxygen comment.

Internal helpers should also have a concise Doxygen/brief description.

Comments explain:

- intent,
- ownership,
- lifetime,
- hardware constraints,
- non-obvious behavior,
- why a design choice exists.

Avoid comments that simply restate code.

Preferred:

```cpp
// IT8951 partial updates require 4-pixel horizontal alignment.
```

Avoid:

```cpp
// Increment x.
```

## 29. Clean-code rules

- one responsibility per header/component,
- small focused functions,
- no dead code,
- no unused abstraction,
- no speculative framework layers,
- no hidden board dependency,
- no app dependency from board code,
- no direct driver access from app/reader code,
- no dynamic allocation in core unless explicitly justified,
- explicit ownership/lifetime where non-obvious,
- preserve deterministic fixed-capacity behavior where reasonable for embedded targets.

## 30. Refactoring strategy from `dev/minimal`

Refactoring is incremental and behavior-preserving.

`dev/minimal` remains the behavioral reference until a migrated feature is covered by tests. This
work happens on `refactor/cpp-design`, branched from `dev/minimal`, not on `dev/minimal` itself.

Order (see `docs/MIGRATION.md` for current status against this order):

1. Freeze this design contract.
2. Add formatting and documentation rules.
3. Introduce generic C++ core headers and remove `xr_` API naming.
4. Move ebook-specific EPUB/library/session behavior into `reader/`.
5. Split reusable hardware drivers from board wiring.
6. Make board choice a build-time concern.
7. Make the simulator implement the M5Paper logical profile.
8. Add input injection and autonomous smoke/regression tests.
9. Introduce shared hierarchical state.
10. Move page-owned shared values into state/domain caches.
11. Add storage and optional capability contracts as required by real features.
12. Port M5Paper to the same contracts and verify on ESP-IDF hardware.
13. Port Xteink to the same contracts (simulator-only profile for now; no real Xteink hardware
    driver exists anywhere today).
14. Stabilize native interfaces.
15. Retire the old `include/xr/`, `src/`, `port/sim/`, `port/sdl/` tree once parity is proven.

Lua binding/runtime and hot reload (§24/§25) are explicitly out of scope for this pass.

## 31. Migration mapping from `dev/minimal`

The `dev/minimal` modules map conceptually as follows:

```text
include/xr/xr_types.h       -> core/geometry.hpp, core/refresh.hpp
include/xr/xr_hal.h         -> core/display.hpp, core/input.hpp, core/platform.hpp, core/event.hpp
include/xr/xr_storage.h     -> core/storage.hpp
include/xr/xr_canvas.h      -> core/canvas.hpp
include/xr/xr_text.h        -> core/text.hpp
include/xr/xr_widget.h      -> core/widgets/
include/xr/xr_page.h        -> app/cpp/pages/ and app/lua/pages/
include/xr/xr_dialog.h      -> core/widgets/dialog.hpp
include/xr/xr_shell.h       -> core/shell.hpp
include/xr/xr_refresh.h     -> core/refresh.hpp
include/xr/xr_epub.h        -> reader/epub.hpp
src/xr_epub.c               -> reader/epub.hpp

app/app.c                   -> reader/book.hpp + app composition/state usage
app/book_title.c            -> reader/book.hpp (Vietnamese mojibake title repair)
app/page_home.c             -> app/{cpp,lua}/pages/home
app/page_library.c          -> app/{cpp,lua}/pages/library + reader/library.hpp
app/page_file_manager.c     -> app/cpp/pages/files.hpp using core/storage.hpp
app/page_reader.c           -> app/{cpp,lua}/pages/reader + reader/session.hpp
app/page_settings.c         -> app/{cpp,lua}/pages/settings + shared state/capabilities
app/page_sleep.c            -> app/{cpp,lua}/pages/sleep, wired to M5Paper deep sleep
app/page_splash.c           -> app/{cpp,lua}/pages/splash
app/dlg_book_info.c         -> app/cpp/pages/library.hpp + core/widgets/dialog.hpp

port/m5paper/*              -> boards/m5paper/ + drivers/it8951/, drivers/gt911/, drivers/inflate/
port/sim/*, port/sdl/*      -> boards/sim/
```

The mapping is architectural rather than a requirement to preserve old filenames one-for-one.

## 32. Completion criteria

A migrated feature is complete when:

- it follows the dependency rules in this document,
- its public APIs are documented,
- it is formatted,
- it builds under the supported compiler configuration,
- simulator behavior is covered by autonomous input-driven tests,
- optional capability absence is handled where relevant,
- persistent/shared state behavior is covered where relevant,
- no old product-prefixed framework API is required by the migrated path.

The hardware port is complete only after the same reader/application behavior is validated on real
hardware in addition to the simulator. This sandbox has no ESP-IDF toolchain available, so the
hardware-board phase of this migration produces code and build wiring only — it is explicitly not
claimed as hardware-verified until that verification happens on real hardware. See
`docs/MIGRATION.md` for the current, honestly-stated status.
