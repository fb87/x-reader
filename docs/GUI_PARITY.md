# GUI Parity Contract

The current workspace GUI is the visual source of truth during adoption of
`reader-wayland-simulator-final.zip`. The ZIP's widgets and Lua frontend must reproduce this GUI;
the migration is not an opportunity to redesign it.

The Wayland simulator is the primary development target. Every visual and interaction change must
be exercised there first, where deterministic framebuffer captures can be compared. M5Paper is not
the first place to diagnose GUI layout or behavior regressions.

## Visual Source Of Truth

The baseline is defined by the current native C++ simulator and its framebuffer output at 540x960
GRAY4. Capture deterministic checkpoints for:

- Splash/loading.
- Home and Continue Reading card.
- Library with short and long titles.
- Favorites.
- File Manager with nested paths.
- Book Info with title, author, pages, size, progress, and buttons.
- Reader cover/index page.
- Reader text page.
- Reader status bar and bottom dock.
- Settings and connectivity.
- Sleep.
- Dialogs in focused and unfocused states.

Each checkpoint must record the input sequence, selected focus, refresh intent, dirty rectangle, and
resulting framebuffer.

## Geometry Contract

Preserve exactly unless the baseline is intentionally updated first:

- logical display: 540x960 portrait;
- status-bar height and bottom dock height;
- page margins and content rectangles;
- list row height, index column, cover size, and scrollbar geometry;
- dialog width, padding, title-bar height, and button placement;
- reader text rectangle, footer rectangle, and progress line;
- icon bounds and alignment;
- font face, font size, line height, ellipsis, and UTF-8 behavior.

## Interaction Contract

Native C++ and Lua must produce the same result for the same normalized input sequence:

- pointer tap;
- rotary Up/Down;
- rotary push;
- long Up/Down;
- Left/Right;
- Back/Escape;
- Menu;
- wheel rotary input;
- dock focus and activation;
- list scrolling and selection;
- reader page turns and chrome toggling.

The Wayland backend may change window/presentation mechanics, but it must not change the semantic
event sequence seen by the shell or application.

## Rendering Ownership

- `core/widgets/` owns reusable rendering primitives.
- `app/cpp/pages/` owns native page composition and policy.
- `app/lua/pages/` owns Lua page composition and policy.
- `runtime/lua/shell/widget.lua` exposes ergonomic wrappers only.
- Native drawing remains the single renderer for C++ and Lua widget primitives.
- Lua must not duplicate list, dialog, status, dock, or reader rendering logic in ad hoc drawing.
- Wayland presents the framebuffer; it does not decide layout or styling.

## Refresh Contract

Refresh intent and visual behavior are separate but both are tested:

- `fast`: focus movement, status updates, and small transient changes;
- `reader_quality`: reader page turns using the reduced-flash grayscale waveform;
- `quality`: list row cleanup, dialogs, settings, and general content changes;
- `full`: deliberate ghosting reset or page navigation.

The migration must preserve:

- no full-screen update for idle status changes;
- no stale selected list row after rapid rotary input;
- no duplicate active row highlight;
- no reader page text outside its rectangle;
- no dialog restoration artifacts;
- full refresh budget behavior.

## Text Contract

Every bounded text region must:

- clip to its rectangle;
- ellipsize single-line titles/authors;
- wrap only where the baseline wraps;
- preserve Vietnamese and extended Latin glyphs;
- avoid drawing into neighboring rows or controls.

Long-title, long-author, UTF-8, and mixed-script cases are mandatory checkpoints.

## Native/Lua Parity

For every native page checkpoint:

1. Run the same input sequence in native C++ mode.
2. Run it in Lua mode.
3. Compare page state, focus state, route, dirty rectangle, refresh intent, and framebuffer.
4. Permit differences only in explicitly documented runtime diagnostics.

Lua hot reload must preserve:

- current route/page;
- selected library item;
- current book and chapter/page;
- reader settings;
- connectivity state;
- framebuffer/runtime board state.

## GUI Acceptance Gates

A GUI migration step is accepted only when:

- native C++ checkpoints match the baseline;
- Lua checkpoints match native C++;
- rapid input does not leave stale highlights;
- long press emits exactly one semantic action;
- list views remain scrollable and bounded;
- reader title/chapter/footer metadata is correct;
- no idle screen flashes outside the status region;
- Wayland GUI builds and launches in a real compositor session;
- headless tests cover the same input path without requiring a display server.

M5Paper migration and flashing are blocked until these simulator parity gates pass for both native
C++ and Lua modes.
