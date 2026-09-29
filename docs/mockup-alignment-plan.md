# Mockup Alignment Plan

Reference: `x-reader-ui.png` (10 screen mockups, e-ink grayscale, **portrait**).
Baseline: firmware as verified on M5Paper K049 hardware on 2026-09-29, after the
bug-fix pass and the hardware fixes recorded in `docs/hardware-verification-2026-09-29.md`.

Every phase below ends with a full simulated-touch GUI sweep on hardware. The
harness is described in "Verification" at the end of this document.

## 1. Where we are against the mockup

| # | Mockup screen | Current state | Gap |
|---|---|---|---|
| 1 | Splash | not implemented | new screen |
| 2 | Home / Library | two screens: a 7-row launcher **and** a book list | mockup merges them; needs folders, covers, sizes |
| 3 | File Browser | exists, plain rows | breadcrumb, folder/file icons, item counts, sizes |
| 4 | Book Info | title/author/chapter text only | cover, metadata table, description, CTA button |
| 5 | Reader | close match | progress bar, icon footer, title+author line |
| 6 | Reader Menu | overlay panel, settings mixed into the list | full-screen list, icons, chevrons, split settings, Go to Page, Exit to Library |
| 7 | Table of Contents | exists | per-chapter page numbers |
| 8 | Bookmarks | exists, session-only | icon, date, page number, persistence |
| 9 | Display Settings | folded into one flat Settings list | new screen + slider/toggle widgets |
| 10 | Reading Settings | folded into one flat Settings list | new screen + slider/segmented widgets |

Cross-cutting gaps:

- **Orientation.** The mockup is portrait throughout; the device currently boots
  landscape (960x540). Orientation is already a persisted setting, so this is a
  default change plus portrait layout validation.
- **Status bar.** Mockup shows `10:24` + a battery *icon*. Current shows a text
  title plus `SD CARD | BAT 100%`. A clock needs a time source (see the
  "Date/time settings" item in `X-READER-TODO.md`).
- **Icons.** The mockup uses ~18 distinct icons. Current code substitutes single
  ASCII letters (`L`, `R`, `M`, `S`, `*`, `Z`) with a comment saying an icon font
  is not available.
- **Covers.** `services/library_index.cpp` already extracts and caches cover
  images, and `epub/image.cpp` can decode them; nothing renders them yet.
- **Widgets.** Slider, toggle switch, segmented control, progress bar,
  breadcrumb, and filled CTA button do not exist.
- **Typography.** The UI is ALL-CAPS everywhere; the mockup is sentence case.

## 2. Icon font

Recommendation: **Material Symbols** (Google), Apache-2.0.

Rationale and evidence:

- The project forbids third-party runtime libraries, so LVGL's font pipeline is
  out. But the repo already owns a font pipeline — `tools/generate_unicode_font.py`
  rasterises a TTF with Pillow into a packed 1-bpp C table
  (`gfx/unicode_font.cpp`). An icon table is the same pipeline with a different
  source font and codepoint list, so this adds a build-time tool, not a
  dependency.
- Apache-2.0 permits vendoring the generated table with attribution, which suits
  a firmware image. Lucide (ISC) is also permissive but ships SVG-first and is
  stroke-based, which rasterises poorly at 20-24 px in 1 bit.
- Material Symbols ships variable TTFs with a `FILL` axis. At `FILL=100,
  wght=500` the glyphs are solid, which matches the mockup and survives 1-bit
  thresholding.
- Already packaged in nixpkgs as `material-symbols`, so the flake can pin it the
  same way it pins the ESP-IDF toolchain.

Verified on this machine: all 18 icons the mockup needs rasterise non-empty and
legible at 24 px, 1-bit (`menu, bookmark, folder, description, settings, delete,
chevron_right, arrow_back, arrow_upward, light_mode, text_fields, search, book,
battery_full, toggle_on, format_align_left, info, home`).

Cost estimate: 24x24 at 1 bpp is 72 bytes/glyph; ~40 glyphs is under 3 KB of
flash. Two sizes (20 px for rows, 28 px for footers) stays under 6 KB.

Plan:

1. Add `tools/generate_icon_font.py` mirroring the existing generator, emitting
   `icon_glyph_t { uint16_t icon; uint8_t width; uint8_t bitmap[N]; }`.
2. Pin `material-symbols` in `flake.nix`; regenerate is a manual, checked-in step
   exactly like the text font.
3. Add `gfx::draw_icon(framebuffer, x, y, icon_id, scale, value)` next to
   `draw_text`, plus an `icon_t` enum so call sites never name codepoints.
4. Vendor the Apache-2.0 `LICENSE` next to the generated table, as
   `SmoochSans-OFL.txt` is today.

## 3. Phases

Each phase is independently shippable and ends with a hardware sweep.

**Status: phases 0-7 implemented and verified on hardware on 2026-09-30.**
See `docs/hardware-verification-2026-09-29.md` for what each phase changed, the
portrait refresh defect it uncovered, and the three items deliberately left out.

### Phase 0 — Portrait default and portrait layout validation
- Default `orientation` to portrait for new devices; migrate existing NVS.
- Re-run the layout oracle for 540x960 and fix any screen whose rows overflow.
- The bug-fix pass rewrote the IT8951 portrait transform but noted it as
  hardware-unvalidated; this phase is where that gets exercised.
- **Risk:** highest-uncertainty phase. Touch transform and waveform behaviour in
  portrait are still unproven on this panel.

### Phase 1 — Icon font infrastructure
- Steps 1-4 from section 2. No visual change yet beyond replacing the ASCII
  placeholder marks on Home.

### Phase 2 — Chrome: status bar and footer
- Battery icon instead of `BAT 100%` text.
- Clock field (renders `--:--` until a time source exists).
- Icon + label footer cells.
- Sentence case across all labels.

### Phase 3 — Widget kit
- `ui/widgets/`: slider, toggle, segmented control, progress bar, breadcrumb,
  CTA button, chevron.
- Each widget gets a hit-test alongside its draw call, so touch and drawing
  cannot drift — the failure mode that made the footer inert before the bug-fix
  pass.

### Phase 4 — Settings split
- Split the flat 14-row Settings list into **Display Settings** and **Reading
  Settings**, matching mockups 9 and 10.
- Reader Menu becomes a full-screen list with icons and chevrons, adding
  *Go to Page* and *Exit to Library*.
- Keep the existing quick-settings cycling behaviour reachable so no
  functionality is lost mid-migration.

### Phase 5 — Library and covers
- Merge Home and Library per mockup 2, with folders and books in one list.
- Render cached covers as thumbnails; fall back to the current line-art box.
- Show `EPUB • 1.2 MB` metadata line.
- **Decision needed:** the mockup has no separate launcher Home. Book Manager,
  Book Sync, Connectivity, and OTA currently hang off that launcher and would
  need to move under Settings or a menu.

### Phase 6 — Book Info, File Browser, Splash
- Book Info: cover, metadata table, description, `Open Book` CTA.
- File Browser: breadcrumb, icons, item counts, sizes.
- Splash screen during mount and library scan — this also gives the multi-second
  SD scan somewhere to show progress.

### Phase 7 — Reader polish
- Progress bar with `3 / 256` and `Title - Author`.
- Icon footer.
- Per-chapter page numbers in the Table of Contents (needs pagination of
  non-current chapters, or an estimate).

## 4. Out of scope here

These are in `X-READER-TODO.md` and are not UI-alignment work: sync protocol,
OTA hardening, XTeink X4 backend, persistence of bookmarks/progress, EPUB
compatibility. Phase 8 items that the mockup *implies* but does not show —
persisted bookmarks with dates (mockup 8), page counts (mockup 4) — are called
out in their phases above.

## 5. Verification

Harness (host side, built during the hardware verification pass):

- `serial_bridge.py` — background serial bridge; logs the device and forwards
  commands from a FIFO.
- Firmware debug console (`main/services/debug_console.cpp`, compiled only with
  `-DXREADER_DEBUG_CONSOLE=1`) — injects synthetic `input::event_t` into the same
  queue the GT911 and rotary drivers feed, so simulated touch takes the identical
  path as a real finger. Also exposes `state` (which screen and which focused
  row the firmware believes it is on) and `ls`/`put` for SD access.
- `geom.py` — host mirror of `ui/layout/layout.cpp`, so taps are aimed at real
  widget rectangles rather than guessed pixels.
- `drive.py` / `suite_v2.py` — the sweep; asserts the landed screen after every
  tap by querying the firmware, not by reading the log.
- `grab.sh` — captures the panel through the screen camera with focus locked.

Per-phase gate: `suite_v2.py` must pass with no new failures, and `tour.py` must
capture every screen for visual comparison against the mockup.
