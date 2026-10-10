#ifndef READER_PLATFORM_H
#define READER_PLATFORM_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

enum platform_status {
    PLATFORM_OK = 0,
    PLATFORM_ERROR = 1,
    PLATFORM_INVALID_ARGUMENT = 2,
    PLATFORM_TIMEOUT = 3,
    PLATFORM_NOT_SUPPORTED = 4,
};

enum platform_event_type {
    PLATFORM_EVENT_NONE = 0,
    PLATFORM_EVENT_QUIT,
    PLATFORM_EVENT_TOUCH_DOWN,
    PLATFORM_EVENT_TOUCH_MOVE,
    PLATFORM_EVENT_TOUCH_UP,
    PLATFORM_EVENT_BUTTON,
};

struct font;

struct platform_event {
    enum platform_event_type type;
    int32_t x;
    int32_t y;
    int32_t button;
    bool pressed;
};

struct platform_display {
    void* context;
    int32_t width;
    int32_t height;
    enum platform_status (*clear)(void* context, uint8_t gray);
    enum platform_status (*rectangle)(void* context, int32_t x, int32_t y, int32_t width,
                                      int32_t height, uint8_t gray);
    enum platform_status (*text)(void* context, const struct font* font, int32_t x, int32_t y,
                                 const char* text, size_t length, uint8_t scale, uint8_t gray);
    enum platform_status (*present)(void* context, bool partial);
};

struct platform {
    void* context;
    struct platform_display display;
    enum platform_status (*next_event)(void* context, struct platform_event* event);
    uint64_t (*monotonic_ms)(void* context);
    void (*sleep_ms)(void* context, uint32_t milliseconds);
};

#endif
