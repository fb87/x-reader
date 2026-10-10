#ifndef READER_SDL_PLATFORM_H
#define READER_SDL_PLATFORM_H

#include <platform.h>

enum platform_status sdl_platform_create(struct platform* platform, const char* title);
void sdl_platform_destroy(struct platform* platform);

#endif
