#ifndef READER_RUNTIME_H
#define READER_RUNTIME_H

#include "platform.h"

struct runtime;

struct runtime_config {
    const char* script_directory;
    const char* library_directory;
    const char* state_directory;
    size_t memory_limit;
    uint32_t instruction_limit;
};

enum platform_status runtime_create(struct runtime** runtime, const struct runtime_config* config,
                                    const struct platform* platform);
enum platform_status runtime_reload(struct runtime** runtime, const struct runtime_config* config);
enum platform_status runtime_stop(struct runtime* runtime);
void runtime_destroy(struct runtime* runtime);
enum platform_status runtime_step(struct runtime* runtime, const struct platform_event* event);
enum platform_status runtime_update(struct runtime* runtime, uint64_t monotonic_ms);
enum platform_status runtime_draw(struct runtime* runtime);
bool runtime_failed(const struct runtime* runtime);
const char* runtime_error(const struct runtime* runtime);

#endif
