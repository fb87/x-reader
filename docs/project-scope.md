# Project Scope

## Goal

Build a low-power EPUB reader firmware for the M5Paper K049 using native
ESP-IDF APIs and handwritten implementation code. The result must be usable
from a microSD card, readable on a 540 x 960 e-ink display, and operable with
the GT911 touch panel and rotary switch.

## Primary Target

- Board: M5Paper K049
- SoC: ESP32-D0WDQ6-V3
- Flash: 16 MB
- PSRAM: 8 MB
- Display: 540 x 960 IT8951E e-ink panel
- Touch: GT911 capacitive controller
- Storage: microSD card
- RTC: BM8563

## Future Target

Xteink 4 is a future board target. Its hardware details must be verified from
the exact board revision before implementation. Board-specific code must not
leak into EPUB parsing, text layout, or application state.

## In Scope

- Board drivers for display, touch, rotary input, SD card, RTC, and power
- Framebuffer and basic 1-bit/4-bit graphics
- Bitmap font rendering and UTF-8 decoding
- EPUB container and package parsing
- ZIP stored entries and DEFLATE entries
- XML, XHTML, and limited CSS parsing
- Text wrapping and pagination
- Library, reader, table of contents, settings, and reading state
- Deep sleep and wake/resume behavior
- Host-testable pure parsing and layout modules where practical

## Initial EPUB Support

- Unencrypted EPUB 2 and EPUB 3
- UTF-8 text
- Reflowable XHTML documents
- Headings, paragraphs, emphasis, links, and lists
- Basic font, margin, alignment, and line-spacing CSS
- Navigation documents and spine order
- Cover image discovery

## Explicitly Out of Scope Initially

- DRM
- JavaScript and embedded media
- SVG rendering
- Full CSS implementation
- Complex bidirectional text shaping
- Font subsetting and advanced OpenType features
- Network book synchronization
- Cloud services
- Audio or accessibility speech output

## Non-negotiable Constraints

- No Arduino or PlatformIO
- No external firmware libraries
- No M5Stack Arduino libraries
- No GUI framework
- No ZIP, XML, HTML, CSS, font, or graphics library
- No C++ STL or hidden resource ownership
- All target resources must have explicit bounds and failure handling
