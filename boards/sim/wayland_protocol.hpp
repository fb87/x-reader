#pragma once

#include <cstddef>
#include <cstdint>

// Minimal Wayland client ABI declarations used by the simulator. The project deliberately
// keeps these declarations local so the simulator can build on systems that only provide
// the Wayland runtime library. The ABI below is the stable libwayland-client ABI.

extern "C" {

struct wl_proxy;
struct wl_display;
struct wl_registry;
struct wl_compositor;
struct wl_surface;
struct wl_shm;
struct wl_shm_pool;
struct wl_buffer;
struct wl_seat;
struct wl_keyboard;
struct wl_pointer;
struct wl_callback;
struct wl_array {
  std::size_t size;
  std::size_t alloc;
  void* data;
};

struct wl_message;
struct wl_interface {
  const char* name;
  int version;
  int method_count;
  const wl_message* methods;
  int event_count;
  const wl_message* events;
};
struct wl_message {
  const char* name;
  const char* signature;
  const wl_interface** types;
};

using wl_fixed_t = std::int32_t;

wl_display* wl_display_connect(const char* name);
void wl_display_disconnect(wl_display* display);
int wl_display_dispatch(wl_display* display);
int wl_display_dispatch_pending(wl_display* display);
int wl_display_roundtrip(wl_display* display);
int wl_display_flush(wl_display* display);
int wl_display_get_fd(wl_display* display);

int wl_proxy_add_listener(wl_proxy* proxy, void (**implementation)(void), void* data);
void wl_proxy_destroy(wl_proxy* proxy);
std::uint32_t wl_proxy_get_version(wl_proxy* proxy);
wl_proxy* wl_proxy_marshal_flags(wl_proxy* proxy, std::uint32_t opcode,
                                 const wl_interface* interface, std::uint32_t version,
                                 std::uint32_t flags, ...);

extern const wl_interface wl_registry_interface;
extern const wl_interface wl_compositor_interface;
extern const wl_interface wl_surface_interface;
extern const wl_interface wl_shm_interface;
extern const wl_interface wl_shm_pool_interface;
extern const wl_interface wl_buffer_interface;
extern const wl_interface wl_seat_interface;
extern const wl_interface wl_keyboard_interface;
extern const wl_interface wl_pointer_interface;

}  // extern "C"

namespace board::sim::wayland::protocol {

inline constexpr std::uint32_t marshal_destroy = 1U;
inline constexpr std::uint32_t shm_format_xrgb8888 = 1U;
inline constexpr std::uint32_t keyboard_keymap_format_xkb_v1 = 1U;
inline constexpr std::uint32_t key_released = 0U;
inline constexpr std::uint32_t key_pressed = 1U;
inline constexpr std::uint32_t pointer_button_released = 0U;
inline constexpr std::uint32_t pointer_button_pressed = 1U;
inline constexpr std::uint32_t pointer_axis_vertical_scroll = 0U;
inline constexpr std::uint32_t seat_capability_pointer = 1U;
inline constexpr std::uint32_t seat_capability_keyboard = 2U;
inline constexpr std::uint32_t btn_left = 0x110U;

struct xdg_wm_base;
struct xdg_surface;
struct xdg_toplevel;

inline const wl_interface* no_types[] = {nullptr};
inline const wl_interface* xdg_surface_get_toplevel_types[] = {nullptr};
inline const wl_interface* xdg_wm_get_surface_types[] = {nullptr, &wl_surface_interface};

inline const wl_message xdg_wm_base_requests[] = {
    {"destroy", "", nullptr},
    {"create_positioner", "n", nullptr},
    {"get_xdg_surface", "no", xdg_wm_get_surface_types},
    {"pong", "u", nullptr},
};
inline const wl_message xdg_wm_base_events[] = {{"ping", "u", nullptr}};
inline const wl_interface xdg_wm_base_interface = {
    "xdg_wm_base", 1, 4, xdg_wm_base_requests, 1, xdg_wm_base_events};

inline const wl_message xdg_surface_requests[] = {
    {"destroy", "", nullptr},
    {"get_toplevel", "n", xdg_surface_get_toplevel_types},
    {"get_popup", "noo", nullptr},
    {"set_window_geometry", "iiii", nullptr},
    {"ack_configure", "u", nullptr},
};
inline const wl_message xdg_surface_events[] = {{"configure", "u", nullptr}};
inline const wl_interface xdg_surface_interface = {
    "xdg_surface", 1, 5, xdg_surface_requests, 1, xdg_surface_events};

inline const wl_message xdg_toplevel_requests[] = {
    {"destroy", "", nullptr},          {"set_parent", "?o", nullptr},
    {"set_title", "s", nullptr},      {"set_app_id", "s", nullptr},
    {"show_window_menu", "ouii", nullptr}, {"move", "ou", nullptr},
    {"resize", "ouu", nullptr},       {"set_max_size", "ii", nullptr},
    {"set_min_size", "ii", nullptr},  {"set_maximized", "", nullptr},
    {"unset_maximized", "", nullptr}, {"set_fullscreen", "?o", nullptr},
    {"unset_fullscreen", "", nullptr},{"set_minimized", "", nullptr},
};
inline const wl_message xdg_toplevel_events[] = {
    {"configure", "iia", nullptr},
    {"close", "", nullptr},
};
inline const wl_interface xdg_toplevel_interface = {
    "xdg_toplevel", 1, 14, xdg_toplevel_requests, 2, xdg_toplevel_events};

inline wl_registry* display_get_registry(wl_display* display) {
  return reinterpret_cast<wl_registry*>(wl_proxy_marshal_flags(
      reinterpret_cast<wl_proxy*>(display), 1, &wl_registry_interface,
      wl_proxy_get_version(reinterpret_cast<wl_proxy*>(display)), 0, nullptr));
}

inline void registry_add_listener(wl_registry* registry, void (**listener)(void), void* data) {
  wl_proxy_add_listener(reinterpret_cast<wl_proxy*>(registry), listener, data);
}

inline wl_proxy* registry_bind(wl_registry* registry, std::uint32_t name,
                               const wl_interface* interface, std::uint32_t version) {
  return wl_proxy_marshal_flags(reinterpret_cast<wl_proxy*>(registry), 0, interface, version, 0,
                                name, interface->name, version, nullptr);
}

inline wl_surface* compositor_create_surface(wl_compositor* compositor) {
  return reinterpret_cast<wl_surface*>(wl_proxy_marshal_flags(
      reinterpret_cast<wl_proxy*>(compositor), 0, &wl_surface_interface,
      wl_proxy_get_version(reinterpret_cast<wl_proxy*>(compositor)), 0, nullptr));
}

inline wl_shm_pool* shm_create_pool(wl_shm* shm, int fd, std::int32_t size) {
  return reinterpret_cast<wl_shm_pool*>(wl_proxy_marshal_flags(
      reinterpret_cast<wl_proxy*>(shm), 0, &wl_shm_pool_interface,
      wl_proxy_get_version(reinterpret_cast<wl_proxy*>(shm)), 0, fd, size, nullptr));
}

inline wl_buffer* shm_pool_create_buffer(wl_shm_pool* pool, std::int32_t offset,
                                         std::int32_t width, std::int32_t height,
                                         std::int32_t stride, std::uint32_t format) {
  return reinterpret_cast<wl_buffer*>(wl_proxy_marshal_flags(
      reinterpret_cast<wl_proxy*>(pool), 0, &wl_buffer_interface,
      wl_proxy_get_version(reinterpret_cast<wl_proxy*>(pool)), 0, offset, width, height, stride,
      format, nullptr));
}

inline void shm_pool_destroy(wl_shm_pool* pool) {
  wl_proxy_marshal_flags(reinterpret_cast<wl_proxy*>(pool), 1, nullptr,
                         wl_proxy_get_version(reinterpret_cast<wl_proxy*>(pool)), marshal_destroy);
}

inline void buffer_add_listener(wl_buffer* buffer, void (**listener)(void), void* data) {
  wl_proxy_add_listener(reinterpret_cast<wl_proxy*>(buffer), listener, data);
}
inline void buffer_destroy(wl_buffer* buffer) {
  wl_proxy_marshal_flags(reinterpret_cast<wl_proxy*>(buffer), 0, nullptr,
                         wl_proxy_get_version(reinterpret_cast<wl_proxy*>(buffer)), marshal_destroy);
}

inline void surface_attach(wl_surface* surface, wl_buffer* buffer, std::int32_t x,
                           std::int32_t y) {
  wl_proxy_marshal_flags(reinterpret_cast<wl_proxy*>(surface), 1, nullptr,
                         wl_proxy_get_version(reinterpret_cast<wl_proxy*>(surface)), 0, buffer, x,
                         y);
}
inline void surface_damage(wl_surface* surface, std::int32_t x, std::int32_t y, std::int32_t width,
                           std::int32_t height) {
  const auto version = wl_proxy_get_version(reinterpret_cast<wl_proxy*>(surface));
  if (version >= 4) {
    wl_proxy_marshal_flags(reinterpret_cast<wl_proxy*>(surface), 9, nullptr, version, 0, x, y, width,
                           height);
  } else {
    wl_proxy_marshal_flags(reinterpret_cast<wl_proxy*>(surface), 2, nullptr, version, 0, x, y, width,
                           height);
  }
}
inline void surface_commit(wl_surface* surface) {
  wl_proxy_marshal_flags(reinterpret_cast<wl_proxy*>(surface), 6, nullptr,
                         wl_proxy_get_version(reinterpret_cast<wl_proxy*>(surface)), 0);
}
inline void surface_destroy(wl_surface* surface) {
  wl_proxy_marshal_flags(reinterpret_cast<wl_proxy*>(surface), 0, nullptr,
                         wl_proxy_get_version(reinterpret_cast<wl_proxy*>(surface)), marshal_destroy);
}

inline wl_keyboard* seat_get_keyboard(wl_seat* seat) {
  return reinterpret_cast<wl_keyboard*>(wl_proxy_marshal_flags(
      reinterpret_cast<wl_proxy*>(seat), 1, &wl_keyboard_interface,
      wl_proxy_get_version(reinterpret_cast<wl_proxy*>(seat)), 0, nullptr));
}
inline wl_pointer* seat_get_pointer(wl_seat* seat) {
  return reinterpret_cast<wl_pointer*>(wl_proxy_marshal_flags(
      reinterpret_cast<wl_proxy*>(seat), 0, &wl_pointer_interface,
      wl_proxy_get_version(reinterpret_cast<wl_proxy*>(seat)), 0, nullptr));
}
inline void seat_add_listener(wl_seat* seat, void (**listener)(void), void* data) {
  wl_proxy_add_listener(reinterpret_cast<wl_proxy*>(seat), listener, data);
}
inline void seat_destroy(wl_seat* seat) {
  wl_proxy_destroy(reinterpret_cast<wl_proxy*>(seat));
}
inline void keyboard_add_listener(wl_keyboard* keyboard, void (**listener)(void), void* data) {
  wl_proxy_add_listener(reinterpret_cast<wl_proxy*>(keyboard), listener, data);
}
inline void keyboard_destroy(wl_keyboard* keyboard) {
  if (wl_proxy_get_version(reinterpret_cast<wl_proxy*>(keyboard)) >= 3) {
    wl_proxy_marshal_flags(reinterpret_cast<wl_proxy*>(keyboard), 0, nullptr,
                           wl_proxy_get_version(reinterpret_cast<wl_proxy*>(keyboard)),
                           marshal_destroy);
  } else {
    wl_proxy_destroy(reinterpret_cast<wl_proxy*>(keyboard));
  }
}
inline void pointer_add_listener(wl_pointer* pointer, void (**listener)(void), void* data) {
  wl_proxy_add_listener(reinterpret_cast<wl_proxy*>(pointer), listener, data);
}
inline void pointer_destroy(wl_pointer* pointer) {
  if (wl_proxy_get_version(reinterpret_cast<wl_proxy*>(pointer)) >= 3) {
    wl_proxy_marshal_flags(reinterpret_cast<wl_proxy*>(pointer), 1, nullptr,
                           wl_proxy_get_version(reinterpret_cast<wl_proxy*>(pointer)),
                           marshal_destroy);
  } else {
    wl_proxy_destroy(reinterpret_cast<wl_proxy*>(pointer));
  }
}

inline xdg_surface* wm_get_xdg_surface(xdg_wm_base* wm, wl_surface* surface) {
  return reinterpret_cast<xdg_surface*>(wl_proxy_marshal_flags(
      reinterpret_cast<wl_proxy*>(wm), 2, &xdg_surface_interface, 1, 0, nullptr, surface));
}
inline void wm_pong(xdg_wm_base* wm, std::uint32_t serial) {
  wl_proxy_marshal_flags(reinterpret_cast<wl_proxy*>(wm), 3, nullptr, 1, 0, serial);
}
inline void wm_destroy(xdg_wm_base* wm) {
  wl_proxy_marshal_flags(reinterpret_cast<wl_proxy*>(wm), 0, nullptr, 1, marshal_destroy);
}
inline void wm_add_listener(xdg_wm_base* wm, void (**listener)(void), void* data) {
  wl_proxy_add_listener(reinterpret_cast<wl_proxy*>(wm), listener, data);
}

inline xdg_toplevel* xdg_surface_get_toplevel(xdg_surface* surface) {
  return reinterpret_cast<xdg_toplevel*>(wl_proxy_marshal_flags(
      reinterpret_cast<wl_proxy*>(surface), 1, &xdg_toplevel_interface, 1, 0, nullptr));
}
inline void xdg_surface_ack_configure(xdg_surface* surface, std::uint32_t serial) {
  wl_proxy_marshal_flags(reinterpret_cast<wl_proxy*>(surface), 4, nullptr, 1, 0, serial);
}
inline void xdg_surface_add_listener(xdg_surface* surface, void (**listener)(void), void* data) {
  wl_proxy_add_listener(reinterpret_cast<wl_proxy*>(surface), listener, data);
}
inline void xdg_surface_destroy(xdg_surface* surface) {
  wl_proxy_marshal_flags(reinterpret_cast<wl_proxy*>(surface), 0, nullptr, 1, marshal_destroy);
}

inline void toplevel_set_title(xdg_toplevel* toplevel, const char* title) {
  wl_proxy_marshal_flags(reinterpret_cast<wl_proxy*>(toplevel), 2, nullptr, 1, 0, title);
}
inline void toplevel_set_app_id(xdg_toplevel* toplevel, const char* app_id) {
  wl_proxy_marshal_flags(reinterpret_cast<wl_proxy*>(toplevel), 3, nullptr, 1, 0, app_id);
}
inline void toplevel_add_listener(xdg_toplevel* toplevel, void (**listener)(void), void* data) {
  wl_proxy_add_listener(reinterpret_cast<wl_proxy*>(toplevel), listener, data);
}
inline void toplevel_destroy(xdg_toplevel* toplevel) {
  wl_proxy_marshal_flags(reinterpret_cast<wl_proxy*>(toplevel), 0, nullptr, 1, marshal_destroy);
}

}  // namespace board::sim::wayland::protocol
