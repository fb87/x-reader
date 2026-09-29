# Persistent library index

X-Reader stores a compact library catalog at `<mount>/.xreader-library-v1.idx`.

On normal boot the catalog is loaded directly and each indexed file is validated with `stat()` (size and modification time), avoiding a recursive SD-card directory scan and EPUB metadata parse. If the cache is missing, corrupted, or an indexed file changed, the catalog is rebuilt.

The index stores:

- EPUB path
- title
- author
- file size
- modification time

The first rebuild extracts title/author from the EPUB OPF metadata. Import operations rebuild the catalog. Book synchronization invalidates the cache when it installs remote EPUBs so the next load rebuilds it.

Sorting backends currently support title, author, and most-recently-modified. Title order is used by the current Library UI. Search/filter matching is available in the backend for title, author, and filename; a dedicated Search UI is still pending.

The Library list now scrolls its visible window when focus moves past the first six rows, and selecting a Library item actually loads that selected EPUB instead of returning to the previously open book.

## Library views and reading history

The catalog keeps a monotonic `last_read_order` for each book. Opening a book updates this value and
persists the catalog, allowing the Home **Recent** action and the `RECENT` sort mode to work without a
wall-clock/RTC dependency.

Library navigation supports four sort modes: title, author, recently added, and recently read. The
MENU action cycles the sort mode. LEFT opens the reusable text keyboard for title/author/filename
search; RIGHT opens the selected book's details. Search results retain catalog indices rather than
copying EPUB metadata or rescanning storage.
