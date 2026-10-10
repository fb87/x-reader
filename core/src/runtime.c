#include "runtime.h"
#include "font.h"

#include <lauxlib.h>
#include <lua.h>
#include <lualib.h>

#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct memory_budget {
    size_t used;
    size_t limit;
};

struct runtime {
    lua_State* state;
    struct platform platform;
    struct memory_budget memory;
    char error[256];
    uint32_t instruction_limit;
    uint32_t instructions;
};

static void* bounded_allocator(void* opaque, void* pointer, size_t old_size, size_t new_size) {
    struct memory_budget* budget = opaque;
    if (new_size > old_size && new_size - old_size > budget->limit - budget->used) {
        return NULL;
    }
    if (new_size > old_size) {
        budget->used += new_size - old_size;
    } else {
        budget->used -= old_size - new_size;
    }
    if (new_size == 0U) {
        free(pointer);
        return NULL;
    }
    return realloc(pointer, new_size);
}

static void set_error(struct runtime* runtime, const char* message) {
    (void)snprintf(runtime->error, sizeof(runtime->error), "%s", message);
}

static void instruction_hook(lua_State* state, lua_Debug* debug) {
    (void)debug;
    struct runtime** runtime = (struct runtime**)lua_getextraspace(state);
    (*runtime)->instructions += 1U;
    if ((*runtime)->instructions > (*runtime)->instruction_limit) {
        luaL_error(state, "Lua instruction limit exceeded");
    }
}

static struct runtime* runtime_from_upvalue(lua_State* state) {
    struct runtime** runtime = (struct runtime**)lua_touserdata(state, lua_upvalueindex(1));
    return *runtime;
}

static int display_clear(lua_State* state) {
    struct runtime* runtime = runtime_from_upvalue(state);
    const lua_Integer gray = luaL_checkinteger(state, 1);
    if (gray < 0 || gray > 15) {
        return luaL_error(state, "gray must be between 0 and 15");
    }
    const enum platform_status status =
        runtime->platform.display.clear(runtime->platform.display.context, (uint8_t)gray);
    return status == PLATFORM_OK ? 0 : luaL_error(state, "display.clear failed");
}

static int display_rectangle(lua_State* state) {
    struct runtime* runtime = runtime_from_upvalue(state);
    const lua_Integer x = luaL_checkinteger(state, 1);
    const lua_Integer y = luaL_checkinteger(state, 2);
    const lua_Integer width = luaL_checkinteger(state, 3);
    const lua_Integer height = luaL_checkinteger(state, 4);
    const lua_Integer gray = luaL_checkinteger(state, 5);
    if (gray < 0 || gray > 15) {
        return luaL_error(state, "gray must be between 0 and 15");
    }
    const enum platform_status status = runtime->platform.display.rectangle(
        runtime->platform.display.context, (int32_t)x, (int32_t)y, (int32_t)width, (int32_t)height,
        (uint8_t)gray);
    return status == PLATFORM_OK ? 0 : luaL_error(state, "canvas.rectangle failed");
}

static int display_text(lua_State* state) {
    struct runtime* runtime = runtime_from_upvalue(state);
    const lua_Integer x = luaL_checkinteger(state, 1);
    const lua_Integer y = luaL_checkinteger(state, 2);
    size_t length = 0U;
    const char* text = luaL_checklstring(state, 3, &length);
    const lua_Integer scale = luaL_optinteger(state, 4, 1);
    const lua_Integer gray = luaL_optinteger(state, 5, 0);
    if (length > 1024U || scale < 1 || scale > 8 || gray < 0 || gray > 15) {
        return luaL_error(state, "invalid canvas.text arguments");
    }
    const bool icon_scale = scale > 3;
    const struct font* font =
        icon_scale ? font_for_size((unsigned)scale - 3U) : font_for_size((unsigned)scale);
    const uint8_t render_scale = 1U;
    const enum platform_status status =
        runtime->platform.display.text(runtime->platform.display.context, font, (int32_t)x,
                                       (int32_t)y, text, length, render_scale, (uint8_t)gray);
    return status == PLATFORM_OK ? 0 : luaL_error(state, "canvas.text failed");
}

static size_t text_codepoint(const char* text, size_t length, uint32_t* codepoint) {
    const uint8_t first = (uint8_t)text[0];
    if (first < 0x80U) {
        *codepoint = first;
        return 1U;
    }
    if ((first & 0xe0U) == 0xc0U && length >= 2U) {
        *codepoint = ((uint32_t)(first & 0x1fU) << 6U) | ((uint8_t)text[1] & 0x3fU);
        return 2U;
    }
    if ((first & 0xf0U) == 0xe0U && length >= 3U) {
        *codepoint = ((uint32_t)(first & 0x0fU) << 12U) |
                     ((uint32_t)((uint8_t)text[1] & 0x3fU) << 6U) | ((uint8_t)text[2] & 0x3fU);
        return 3U;
    }
    *codepoint = 0xfffdU;
    return 1U;
}

static int display_measure(lua_State* state) {
    size_t length = 0U;
    const char* text = luaL_checklstring(state, 1, &length);
    const lua_Integer requested_size = luaL_optinteger(state, 2, 2);
    if (requested_size < 1 || requested_size > 8) {
        return luaL_error(state, "invalid canvas.measure size");
    }
    const unsigned size = (unsigned)requested_size;
    const bool icon_scale = size > 3U;
    const struct font* font = icon_scale ? font_for_size(size - 3U) : font_for_size(size);
    const uint8_t render_scale = 1U;
    int32_t width = 0;
    int32_t top = INT32_MAX;
    int32_t bottom = INT32_MIN;
    for (size_t offset = 0U; offset < length;) {
        uint32_t codepoint = 0U;
        const size_t consumed = text_codepoint(&text[offset], length - offset, &codepoint);
        const struct font_glyph* glyph = font_lookup(font, codepoint);
        if (glyph == NULL) {
            glyph = font_lookup(font, '?');
        }
        if (glyph != NULL) {
            width += (int32_t)glyph->advance * render_scale;
            const int32_t glyph_top = ((int32_t)glyph->bearing_y + font->baseline) * render_scale;
            const int32_t glyph_bottom = glyph_top + (int32_t)glyph->height * render_scale;
            if (glyph_top < top)
                top = glyph_top;
            if (glyph_bottom > bottom)
                bottom = glyph_bottom;
        }
        offset += consumed;
    }
    if (top == INT32_MAX) {
        top = 0;
        bottom = font->baseline;
    }
    lua_pushinteger(state, width);
    lua_pushinteger(state, bottom - top);
    lua_pushinteger(state, top);
    lua_pushinteger(state, bottom);
    return 4;
}

static int display_present(lua_State* state) {
    struct runtime* runtime = runtime_from_upvalue(state);
    const bool partial = lua_toboolean(state, 1) != 0;
    const enum platform_status status =
        runtime->platform.display.present(runtime->platform.display.context, partial);
    return status == PLATFORM_OK ? 0 : luaL_error(state, "display.present failed");
}

static int system_monotonic_ms(lua_State* state) {
    struct runtime* runtime = runtime_from_upvalue(state);
    lua_pushinteger(state, (lua_Integer)runtime->platform.monotonic_ms(runtime->platform.context));
    return 1;
}

static int display_size(lua_State* state) {
    struct runtime* runtime = runtime_from_upvalue(state);
    lua_pushinteger(state, runtime->platform.display.width);
    lua_pushinteger(state, runtime->platform.display.height);
    return 2;
}

static void push_runtime_userdata(lua_State* state, struct runtime* runtime) {
    struct runtime** value = lua_newuserdatauv(state, sizeof(*value), 0);
    *value = runtime;
}

static void register_services(struct runtime* runtime) {
    lua_State* state = runtime->state;
    lua_newtable(state);
    push_runtime_userdata(state, runtime);
    lua_pushcclosure(state, display_clear, 1);
    lua_setfield(state, -2, "clear");
    push_runtime_userdata(state, runtime);
    lua_pushcclosure(state, display_rectangle, 1);
    lua_setfield(state, -2, "rectangle");
    push_runtime_userdata(state, runtime);
    lua_pushcclosure(state, display_text, 1);
    lua_setfield(state, -2, "text");
    push_runtime_userdata(state, runtime);
    lua_pushcclosure(state, display_measure, 1);
    lua_setfield(state, -2, "measure");
    lua_setglobal(state, "canvas");

    lua_newtable(state);
    push_runtime_userdata(state, runtime);
    lua_pushcclosure(state, display_present, 1);
    lua_setfield(state, -2, "present");
    push_runtime_userdata(state, runtime);
    lua_pushcclosure(state, display_size, 1);
    lua_setfield(state, -2, "size");
    lua_setglobal(state, "display");

    lua_newtable(state);
    push_runtime_userdata(state, runtime);
    lua_pushcclosure(state, system_monotonic_ms, 1);
    lua_setfield(state, -2, "monotonic_ms");
    lua_setglobal(state, "system");
}

static void restrict_libraries(lua_State* state) {
    const char* globals[] = {"io", "os", "debug", "dofile", "loadfile", "load"};
    for (size_t index = 0; index < sizeof(globals) / sizeof(globals[0]); ++index) {
        lua_pushnil(state);
        lua_setglobal(state, globals[index]);
    }

    lua_getglobal(state, "package");
    lua_pushliteral(state, "");
    lua_setfield(state, -2, "cpath");
    lua_pushnil(state);
    lua_setfield(state, -2, "loadlib");
    lua_getfield(state, -1, "searchers");
    lua_newtable(state);
    lua_geti(state, -2, 1);
    lua_seti(state, -2, 1);
    lua_geti(state, -2, 2);
    lua_seti(state, -2, 2);
    lua_replace(state, -2);
    lua_setfield(state, -2, "searchers");
    lua_pop(state, 1);
}

static enum platform_status call_application(struct runtime* runtime, const char* field,
                                             int arguments, int results) {
    lua_State* state = runtime->state;
    lua_getglobal(state, "application");
    if (!lua_istable(state, -1)) {
        lua_pop(state, 1);
        set_error(runtime, "global application is not a table");
        return PLATFORM_ERROR;
    }
    lua_getfield(state, -1, field);
    lua_remove(state, -2);
    if (!lua_isfunction(state, -1)) {
        lua_pop(state, 1);
        set_error(runtime, "application callback is missing");
        return PLATFORM_ERROR;
    }
    lua_insert(state, -(arguments + 1));
    runtime->instructions = 0U;
    const int status = lua_pcall(state, arguments, results, 0);
    if (status != LUA_OK) {
        const char* message = lua_tostring(state, -1);
        set_error(runtime, message == NULL ? "Lua callback failed" : message);
        lua_settop(state, 0);
        return PLATFORM_ERROR;
    }
    lua_settop(state, 0);
    return PLATFORM_OK;
}

static enum platform_status load_script(struct runtime* runtime, const char* directory) {
    char path[512];
    const int written = snprintf(path, sizeof(path), "%s/init.lua", directory);
    if (written < 0 || (size_t)written >= sizeof(path)) {
        set_error(runtime, "Lua script path is too long");
        return PLATFORM_INVALID_ARGUMENT;
    }
    lua_getglobal(runtime->state, "package");
    lua_pushfstring(runtime->state, "%s/?.lua", directory);
    lua_setfield(runtime->state, -2, "path");
    lua_pop(runtime->state, 1);
    if (luaL_loadfile(runtime->state, path) != LUA_OK ||
        lua_pcall(runtime->state, 0, 1, 0) != LUA_OK) {
        const char* message = lua_tostring(runtime->state, -1);
        set_error(runtime, message == NULL ? "Lua script failed" : message);
        lua_settop(runtime->state, 0);
        return PLATFORM_ERROR;
    }
    if (!lua_istable(runtime->state, -1)) {
        set_error(runtime, "init.lua must return a table");
        lua_settop(runtime->state, 0);
        return PLATFORM_ERROR;
    }
    lua_setglobal(runtime->state, "application");
    return PLATFORM_OK;
}

static enum platform_status capture_snapshot(struct runtime* runtime, char* value,
                                             size_t capacity) {
    lua_State* state = runtime->state;
    lua_getglobal(state, "application");
    lua_getfield(state, -1, "snapshot");
    lua_remove(state, -2);
    if (!lua_isfunction(state, -1)) {
        lua_pop(state, 1);
        value[0] = '\0';
        return PLATFORM_OK;
    }
    runtime->instructions = 0U;
    if (lua_pcall(state, 0, 1, 0) != LUA_OK) {
        const char* message = lua_tostring(state, -1);
        set_error(runtime, message == NULL ? "snapshot callback failed" : message);
        lua_settop(state, 0);
        return PLATFORM_ERROR;
    }
    if (!lua_istable(state, -1)) {
        set_error(runtime, "snapshot must return a table");
        lua_settop(state, 0);
        return PLATFORM_ERROR;
    }
    lua_getfield(state, -1, "screen");
    if (!lua_isnil(state, -1)) {
        size_t length = 0U;
        const char* screen = lua_tolstring(state, -1, &length);
        if (screen == NULL || length >= capacity) {
            set_error(runtime, "snapshot screen value is too long");
            lua_settop(state, 0);
            return PLATFORM_ERROR;
        }
        memcpy(value, screen, length);
        value[length] = '\0';
    } else {
        value[0] = '\0';
    }
    lua_settop(state, 0);
    return PLATFORM_OK;
}

static enum platform_status create_runtime(struct runtime** result,
                                           const struct runtime_config* config,
                                           const struct platform* platform, const char* snapshot) {
    struct runtime* runtime = calloc(1U, sizeof(*runtime));
    if (runtime == NULL) {
        return PLATFORM_ERROR;
    }
    runtime->platform = *platform;
    runtime->instruction_limit = config->instruction_limit;
    runtime->memory.limit = config->memory_limit;
    runtime->state = lua_newstate(bounded_allocator, &runtime->memory);
    if (runtime->state == NULL) {
        free(runtime);
        return PLATFORM_ERROR;
    }
    memcpy(lua_getextraspace(runtime->state), &runtime, sizeof(runtime));
    luaL_openlibs(runtime->state);
    restrict_libraries(runtime->state);
    lua_sethook(runtime->state, instruction_hook, LUA_MASKCOUNT, 1);
    register_services(runtime);
    const enum platform_status status = load_script(runtime, config->script_directory);
    if (status == PLATFORM_OK) {
        if (snapshot == NULL) {
            lua_pushnil(runtime->state);
        } else {
            lua_newtable(runtime->state);
            lua_pushstring(runtime->state, snapshot);
            lua_setfield(runtime->state, -2, "screen");
        }
        if (call_application(runtime, "start", 1, 0) == PLATFORM_OK) {
            *result = runtime;
            return PLATFORM_OK;
        }
    }
    *result = NULL;
    runtime_destroy(runtime);
    return status == PLATFORM_OK ? PLATFORM_ERROR : status;
}

enum platform_status runtime_create(struct runtime** result, const struct runtime_config* config,
                                    const struct platform* platform) {
    if (result == NULL || config == NULL || platform == NULL || config->script_directory == NULL ||
        config->memory_limit == 0U || config->instruction_limit == 0U) {
        return PLATFORM_INVALID_ARGUMENT;
    }
    return create_runtime(result, config, platform, NULL);
}

enum platform_status runtime_reload(struct runtime** result, const struct runtime_config* config) {
    if (result == NULL || *result == NULL || config == NULL) {
        return PLATFORM_INVALID_ARGUMENT;
    }
    struct runtime* old_runtime = *result;
    char snapshot[128];
    if (capture_snapshot(old_runtime, snapshot, sizeof(snapshot)) != PLATFORM_OK) {
        return PLATFORM_ERROR;
    }
    struct runtime* new_runtime = NULL;
    const enum platform_status status = create_runtime(&new_runtime, config, &old_runtime->platform,
                                                       snapshot[0] == '\0' ? NULL : snapshot);
    if (status != PLATFORM_OK) {
        set_error(old_runtime, "new Lua VM failed to initialize");
        return status;
    }
    lua_pushliteral(old_runtime->state, "reload");
    (void)call_application(old_runtime, "stop", 1, 0);
    runtime_destroy(old_runtime);
    *result = new_runtime;
    return PLATFORM_OK;
}

void runtime_destroy(struct runtime* runtime) {
    if (runtime == NULL) {
        return;
    }
    if (runtime->state != NULL) {
        lua_close(runtime->state);
    }
    free(runtime);
}

enum platform_status runtime_step(struct runtime* runtime, const struct platform_event* event) {
    if (runtime == NULL) {
        return PLATFORM_INVALID_ARGUMENT;
    }
    if (event == NULL) {
        return call_application(runtime, "start", 0, 0);
    }
    lua_newtable(runtime->state);
    lua_pushinteger(runtime->state, event->type);
    lua_setfield(runtime->state, -2, "type");
    lua_pushinteger(runtime->state, event->x);
    lua_setfield(runtime->state, -2, "x");
    lua_pushinteger(runtime->state, event->y);
    lua_setfield(runtime->state, -2, "y");
    lua_pushinteger(runtime->state, event->button);
    lua_setfield(runtime->state, -2, "button");
    lua_pushboolean(runtime->state, event->pressed);
    lua_setfield(runtime->state, -2, "pressed");
    lua_pushinteger(runtime->state,
                    (lua_Integer)runtime->platform.monotonic_ms(runtime->platform.context));
    lua_setfield(runtime->state, -2, "time");
    return call_application(runtime, "event", 1, 0);
}

enum platform_status runtime_draw(struct runtime* runtime) {
    if (runtime == NULL) {
        return PLATFORM_INVALID_ARGUMENT;
    }
    return call_application(runtime, "draw", 0, 0);
}

enum platform_status runtime_update(struct runtime* runtime, uint64_t monotonic_ms) {
    if (runtime == NULL) {
        return PLATFORM_INVALID_ARGUMENT;
    }
    lua_pushinteger(runtime->state, (lua_Integer)monotonic_ms);
    return call_application(runtime, "update", 1, 0);
}

bool runtime_failed(const struct runtime* runtime) {
    return runtime == NULL || runtime->error[0] != '\0';
}

const char* runtime_error(const struct runtime* runtime) {
    return runtime == NULL ? "runtime is null" : runtime->error;
}
