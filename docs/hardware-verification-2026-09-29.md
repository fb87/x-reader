# Hardware GUI verification — 2026-09-29

Target: M5Paper K049 on `/dev/ttyACM1` (CH9102, 1a86:55d4), 960x540 landscape,
IT8951E + GT911, 32 GB SD card with 27 EPUBs.
Screen captured through a C922 camera on `/dev/video5`, focus locked at 60.

All navigation below was driven by **simulated touch**: the debug console
enqueues `input::event_t` values into the same FreeRTOS queue the GT911 driver
feeds, so injected taps take the identical path through `input::map_event()` and
`ui::dispatch()` as a real finger.

## Defects found and fixed

### 1. Whole UI rendered inverted (`drivers/it8951e/it8951e.cpp`)
`write_pixel_data()` inverted every nibble on upload (`~pixels[...]`). IT8951
4-bpp uses 0x0 black / 0xF white, which is already the UI's convention
(`0x0f` paper, `0x00` ink), so the panel showed white text on a black page.
Replaced the inversion with a straight `memcpy`.
**Evidence:** before/after captures of the Home screen.

### 2. No EPUB was ever discoverable (`sdkconfig.defaults`)
`CONFIG_FATFS_LFN_NONE` was set, so FATFS exposed only 8.3 aliases. An 8.3
extension is at most three characters, so every `book.epub` appeared as
`BOOK~1.EPU` and `ends_with_epub()` — which requires a five-character `.epub`
suffix — could never match. The device logged *"no valid EPUB book available"*
with 27 books present, and fell into the reduced empty-library event loop where
most screens did nothing. Proof from the device: `state.json` listed as
`STATE~1.JSO`.

Enabled `CONFIG_FATFS_LFN_HEAP`, `CONFIG_FATFS_MAX_LFN=255` and
`CONFIG_FATFS_API_ENCODING_UTF_8`. Codepage stays fixed at 437;
`CONFIG_FATFS_CODEPAGE_DYNAMIC` links every OEM table and overflowed the app
partition by 0x3f520 bytes.
**Result:** library index built (23 KB), Vietnamese titles and authors read
correctly.

### 3. Reader Menu rows were inert under touch (`main/ui/screen.cpp`)
The `screen_quick_settings` pointer branch returned `screen_command_edit_setting`
for *every* row, so Contents, Bookmarks, Book Info, Add Bookmark and Search only
worked from a physical button. Factored the activation into
`activate_quick_setting()` and called it from both the pointer and select paths.

### 4. Reader Menu showed the wrong footer (`main/ui/quick_settings.cpp`)
The menu is an overlay that paints only its own panel, so the reader's
`< PREV | MENU | NEXT >` footer stayed on screen while the footer already
dispatched select/back for that screen. Now repaints it as
`MOVE | SELECT | CLOSE`.

### 5. Focused Reader Menu row was unreadable (`main/ui/quick_settings.cpp`)
The focused row was filled black (`0x00`) while its label was also drawn black,
so the focused entry rendered as a blank bar. Other screens had already moved to
the light-gray (`0x0d`) focus wash; the Reader Menu had not.

### 6. Book Sync SERVER row did nothing (`main/main.cpp`, `main/ui/screen.cpp`)
`perform_service_action()` handled `book_sync_now`, `_books` and `_progress` but
had no case for `book_sync_server`, so the row emitted a command that nothing
consumed. It now opens the same editor the Connectivity screen uses;
`return_screen` records the origin so the keyboard returns to Book Sync or
Connectivity correctly on both submit and cancel.

## Open issue, not fixed

**Long operations block the input loop.** Book Manager *Import* and *Cleanup*
run a full SD rescan synchronously on the UI task. During the rescan the event
queue is not serviced and taps are dropped — reproducibly three checks in the
sweep, which pass when run in isolation. This is the
"watchdog-safe long operations" item in `X-READER-TODO.md`, and it is the
remaining cause of "the screen does not react" reports.

## Result

Full sweep: every screen reachable and every documented transition correct by
simulated touch, apart from the blocking-rescan artifact above.

Screens exercised: Home, Library, Library Details, Book Actions, Book Manager,
File Browser, Storage, Book Sync, Settings, Connectivity, Wi-Fi Networks,
Keyboard, OTA, About, Reader, Reader Menu, Contents, Bookmarks, Book Info,
Dialog.

---

# Mockup alignment, phases 0-7 — 2026-09-30

Device now runs **portrait 540x960** by default.

## Further defect found on hardware

### 7. Portrait partial refresh updated the wrong band (`drivers/it8951e/it8951e.cpp`)
The portrait branch of `refresh()` mirrored the area
(`native_x = width - y - height`). A full-screen refresh covers the whole panel
either way, so boot and screen switches looked correct; but any *partial* update
refreshed a band at the opposite end of the display. The visible symptom was a
screen showing rows from the previous screen mixed with rows from the new one —
for example Settings rows 0-4 still on screen under a "Reading Settings" title.

Measured on hardware: with `rotation=1` the controller maps logical y directly
onto native x, so the correct transform is a plain transpose
(`target_x = y, target_y = x, target_width = height, target_height = width`).

This one only reproduces in portrait with a partial dirty rect, which is why it
survived the earlier landscape sweep.

## What each phase changed

- **0 Portrait** — NVS schema v2 defaults to portrait and drops a stored v1
  landscape value once. `layout::stacked_row()` added: `layout::row()` centres a
  short list in a tall area, which stranded lists mid-screen in portrait.
- **1 Icons** — `tools/generate_icon_font.py` rasterises Material Symbols
  (Apache-2.0, pinned in `flake.nix`) into `gfx/icon_font.cpp`; `gfx::draw_icon`
  renders them. 40 icons, ~3 KB.
- **2 Chrome** — status bar is clock + centred title + drawn battery; the footer
  carries icon+label cells; labels moved to sentence case.
- **3 Widgets** — `ui/widgets.{hpp,cpp}`: slider, toggle, segmented control,
  progress bar, breadcrumb, CTA button. Every widget ships its hit-test beside
  its draw call.
- **4 Settings split** — Settings became a hub; Display Settings and Reading
  Settings are their own screens built on a shared `ui/settings_panel`. The
  Reader Menu is a full-screen list with Add bookmark, Search, and Exit to
  Library. Book Manager and Book Sync moved under Settings.
- **5 Library** — header row ("Library / SD Card, sort"), rows sized to fill the
  portrait screen (13 visible instead of 6), and an `EPUB  <size>` line.
- **6 Book Info / File Browser** — Book Info gained a cover slot, metadata table
  and an "Open Book" call to action; File Browser gained a breadcrumb and
  folder/file icons.
- **7 Reader** — progress bar with `page / total` and a title-author line above
  the action bar; the text area reserves that strip.

## Deliberately not implemented

- **Cover thumbnails in the library list.** `library_index` caches the cover
  still *encoded*; `epub::image::decode_mono` would have to run once per visible
  row on every redraw, which costs more than the ~300 ms e-paper refresh itself.
  Book Info shows a cover slot, and decoding a single cover there is the natural
  next step.
- **Brightness and contrast sliders** (mockup 9). The M5Paper has no frontlight
  and no contrast control; a slider that moves nothing is worse than no slider.
- **Splash screen** (mockup 1). Worth doing together with making the SD scan
  asynchronous — see the open issue below — rather than as decoration.

## Still open

**Long operations block the input loop.** Book Manager *Import* and *Cleanup*
rescan the card synchronously on the UI task, so taps are dropped for several
seconds. This is the "watchdog-safe long operations" item in `X-READER-TODO.md`.

---

# Follow-up: blocking scan, splash, cover decode — 2026-09-30

## Bug found and fixed

### 8. Book Manager Import/Cleanup crashed the device (interrupt WDT panic)
Moving the catalog rebuild to a background task (`services::library_scan`) to
stop it from blocking input, as flagged in the "Still open" section above,
initially **crashed the device** rather than fixing it: `Guru Meditation
Error: Core 0 panic'ed (Interrupt wdt timeout)`. Decoded with `addr2line`, the
backtrace was the new task inside `zip::open() -> fopen() -> vfs_fat_open() ->
_lock_acquire`, stuck spinning on a newlib lock.

Root cause: stack size, not concurrency. `library_index::rebuild()` runs the
same EPUB zip/XML metadata parser that boots on the main app task's 64 KB
stack (`CONFIG_ESP_MAIN_TASK_STACK_SIZE`). The new background task was given
only 8 KB — plenty for simple I/O, not for this parser. The overflow
corrupted adjacent memory (apparently including a libc lock structure) before
FreeRTOS's canary check could catch it, which is what made it look like an
SD/SPI concurrency bug rather than what it was. Giving the task 65536 bytes
(matching the main task) fixed it outright; verified over multiple Import and
Cleanup runs with no crash and rotary input actively moving focus *during* the
scan (confirming input isn't blocked, not just that the device doesn't crash).

## What shipped

- **Async library scan** (`services/library_scan.{hpp,cpp}`): Import and
  Cleanup now rebuild the catalog on a background task. Book Manager shows
  "SCANNING..." on both rows while it runs and polls for completion every
  500 ms like Connectivity/OTA already did, only triggering an e-paper
  refresh when the busy state actually changes (not every poll tick).
- **Splash screen** (`ui/splash.{hpp,cpp}`, mockup 1): shown from display
  init through SD mount to catalog load, replacing what used to be a
  placeholder Home paint (nullptr book title) that got overwritten a moment
  later anyway. Confirmed via serial log dirty-rect sizes (full GC16 paint,
  two small DU status-line updates, final full GC16 repaint into Home) and
  two hardware photos catching it mid- and post-render.
- **Book Info cover decode**: `main.cpp` now reads the cached cover file and
  decodes it via the existing `epub::image::decode_mono`/`blit_4bpp_scaled`
  (both already used and host-tested elsewhere), letterboxed to preserve
  aspect ratio. `ui/book_info.cpp` stays a pure drawing function — it takes
  already-decoded pixels, no file I/O in the UI layer.

## Verification gap

None of the 32 EPUBs on the test SD card have an embedded cover image
(`.xreader-covers` cache directory doesn't exist), so the actual decode path
could not be exercised end-to-end on hardware with real data. What *is*
verified: the underlying primitives (`decode_mono`, `blit_4bpp_scaled`) have
existing host-test coverage with a synthetic PNG; the fallback path (no
cover → placeholder icon, the case 100% of current books hit) was confirmed
crash-free on hardware; and the full touch-navigation sweep passed with this
code active. The aspect-fit and file-loading glue in `load_book_cover()` is
new and only indirectly exercised.

Full sweep after all three fixes: `ALL CHECKS PASSED`.

---

# Real-touch regression: inverted portrait Y-axis — 2026-09-30

## Bug

User report: "GUI is frozen, touch but no reaction." Real touch was reaching
the driver fine (`input: emit touch_down/up` logged normally) and dispatch
correctly returned `screen_command_none` for every one of them — the screen
genuinely had nothing at those coordinates.

Root cause: `main.cpp`'s portrait pointer remap mirrored the Y axis
(`action_event.y = display_config.width - 1 - physical_x`). This line was
called out as hardware-unvalidated when portrait was made the default in
Phase 0, and it stayed unvalidated through every subsequent sweep, because
the touch-injection harness used for every "ALL CHECKS PASSED" result in this
document computes its synthetic raw coordinates as the *exact inverse* of
this same formula. That setup proves `ui::dispatch()` and the layout math are
internally consistent; it can never exercise the actual raw-touch transform,
because it never puts a real finger through the GT911 sensor.

## Diagnosis

Pulled the 13 raw `(physical_x, physical_y)` touch events logged during the
user's session and checked them against Home's row/footer rects under both
formulas:

- Current formula (`y = width - 1 - physical_x`): **0 of 13** landed on
  anything.
- Proposed formula (`y = physical_x`, a plain transpose — matching the
  relationship the IT8951 portrait *refresh* transform already uses, fixed
  earlier in Phase 0): **11 of 13** landed cleanly inside a row or the
  footer; the other two were a few pixels short of a row boundary, consistent
  with ordinary finger imprecision rather than a miss.

## Fix

Removed the Y-mirroring at both pointer-remap sites in `main.cpp`; the
transform is now a plain transpose (`x' = physical_y`, `y' = physical_x`),
matching the IT8951 refresh-area transform's own rotation convention. Updated
the test harness's `tap_at()` to the same corrected formula.

Verified: full synthetic sweep still `ALL CHECKS PASSED` (self-consistency,
now against the corrected formula), release build unaffected (0x143670 bytes,
no meaningful size change), and — the test that actually matters — **the
user confirmed real-finger touch works after reflashing.**

## Lesson

A synthetic input harness that computes its own inputs as the inverse of the
code under test can validate everything the code does *after* that
transform, but is structurally blind to bugs *in* the transform itself. Any
future change to the raw touch/rotation pipeline needs a real-touch check,
not just a green sweep.
