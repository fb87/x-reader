#pragma once

#include <stddef.h>
#include <stdio.h>

#include "book.hpp"

namespace xreader
{
namespace epub
{

static constexpr size_t chapter_stream_window_size = 2048;

// Forward-scanning (with re-seek-on-miss) reader over a chapter's cached
// plain-text file. Every actual read in ui/reader.cpp's pagination/rendering
// walks forward through one window's worth of bytes at a time; the one
// exception (resuming a "find next" search mid-chapter) just costs one extra
// fseek+fread on a window miss rather than needing a separate seek API.
struct chapter_stream_t
{
    FILE* file;
    size_t file_offset;   // file offset of window[0]
    size_t window_length; // valid bytes in window, starting at file_offset
    size_t total_length;  // document->length, i.e. the chapter's decoded size
    // +4 zero-padded tail so a caller decoding a multi-byte UTF-8 sequence
    // that starts at the last loaded byte can always look 4 bytes ahead
    // safely, even right at end-of-chapter.
    char window[chapter_stream_window_size + 4];
};

// Opens `document`'s cache file for reading. An empty chapter (length == 0,
// no cache file ever written) is a valid, successfully "open" empty stream,
// matching how callers already treat document->length == 0 as an empty
// document rather than an error.
bool chapter_stream_open(chapter_stream_t* stream, const document_t* document);

// Returns a pointer into the stream's window covering `offset`, refilling
// the window (via fseek+fread) if `offset` isn't already covered. Safe to
// read up to 4 bytes forward from the returned pointer even at end-of-text
// (reads back zero bytes past the real content). Returns a stable
// zero-filled pointer for an out-of-range offset or an unopened stream.
const char* chapter_stream_at(chapter_stream_t* stream, size_t offset);

void chapter_stream_close(chapter_stream_t* stream);

} // namespace epub
} // namespace xreader
