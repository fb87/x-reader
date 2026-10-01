#include "chapter_stream.hpp"

#include <string.h>

namespace xreader
{
namespace epub
{
namespace
{
static const char zero_sentinel[8] = {};
} // namespace

bool chapter_stream_open(chapter_stream_t* stream, const document_t* document)
{
    if (stream == nullptr || document == nullptr)
        return false;
    memset(stream, 0, sizeof(*stream));
    stream->total_length = document->length;
    if (document->length == 0)
        return true;
    stream->file = fopen(document->cache_path, "rb");
    return stream->file != nullptr;
}

const char* chapter_stream_at(chapter_stream_t* stream, size_t offset)
{
    if (stream == nullptr || stream->file == nullptr || offset >= stream->total_length)
        return zero_sentinel;
    if (offset < stream->file_offset || offset >= stream->file_offset + stream->window_length)
    {
        if (fseek(stream->file, static_cast<long>(offset), SEEK_SET) != 0)
            return zero_sentinel;
        const size_t read = fread(stream->window, 1, chapter_stream_window_size, stream->file);
        memset(stream->window + read, 0, sizeof(stream->window) - read);
        stream->file_offset = offset;
        stream->window_length = read;
        if (read == 0)
            return zero_sentinel;
    }
    return stream->window + (offset - stream->file_offset);
}

void chapter_stream_close(chapter_stream_t* stream)
{
    if (stream != nullptr && stream->file != nullptr)
    {
        fclose(stream->file);
        stream->file = nullptr;
    }
}

} // namespace epub
} // namespace xreader
