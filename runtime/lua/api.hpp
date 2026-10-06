#pragma once

#include <cstddef>
#include <cstdint>
#include <dlfcn.h>

/**
 * @brief Minimal Lua 5.4 ABI surface loaded dynamically on host builds.
 *
 * The simulator intentionally avoids a compile-time dependency on Lua development
 * headers. Embedded builds can replace this loader with the same ABI backed by a
 * vendored Lua source build.
 */
namespace lua::api {

struct state;
using integer = std::int64_t;
using c_function = int (*)(state*);
using k_context = std::intptr_t;
using k_function = int (*)(state*, int, k_context);

inline constexpr int ok = 0;
inline constexpr int type_nil = 0;
inline constexpr int type_boolean = 1;
inline constexpr int type_number = 3;
inline constexpr int type_string = 4;
inline constexpr int registry_index = -1001000;
constexpr int upvalue_index(int i) { return registry_index - i; }

struct library {
  void* handle = nullptr;

  state* (*new_state)() = nullptr;
  void (*open_libs)(state*) = nullptr;
  int (*load_file)(state*, const char*, const char*) = nullptr;
  int (*pcall)(state*, int, int, int, k_context, k_function) = nullptr;
  void (*close)(state*) = nullptr;
  int (*get_top)(state*) = nullptr;
  void (*set_top)(state*, int) = nullptr;
  int (*get_global)(state*, const char*) = nullptr;
  void (*set_global)(state*, const char*) = nullptr;
  void (*create_table)(state*, int, int) = nullptr;
  void (*set_field)(state*, int, const char*) = nullptr;
  int (*get_field)(state*, int, const char*) = nullptr;
  void (*push_cclosure)(state*, c_function, int) = nullptr;
  void (*push_lightuserdata)(state*, void*) = nullptr;
  void* (*to_userdata)(state*, int) = nullptr;
  void (*push_integer)(state*, integer) = nullptr;
  integer (*to_integer)(state*, int, int*) = nullptr;
  void (*push_boolean)(state*, int) = nullptr;
  int (*to_boolean)(state*, int) = nullptr;
  const char* (*push_string)(state*, const char*) = nullptr;
  const char* (*to_string)(state*, int, std::size_t*) = nullptr;
  int (*type)(state*, int) = nullptr;
};

template <typename T>
inline bool symbol(void* handle, const char* name, T& out) {
  out = reinterpret_cast<T>(dlsym(handle, name));
  return out != nullptr;
}

inline bool load(library& self) {
  self.handle = dlopen("liblua5.4.so.0", RTLD_NOW | RTLD_LOCAL);
  if (self.handle == nullptr) self.handle = dlopen("liblua5.4.so", RTLD_NOW | RTLD_LOCAL);
  if (self.handle == nullptr) self.handle = dlopen("liblua.so.5.4", RTLD_NOW | RTLD_LOCAL);
  if (self.handle == nullptr) self.handle = dlopen("liblua.so", RTLD_NOW | RTLD_LOCAL);
  if (self.handle == nullptr) return false;

  return symbol(self.handle, "luaL_newstate", self.new_state) &&
         symbol(self.handle, "luaL_openlibs", self.open_libs) &&
         symbol(self.handle, "luaL_loadfilex", self.load_file) &&
         symbol(self.handle, "lua_pcallk", self.pcall) && symbol(self.handle, "lua_close", self.close) &&
         symbol(self.handle, "lua_gettop", self.get_top) && symbol(self.handle, "lua_settop", self.set_top) &&
         symbol(self.handle, "lua_getglobal", self.get_global) &&
         symbol(self.handle, "lua_setglobal", self.set_global) &&
         symbol(self.handle, "lua_createtable", self.create_table) &&
         symbol(self.handle, "lua_setfield", self.set_field) && symbol(self.handle, "lua_getfield", self.get_field) &&
         symbol(self.handle, "lua_pushcclosure", self.push_cclosure) &&
         symbol(self.handle, "lua_pushlightuserdata", self.push_lightuserdata) &&
         symbol(self.handle, "lua_touserdata", self.to_userdata) &&
         symbol(self.handle, "lua_pushinteger", self.push_integer) &&
         symbol(self.handle, "lua_tointegerx", self.to_integer) &&
         symbol(self.handle, "lua_pushboolean", self.push_boolean) &&
         symbol(self.handle, "lua_toboolean", self.to_boolean) &&
         symbol(self.handle, "lua_pushstring", self.push_string) &&
         symbol(self.handle, "lua_tolstring", self.to_string) && symbol(self.handle, "lua_type", self.type);
}

inline void unload(library& self) {
  if (self.handle != nullptr) dlclose(self.handle);
  self = {};
}

inline void pop(library& lib, state* l, int count) { lib.set_top(l, -count - 1); }

}  // namespace lua::api
