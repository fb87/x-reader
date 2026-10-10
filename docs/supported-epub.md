# Supported EPUB Profile

This document defines the intended first reader profile. It is deliberately
smaller than the complete EPUB specification.

## Publication Types

Planned support:

- Reflowable EPUB 3 publications
- Reflowable EPUB 2 publications
- Unencrypted ZIP entries using stored or DEFLATE compression
- Well-formed XML and XHTML encoded as UTF-8

Not supported:

- DRM or encrypted resources
- Split or multi-disk ZIP archives
- Fixed-layout publications
- Scripting and interactive content
- Media overlays, audio, or video

## Package and Navigation

Planned support:

- `META-INF/container.xml`
- OPF metadata needed by the bookshelf
- Manifest and linear spine order
- EPUB 3 navigation documents
- EPUB 2 NCX as a later compatibility step
- Archive-relative resource resolution
- Book title, author, identifier, and cover reference when available

## XHTML

The text-first profile is intended to cover:

- Paragraphs and line breaks
- Headings
- Ordered and unordered lists
- Emphasis and strong emphasis
- Hyperlinks within the publication
- Basic block quotations
- Horizontal separators
- Images with dimensions and alternative text
- Explicit page-break hints where practical

Unsupported or initially degraded structures include:

- Complex tables
- Forms
- Canvas
- SVG and MathML
- Ruby annotations
- Frames and embedded web content
- Author-provided scripts

Unknown elements should preserve readable descendant text unless the element
is hidden by a supported rule or has semantics that make this unsafe.

## CSS

The initial CSS subset may include:

- `display: none`
- Block and inline display classification
- `font-style`
- `font-weight`
- A small set of relative font sizes
- `text-align`
- Basic paragraph margins
- Basic whitespace handling
- Simple page-break properties
- A bounded set of element, class, and inline-style matches

The first implementation does not promise full cascading or browser-compatible
selector specificity. Unsupported declarations are ignored.

Not planned for the initial reader:

- Floats
- Absolute or fixed positioning
- Flexbox or grid
- Complex selectors and generated content
- Writing modes
- Full table layout
- CSS animations or transitions

## Text and Fonts

- Input text is UTF-8.
- A bounded bitmap font supplies an explicitly documented Latin-oriented
  character repertoire.
- Common punctuation should be included where practical.
- Unsupported characters use a visible replacement glyph.
- Embedded EPUB fonts are ignored.
- Kerning, font fallback, bidirectional layout, Arabic and Indic shaping, and
  general combining-mark placement are initially unsupported.

The implementation must not claim general Unicode rendering until those
features exist.

## Images

The first decoded format is non-interlaced PNG. The planned decoder supports a
carefully selected set of standard color types and bit depths, converts output
to grayscale, composites alpha, and scales images to the available layout
width.

Initial handling by format:

| Format | Initial behavior |
| --- | --- |
| Non-interlaced PNG | Decode after the PNG milestone |
| Interlaced PNG | Reject or show a placeholder |
| Baseline JPEG | Show a placeholder; decoder deferred |
| Progressive JPEG | Show a placeholder; decoder deferred |
| GIF | Show a placeholder; decoder deferred |
| SVG | Show a placeholder; renderer deferred |

Before PNG decoding is complete, all images may use a bounded placeholder with
alternative text. The parser must still validate resource paths and dimensions.

## Pagination and Positions

Pages are generated for the current display dimensions and reader settings.
They are not intrinsic EPUB locations.

A saved position consists of:

- Stable book identity
- Spine item index or identifier
- Logical position in the normalized content stream
- Relevant layout settings needed to reproduce pagination

The reader must not persist only a compressed file offset. To restore or move
backward, it may replay the current XHTML entry, cache normalized chapter data
on SD, or retain a bounded chapter representation in PSRAM.

The simplest first implementation replays from the start of the spine item and
discards events until the saved logical location is reached.

## Failure Behavior

- Corrupt archives and malformed XML produce an error page, not a crash.
- Unsupported essential features are reported to the user.
- Unsupported optional styling degrades to readable text when possible.
- Size, nesting, and expansion limits are enforced before large allocations.
- A single malformed book must not prevent the bookshelf from opening other
  books.

The same fixtures and failure behavior apply to firmware and simulator builds.
Platform-specific DEFLATE implementations must not change accepted ZIP or EPUB
semantics.
