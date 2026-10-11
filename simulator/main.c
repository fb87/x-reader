#include "platform.h"
#include "platform/sdl/platform.h"
#include "runtime.h"

#include <SDL.h>

#include <dirent.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

enum { DEFAULT_INSTRUCTION_LIMIT = 1000000 };

static uint64_t script_stamp(const char* directory) {
    DIR* stream = opendir(directory);
    if (stream == NULL) {
        return 0U;
    }
    uint64_t stamp = 1469598103934665603ULL;
    struct dirent* entry = NULL;
    while ((entry = readdir(stream)) != NULL) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }
        char path[512];
        const int written = snprintf(path, sizeof(path), "%s/%s", directory, entry->d_name);
        if (written < 0 || (size_t)written >= sizeof(path)) {
            continue;
        }
        struct stat file = {0};
        if (stat(path, &file) != 0) {
            continue;
        }
        if (S_ISDIR(file.st_mode)) {
            stamp ^= script_stamp(path);
            stamp *= 1099511628211ULL;
        } else if (S_ISREG(file.st_mode) &&
                   (strlen(entry->d_name) > 4U &&
                    strcmp(entry->d_name + strlen(entry->d_name) - 4U, ".lua") == 0)) {
            for (const char* character = path; *character != '\0'; ++character) {
                stamp ^= (uint8_t)*character;
                stamp *= 1099511628211ULL;
            }
            stamp ^= (uint64_t)file.st_mtime;
            stamp *= 1099511628211ULL;
            stamp ^= (uint64_t)file.st_size;
            stamp *= 1099511628211ULL;
        }
    }
    closedir(stream);
    return stamp;
}

int main(int argc, char** argv) {
    const char* script_directory = "lua";
    const char* library_directory = NULL;
    const char* state_directory = NULL;
    bool self_test = false;
    bool watch = false;
    for (int argument = 1; argument < argc; ++argument) {
        if (strcmp(argv[argument], "--self-test") == 0) {
            self_test = true;
        } else if (strcmp(argv[argument], "--watch") == 0) {
            watch = true;
        } else if (strcmp(argv[argument], "--lua") == 0 && argument + 1 < argc) {
            script_directory = argv[++argument];
        } else if (strcmp(argv[argument], "--library") == 0 && argument + 1 < argc) {
            library_directory = argv[++argument];
        } else if (strcmp(argv[argument], "--state") == 0 && argument + 1 < argc) {
            state_directory = argv[++argument];
        } else {
            fprintf(stderr,
                    "usage: %s [--lua DIRECTORY] [--library DIRECTORY] [--state DIRECTORY] "
                    "[--watch|--self-test]\n",
                    argv[0]);
            return 2;
        }
    }

    char default_library[512];
    char default_state[512];
    const char* sdcard_root = getenv("SDCARD_ROOT");
    if (library_directory == NULL) {
        const int written = snprintf(default_library, sizeof(default_library), "%s",
                                     sdcard_root == NULL ? "/home/dao/data/sdcard" : sdcard_root);
        library_directory =
            written >= 0 && (size_t)written < sizeof(default_library) ? default_library : ".";
    }
    if (state_directory == NULL) {
        const int written =
            snprintf(default_state, sizeof(default_state), "%s/.x-reader", library_directory);
        state_directory =
            written >= 0 && (size_t)written < sizeof(default_state) ? default_state : ".";
    }

    struct platform platform = {0};
    if (sdl_platform_create(&platform, "x-reader simulator") != PLATFORM_OK) {
        fprintf(stderr, "failed to initialize SDL: %s\n", SDL_GetError());
        return 1;
    }
    struct runtime* runtime = NULL;
    const struct runtime_config config = {
        .script_directory = script_directory,
        .library_directory = library_directory,
        .state_directory = state_directory,
        .memory_limit = 1024U * 1024U,
        .instruction_limit = DEFAULT_INSTRUCTION_LIMIT,
    };
    if (runtime_create(&runtime, &config, &platform) != PLATFORM_OK) {
        fprintf(stderr, "failed to initialize Lua: %s\n", runtime_error(runtime));
        sdl_platform_destroy(&platform);
        return 1;
    }

    if (self_test) {
        const enum platform_status status = runtime_draw(runtime);
        (void)runtime_stop(runtime);
        runtime_destroy(runtime);
        sdl_platform_destroy(&platform);
        return status == PLATFORM_OK ? 0 : 1;
    }

    uint64_t previous_script_stamp = script_stamp(script_directory);
    bool running = true;
    while (running) {
        struct platform_event event = {0};
        if (platform.next_event(platform.context, &event) != PLATFORM_OK) {
            fprintf(stderr, "failed to read platform event\n");
            break;
        }
        if (event.type == PLATFORM_EVENT_QUIT) {
            running = false;
        } else if (watch && event.type == PLATFORM_EVENT_BUTTON && event.button == SDLK_r) {
            if (runtime_reload(&runtime, &config) != PLATFORM_OK) {
                fprintf(stderr, "Lua reload failed: %s\n", runtime_error(runtime));
            } else {
                previous_script_stamp = script_stamp(script_directory);
            }
        } else if (event.type != PLATFORM_EVENT_NONE &&
                   runtime_step(runtime, &event) != PLATFORM_OK) {
            fprintf(stderr, "Lua event failed: %s\n", runtime_error(runtime));
            break;
        }
        if (watch && script_stamp(script_directory) != previous_script_stamp) {
            const uint64_t current_script_stamp = script_stamp(script_directory);
            if (runtime_reload(&runtime, &config) != PLATFORM_OK) {
                fprintf(stderr, "Lua reload failed: %s\n", runtime_error(runtime));
            } else {
                previous_script_stamp = current_script_stamp;
            }
            previous_script_stamp = current_script_stamp;
        }
        if (running &&
            runtime_update(runtime, platform.monotonic_ms(platform.context)) != PLATFORM_OK) {
            fprintf(stderr, "Lua update failed: %s\n", runtime_error(runtime));
            break;
        }
        if (running && runtime_draw(runtime) != PLATFORM_OK) {
            fprintf(stderr, "Lua draw failed: %s\n", runtime_error(runtime));
            break;
        }
    }
    (void)runtime_stop(runtime);
    runtime_destroy(runtime);
    sdl_platform_destroy(&platform);
    return running ? 1 : 0;
}
