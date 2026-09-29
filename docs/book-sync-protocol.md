# X-Reader book sync protocol v1

The reader treats the configured sync server as a small REST endpoint. All JSON is UTF-8.

## Library manifest

`GET /v1/library`

```json
{
  "books": [
    {
      "name": "example.epub",
      "url": "https://server/books/example.epub",
      "size": 1234567,
      "sha256": "0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef"
    }
  ]
}
```

`name` must be a plain filename. `size` and `sha256` are used to decide whether a local copy is
current and to verify a completed download. The reader downloads to `<name>.part`, resumes with an
HTTP `Range` request when possible, validates the result, and only then renames it to `<name>`.

The v1 client does not delete local books that are absent from the manifest. This conservative
policy avoids data loss while offline or when a server-side library is temporarily incomplete.

## Reading progress

`GET /v1/progress?book=<url-encoded-filename>`

```json
{"spine": 3, "page": 12}
```

`POST /v1/progress`

```json
{"book": "example.epub", "spine": 3, "page": 12}
```

Conflict policy for v1 is deterministic and offline-friendly: the furthest position wins. A remote
position ahead of the local position is saved locally; otherwise the local position is uploaded.

## Bookmarks

`GET /v1/bookmarks?book=<url-encoded-filename>`

```json
{
  "bookmarks": [
    {"spine": 1, "page": 4},
    {"spine": 5, "page": 2}
  ]
}
```

`POST /v1/bookmarks`

```json
{
  "book": "example.epub",
  "bookmarks": [
    {"spine": 1, "page": 4},
    {"spine": 5, "page": 2}
  ]
}
```

The reader merges local and remote bookmarks by `(spine,page)`, removes duplicates, persists the
merged set, and uploads it. The current embedded limit is eight bookmarks per book.

## Failure behavior

A failed transfer leaves `.part` data in place so the next sync can resume. Installed `.epub` files
are never replaced until the incoming file has passed size/hash verification.
