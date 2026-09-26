# Milestones

## M0: Baseline

- Nix flake and pinned inputs
- ESP-IDF project skeleton
- Formatting and coding rules
- Documentation structure
- Empty firmware boots and logs

## M1: Display Bring-up

- IT8951E reset and identification
- Full framebuffer refresh
- Grayscale test pattern
- Orientation and dirty-region model

## M2: Board Input and Storage

- SD-card mount and file access
- GT911 touch events
- Rotary input and debounce
- RTC read/write
- Basic power and sleep control

## M3: Graphics

- Drawing primitives
- Bitmap font format
- UTF-8 decoder
- Text measurement and rendering

## M4: EPUB Core

- CRC32
- ZIP central-directory reader
- Stored entries
- DEFLATE decoder
- XML tokenizer
- Container and OPF parsing

## M5: Reader

- XHTML text extraction
- Basic CSS
- Layout and pagination
- Library screen
- Reader navigation
- Table of contents

## M6: Persistence and Power

- Reading-position persistence
- Bookmarks
- Settings
- Resume after wake
- Refresh and battery optimization

## M7: Xteink 4 Port

- Confirm exact hardware revision
- Add board backend
- Add display/input/storage capabilities
- Reuse shared EPUB, layout, and UI tests
