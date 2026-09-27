# GUI Design

## Goals

The GUI is a low-refresh, e-ink-first interface for the M5Paper K049. It must
be usable with either the rotary controls or the touch panel. Every visible
touch action has an equivalent logical action that can be triggered by the
physical controls.

The initial logical display geometry is 960 x 540. The GUI must use board
capabilities for dimensions and must not hard-code physical GPIO or controller
details.

## Visual Language

- Use a quiet editorial reading style: strong hierarchy, generous whitespace,
  thin rules, and restrained grayscale.
- Prefer paper-like backgrounds and dark ink content once display polarity is
  confirmed on hardware.
- Avoid animation, gradients, shadows, and dense icon grids.
- Show focus and pressed states with inversion, borders, or patterns that are
  visible after an e-ink refresh.
- Use large touch targets suitable for the display and rotary focus model.
- Keep screen updates bounded to the changed content and chrome regions where
  the display driver permits it.

## Shared Chrome

All primary screens use a common shell:

```text
Status bar
----------------------------------------------
Screen content
----------------------------------------------
Indication bar
```

### Status Bar

The top status bar is shared by Home, Library, Reading, and Settings. It may
show:

- RTC date and time.
- Battery level and charging state.
- SD-card availability.
- Loading, sleep, or refresh status when relevant.
- The current section title when it improves orientation.

Unavailable hardware values are omitted or rendered as a neutral placeholder.
The status bar should have a stable height and should not reflow screen content
when an optional value is unavailable.

### Indication Bar

The bottom indication bar is context-sensitive:

- Home: focused launch action, recent-book summary, or storage status.
- Library: focused book, book count, and sorting/filter state.
- Reading: book title, chapter, current page/total pages, and progress.
- Settings: focused setting and current value.

The Reading indication bar also contains visible action buttons:

```text
Previous       page / progress        Next
```

The Previous and Next regions mirror the physical left and right controls.
The center region opens Quick Settings.

## Screens

### Home

Home is the application launcher and startup screen. It is not replaced by the
Library screen.

The main content contains a prominent Continue Reading card followed by a
focused launch menu:

- Continue Reading.
- Library.
- Recent Books.
- Settings.
- Bookmarks when implemented.
- Sleep.
- Hardware Test in development builds only.

Rotary left/right moves the focused menu item and rotary press activates it.
Touching a card or row activates it directly.

### Library

Library displays discovered EPUB files as bounded, focusable rows. A row may
show title, author, reading progress, and a secondary status line. Rotary
left/right changes the selected row and rotary press opens it. Touch selects or
opens a row according to the current interaction state.

The screen has explicit states for no SD card, SD errors, no books, invalid
books, and loading.

### Reading

Reading gives the document content the largest possible region between the
shared bars. Controls remain visible as footer action regions; the document
must never render underneath them.

- Rotary left/right changes page or chapter.
- Touching the left/right footer actions performs the same operations.
- Touching the center footer opens Quick Settings.
- Rotary press opens Quick Settings.
- At a chapter boundary, the same action loads the adjacent spine document.
- Page and spine position are persisted after successful navigation.

The Reading screen must show an asynchronous loading state while a chapter is
being opened and a recoverable error if it cannot be loaded.

### Settings

Settings is a focusable list. Rotary left/right changes the focused row and
rotary press edits or applies it. Touch activates a row or value control.

Initial settings include:

- Text size.
- Line spacing.
- Margins.
- Display refresh mode.
- Sleep timeout.

Settings are device-local and stored separately from removable book data.

### Quick Settings Overlay

Quick Settings is an opaque popup over the current screen. It preserves the
underlying screen and both shared bars, and uses a strong border rather than
transparency.

Initial actions include:

- Text size.
- Line spacing.
- Refresh mode.
- Sleep timeout.
- Bookmark toggle when implemented.
- Return to Home or Library.

Rotary left/right moves focus, rotary press activates or confirms, and touch
activates a control. Touching outside dismisses the overlay. A button or press
action must provide a deterministic dismissal path.

## Interaction Model

Input drivers publish hardware events. The UI converts them to logical actions;
screens must not depend directly on GPIO or GT911 details.

```text
physical event ----\
                    > logical UI action -> focused screen transition
touch target ------/
```

The logical action set should include navigation, activation, dismissal,
previous/next page, previous/next chapter, and sleep. The same action must
produce the same result regardless of whether it came from touch or physical
input.

Focus is persistent within a screen while the screen remains active. Focus is
shown visibly using grayscale-safe inversion or borders.

## State Model

```text
Home
├── Library
│   └── Reading
├── Recent Books
│   └── Reading
├── Settings
└── Quick Settings overlay
```

Quick Settings may be opened from Reading and, later, from other screens. A
screen transition must preserve or explicitly discard its overlay state.

## Implementation Order

1. Confirm grayscale polarity, orientation, and stable bar heights through
   `/dev/video5` captures.
2. Add a UI screen controller and logical action enum.
3. Add shared status and indication bar rendering.
4. Build the Home launcher and focus behavior.
5. Redesign Library rows and empty/error states.
6. Redesign Reading content, footer actions, and loading/error overlays.
7. Add Quick Settings and Settings screens.
8. Add simulated event sequences for every screen and boundary condition.
9. Validate full-screen and dirty-region captures on hardware.

All GUI modules must remain compatible with the no-framework, bounded-memory,
handwritten-code constraints in the project scope.
