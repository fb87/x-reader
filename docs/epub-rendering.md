# EPUB visual rendering

X-Reader keeps the EPUB renderer deliberately small for ESP32-class targets.

## Cover cache

During a library-index rebuild the OPF metadata is inspected for both EPUB 2 and
EPUB 3 cover declarations:

- EPUB 2: `<meta name="cover" content="...">`
- EPUB 3: manifest item with `properties="cover-image"`

The cover resource is extracted once into `<sd>/.xreader-covers/`. The persistent
library index stores the cache path and known dimensions, so opening Book Details
does not need to reopen and walk the EPUB ZIP just to locate the cover.

PNG covers are decoded to the native 4-bpp grayscale framebuffer and scaled to
fit the Book Details panel. JPEG dimensions are recognized and cached, but JPEG
pixel decoding is intentionally still pending; those covers fall back to the
text-only details layout.

## Inline images

`<img src="...">` elements are retained in the parsed document as object markers
with their EPUB resource path and intrinsic dimensions. Pagination reserves
vertical space for the image, so image pages and text pages remain stable.

PNG images are lazily decoded when their page is drawn, cached one-at-a-time,
and nearest-neighbor scaled into the 4-bpp framebuffer. If the image format is
known but cannot be decoded, the reader draws a lightweight IMAGE placeholder
instead of dropping content or failing the chapter.

The one-image decode cache bounds steady-state memory. Very large images are
rejected rather than risking an out-of-memory reset.

## CSS subset

The parser now recognizes a small inline-CSS subset that has direct layout value
on an e-paper text reader:

- `display: none` / `visibility: hidden`
- `display: block` / `display: list-item`
- `white-space: pre`, `pre-wrap`, `break-spaces`
- `text-align: left|center|right|justify` (parsed for future line layout)
- `page-break-before: always` / `break-before: page`
- `page-break-after: always` / `break-after: page` (parsed for future block-end handling)

Structural XHTML elements such as paragraphs, headings, sections, lists,
blockquotes and `<pre>` also create explicit document breaks. List items get a
simple `- ` prefix suitable for the bundled bitmap font.

External stylesheets, selector matching, JPEG/SVG decoding and richer block
layout remain separate TODO items rather than pulling a browser-sized engine
onto the device.
