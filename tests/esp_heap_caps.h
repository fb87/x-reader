#pragma once

#include <stdlib.h>

#define MALLOC_CAP_SPIRAM 0
#define MALLOC_CAP_8BIT 0
static inline void* heap_caps_malloc(size_t size, int)
{
    return malloc(size);
}
static inline void* heap_caps_calloc(size_t count, size_t size, int)
{
    return calloc(count, size);
}
static inline void heap_caps_free(void* pointer)
{
    free(pointer);
}
