#pragma once

#include "boards/sim/runtime.hpp"
#include "boards/sim/wayland_protocol.hpp"

#include <algorithm>
#include <array>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <poll.h>
#include <sys/mman.h>
#include <sys/syscall.h>
#include <unistd.h>

namespace board::sim::wayland {

namespace p = protocol;

inline constexpr int scale = 1;
inline constexpr int pixel_stride = board::sim::width * 4;
inline constexpr int buffer_size = pixel_stride * board::sim::height;
inline constexpr std::uint32_t key_esc = 1;
inline constexpr std::uint32_t key_backspace = 14;
inline constexpr std::uint32_t key_enter = 28;
inline constexpr std::uint32_t key_m = 50;
inline constexpr std::uint32_t key_space = 57;
inline constexpr std::uint32_t key_f5 = 63;
inline constexpr std::uint32_t key_up = 103;
inline constexpr std::uint32_t key_left = 105;
inline constexpr std::uint32_t key_right = 106;
inline constexpr std::uint32_t key_down = 108;

struct held_key {
  event::key_code code = event::key_code::none;
  std::chrono::steady_clock::time_point started{};
  bool down = false;
};

struct shm_buffer {
  wl_buffer* handle = nullptr;
  std::uint8_t* data = nullptr;
  bool busy = false;
};

struct backend {
  wl_display* display = nullptr;
  wl_registry* registry = nullptr;
  wl_compositor* compositor = nullptr;
  wl_shm* shm = nullptr;
  wl_seat* seat = nullptr;
  wl_keyboard* keyboard = nullptr;
  wl_pointer* pointer = nullptr;
  p::xdg_wm_base* wm_base = nullptr;
  wl_surface* surface = nullptr;
  p::xdg_surface* xdg_surface = nullptr;
  p::xdg_toplevel* toplevel = nullptr;
  std::array<shm_buffer, 2> buffers{};
  int shm_fd = -1;
  void* shm_map = MAP_FAILED;
  bool configured = false;
  bool running = true;
  bool dirty = true;
  double pointer_x = 0.0;
  double pointer_y = 0.0;
  held_key held{};
  bool manual_reload = false;
  board::sim::runtime* sim = nullptr;
};

inline event::key_code map_key(std::uint32_t key) {
  switch (key) {
    case key_up: return event::key_code::up;
    case key_down: return event::key_code::down;
    case key_left: return event::key_code::left;
    case key_right: return event::key_code::right;
    case key_enter:
    case key_space: return event::key_code::ok;
    case key_esc:
    case key_backspace: return event::key_code::back;
    case key_m: return event::key_code::menu;
    default: return event::key_code::none;
  }
}

inline void start_key(backend& self, std::uint32_t key) {
  if (key == key_f5) return;
  const auto code = map_key(key);
  if (code == event::key_code::none || self.held.down) return;
  self.held = {.code = code, .started = std::chrono::steady_clock::now(), .down = true};
}

inline void release_key(backend& self, std::uint32_t key) {
  if (key == key_f5) {
    self.manual_reload = true;
    return;
  }
  const auto code = map_key(key);
  if (!self.held.down || code == event::key_code::none || code != self.held.code || self.sim == nullptr) {
    return;
  }
  const auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
      std::chrono::steady_clock::now() - self.held.started).count();
  const auto ms = static_cast<std::uint32_t>(std::max<std::int64_t>(duration, 0));
  board::sim::inject(*self.sim, event::key(code, ms, ms >= event::long_press_threshold_ms));
  self.held = {};
}

inline int create_memfd() {
#ifdef SYS_memfd_create
  return static_cast<int>(::syscall(SYS_memfd_create, "ebook-reader-sim", 0x0001U));
#else
  return -1;
#endif
}

inline void buffer_release(void* data, wl_buffer*) {
  static_cast<shm_buffer*>(data)->busy = false;
}

inline void registry_global(void* data, wl_registry* registry, std::uint32_t name,
                            const char* interface, std::uint32_t version) {
  auto& self = *static_cast<backend*>(data);
  if (std::strcmp(interface, "wl_compositor") == 0) {
    const auto use_version = std::min(version, 4U);
    self.compositor = reinterpret_cast<wl_compositor*>(
        p::registry_bind(registry, name, &wl_compositor_interface, use_version));
  } else if (std::strcmp(interface, "wl_shm") == 0) {
    self.shm = reinterpret_cast<wl_shm*>(p::registry_bind(registry, name, &wl_shm_interface, 1));
  } else if (std::strcmp(interface, "wl_seat") == 0) {
    self.seat = reinterpret_cast<wl_seat*>(
        p::registry_bind(registry, name, &wl_seat_interface, std::min(version, 5U)));
  } else if (std::strcmp(interface, "xdg_wm_base") == 0) {
    self.wm_base = reinterpret_cast<p::xdg_wm_base*>(
        p::registry_bind(registry, name, &p::xdg_wm_base_interface, 1));
  }
}
inline void registry_global_remove(void*, wl_registry*, std::uint32_t) {}

inline void wm_ping(void*, p::xdg_wm_base* wm, std::uint32_t serial) { p::wm_pong(wm, serial); }

inline void xdg_surface_configure(void* data, p::xdg_surface* surface, std::uint32_t serial) {
  auto& self = *static_cast<backend*>(data);
  p::xdg_surface_ack_configure(surface, serial);
  self.configured = true;
  self.dirty = true;
}

inline void toplevel_configure(void*, p::xdg_toplevel*, std::int32_t, std::int32_t, wl_array*) {}
inline void toplevel_close(void* data, p::xdg_toplevel*) {
  static_cast<backend*>(data)->running = false;
}

inline void seat_capabilities(void* data, wl_seat* seat, std::uint32_t capabilities);
inline void seat_name(void*, wl_seat*, const char*) {}

inline void keyboard_keymap(void*, wl_keyboard*, std::uint32_t, int fd, std::uint32_t) {
  if (fd >= 0) ::close(fd);
}
inline void keyboard_enter(void*, wl_keyboard*, std::uint32_t, wl_surface*, wl_array*) {}
inline void keyboard_leave(void* data, wl_keyboard*, std::uint32_t, wl_surface*) {
  static_cast<backend*>(data)->held = {};
}
inline void keyboard_key(void* data, wl_keyboard*, std::uint32_t, std::uint32_t, std::uint32_t key,
                         std::uint32_t state) {
  auto& self = *static_cast<backend*>(data);
  if (state == p::key_pressed) {
    start_key(self, key);
  } else if (state == p::key_released) {
    release_key(self, key);
  }
}
inline void keyboard_modifiers(void*, wl_keyboard*, std::uint32_t, std::uint32_t, std::uint32_t,
                               std::uint32_t, std::uint32_t) {}
inline void keyboard_repeat_info(void*, wl_keyboard*, std::int32_t, std::int32_t) {}

inline double from_fixed(wl_fixed_t value) { return static_cast<double>(value) / 256.0; }
inline void pointer_enter(void* data, wl_pointer*, std::uint32_t, wl_surface*, wl_fixed_t x,
                          wl_fixed_t y) {
  auto& self = *static_cast<backend*>(data);
  self.pointer_x = from_fixed(x);
  self.pointer_y = from_fixed(y);
}
inline void pointer_leave(void*, wl_pointer*, std::uint32_t, wl_surface*) {}
inline void pointer_motion(void* data, wl_pointer*, std::uint32_t, wl_fixed_t x, wl_fixed_t y) {
  auto& self = *static_cast<backend*>(data);
  self.pointer_x = from_fixed(x);
  self.pointer_y = from_fixed(y);
}
inline void pointer_button(void* data, wl_pointer*, std::uint32_t, std::uint32_t,
                           std::uint32_t button, std::uint32_t state) {
  auto& self = *static_cast<backend*>(data);
  if (self.sim == nullptr || button != p::btn_left || state != p::pointer_button_pressed) return;
  board::sim::touch(*self.sim, static_cast<int>(self.pointer_x) / scale,
                    static_cast<int>(self.pointer_y) / scale);
}
inline void pointer_axis(void* data, wl_pointer*, std::uint32_t, std::uint32_t axis,
                         wl_fixed_t value) {
  auto& self = *static_cast<backend*>(data);
  if (self.sim == nullptr || axis != p::pointer_axis_vertical_scroll || value == 0) return;
  if (value < 0) {
    board::sim::rotary_left(*self.sim);
  } else {
    board::sim::rotary_right(*self.sim);
  }
}
inline void pointer_frame(void*, wl_pointer*) {}
inline void pointer_axis_source(void*, wl_pointer*, std::uint32_t) {}
inline void pointer_axis_stop(void*, wl_pointer*, std::uint32_t, std::uint32_t) {}
inline void pointer_axis_discrete(void*, wl_pointer*, std::uint32_t, std::int32_t) {}

inline void seat_capabilities(void* data, wl_seat* seat, std::uint32_t capabilities) {
  auto& self = *static_cast<backend*>(data);
  if ((capabilities & p::seat_capability_keyboard) != 0U && self.keyboard == nullptr) {
    self.keyboard = p::seat_get_keyboard(seat);
    static void (*listener[])(void) = {
        reinterpret_cast<void (*)(void)>(keyboard_keymap),
        reinterpret_cast<void (*)(void)>(keyboard_enter),
        reinterpret_cast<void (*)(void)>(keyboard_leave),
        reinterpret_cast<void (*)(void)>(keyboard_key),
        reinterpret_cast<void (*)(void)>(keyboard_modifiers),
        reinterpret_cast<void (*)(void)>(keyboard_repeat_info),
    };
    p::keyboard_add_listener(self.keyboard, listener, &self);
  } else if ((capabilities & p::seat_capability_keyboard) == 0U && self.keyboard != nullptr) {
    p::keyboard_destroy(self.keyboard);
    self.keyboard = nullptr;
    self.held = {};
  }

  if ((capabilities & p::seat_capability_pointer) != 0U && self.pointer == nullptr) {
    self.pointer = p::seat_get_pointer(seat);
    static void (*listener[])(void) = {
        reinterpret_cast<void (*)(void)>(pointer_enter),
        reinterpret_cast<void (*)(void)>(pointer_leave),
        reinterpret_cast<void (*)(void)>(pointer_motion),
        reinterpret_cast<void (*)(void)>(pointer_button),
        reinterpret_cast<void (*)(void)>(pointer_axis),
        reinterpret_cast<void (*)(void)>(pointer_frame),
        reinterpret_cast<void (*)(void)>(pointer_axis_source),
        reinterpret_cast<void (*)(void)>(pointer_axis_stop),
        reinterpret_cast<void (*)(void)>(pointer_axis_discrete),
    };
    p::pointer_add_listener(self.pointer, listener, &self);
  } else if ((capabilities & p::seat_capability_pointer) == 0U && self.pointer != nullptr) {
    p::pointer_destroy(self.pointer);
    self.pointer = nullptr;
  }
}

inline bool create_buffers(backend& self) {
  self.shm_fd = create_memfd();
  if (self.shm_fd < 0) return false;
  const int total_size = buffer_size * static_cast<int>(self.buffers.size());
  if (::ftruncate(self.shm_fd, total_size) != 0) return false;
  self.shm_map = ::mmap(nullptr, static_cast<std::size_t>(total_size), PROT_READ | PROT_WRITE,
                        MAP_SHARED, self.shm_fd, 0);
  if (self.shm_map == MAP_FAILED) return false;

  wl_shm_pool* pool = p::shm_create_pool(self.shm, self.shm_fd, total_size);
  if (pool == nullptr) return false;
  static void (*release_listener[])(void) = {
      reinterpret_cast<void (*)(void)>(buffer_release),
  };
  for (std::size_t i = 0; i < self.buffers.size(); ++i) {
    auto& buffer = self.buffers[i];
    buffer.data = static_cast<std::uint8_t*>(self.shm_map) + i * buffer_size;
    buffer.handle = p::shm_pool_create_buffer(pool, static_cast<std::int32_t>(i * buffer_size),
                                              board::sim::width, board::sim::height, pixel_stride,
                                              p::shm_format_xrgb8888);
    if (buffer.handle == nullptr) {
      p::shm_pool_destroy(pool);
      return false;
    }
    p::buffer_add_listener(buffer.handle, release_listener, &buffer);
  }
  p::shm_pool_destroy(pool);
  return true;
}

inline bool open(backend& self, board::sim::runtime& sim, const char* title) {
  self.sim = &sim;
  self.display = wl_display_connect(nullptr);
  if (self.display == nullptr) {
    std::fprintf(stderr, "simulator: cannot connect to Wayland display (WAYLAND_DISPLAY)\n");
    return false;
  }

  self.registry = p::display_get_registry(self.display);
  if (self.registry == nullptr) return false;
  static void (*registry_listener[])(void) = {
      reinterpret_cast<void (*)(void)>(registry_global),
      reinterpret_cast<void (*)(void)>(registry_global_remove),
  };
  p::registry_add_listener(self.registry, registry_listener, &self);
  if (wl_display_roundtrip(self.display) < 0 || self.compositor == nullptr || self.shm == nullptr ||
      self.wm_base == nullptr) {
    std::fprintf(stderr, "simulator: compositor lacks required Wayland globals\n");
    return false;
  }

  static void (*wm_listener[])(void) = {reinterpret_cast<void (*)(void)>(wm_ping)};
  p::wm_add_listener(self.wm_base, wm_listener, &self);

  if (self.seat != nullptr) {
    static void (*seat_listener[])(void) = {
        reinterpret_cast<void (*)(void)>(seat_capabilities),
        reinterpret_cast<void (*)(void)>(seat_name),
    };
    p::seat_add_listener(self.seat, seat_listener, &self);
    if (wl_display_roundtrip(self.display) < 0) return false;
  }

  self.surface = p::compositor_create_surface(self.compositor);
  if (self.surface == nullptr) return false;
  self.xdg_surface = p::wm_get_xdg_surface(self.wm_base, self.surface);
  if (self.xdg_surface == nullptr) return false;
  static void (*surface_listener[])(void) = {
      reinterpret_cast<void (*)(void)>(xdg_surface_configure),
  };
  p::xdg_surface_add_listener(self.xdg_surface, surface_listener, &self);

  self.toplevel = p::xdg_surface_get_toplevel(self.xdg_surface);
  if (self.toplevel == nullptr) return false;
  static void (*toplevel_listener[])(void) = {
      reinterpret_cast<void (*)(void)>(toplevel_configure),
      reinterpret_cast<void (*)(void)>(toplevel_close),
  };
  p::toplevel_add_listener(self.toplevel, toplevel_listener, &self);
  p::toplevel_set_title(self.toplevel, title);
  p::toplevel_set_app_id(self.toplevel, "ebook-reader-simulator");

  if (!create_buffers(self)) {
    std::fprintf(stderr, "simulator: failed to create Wayland shared-memory buffers\n");
    return false;
  }

  p::surface_commit(self.surface);
  while (!self.configured && self.running) {
    if (wl_display_dispatch(self.display) < 0) return false;
  }
  return self.configured;
}

inline bool pump(backend& self) {
  if (self.display == nullptr || !self.running) return false;
  if (wl_display_dispatch_pending(self.display) < 0) {
    self.running = false;
    return false;
  }
  wl_display_flush(self.display);
  pollfd fd{.fd = wl_display_get_fd(self.display), .events = POLLIN, .revents = 0};
  const int ready = ::poll(&fd, 1, 0);
  if (ready > 0 && (fd.revents & POLLIN) != 0 && wl_display_dispatch(self.display) < 0) {
    self.running = false;
  }
  return self.running;
}

inline bool present(backend& self, const board::sim::runtime& sim) {
  if (!self.configured) return false;
  auto it = std::find_if(self.buffers.begin(), self.buffers.end(),
                         [](const shm_buffer& buffer) { return !buffer.busy; });
  if (it == self.buffers.end()) return false;

  for (int y = 0; y < board::sim::height; ++y) {
    auto* row = reinterpret_cast<std::uint32_t*>(it->data + y * pixel_stride);
    for (int x = 0; x < board::sim::width; ++x) {
      const auto packed = sim.framebuffer[static_cast<std::size_t>(y) * board::sim::stride + x / 2];
      const auto nibble = (x & 1) == 0 ? static_cast<std::uint8_t>(packed >> 4U)
                                       : static_cast<std::uint8_t>(packed & 0x0fU);
      const auto gray = static_cast<std::uint32_t>(nibble * 17U);
      row[x] = (gray << 16U) | (gray << 8U) | gray;
    }
  }

  it->busy = true;
  p::surface_attach(self.surface, it->handle, 0, 0);
  p::surface_damage(self.surface, 0, 0, board::sim::width, board::sim::height);
  p::surface_commit(self.surface);
  wl_display_flush(self.display);
  self.dirty = false;
  return true;
}

inline bool take_manual_reload(backend& self) {
  const bool requested = self.manual_reload;
  self.manual_reload = false;
  return requested;
}

inline void close(backend& self) {
  if (self.pointer != nullptr) p::pointer_destroy(self.pointer);
  if (self.keyboard != nullptr) p::keyboard_destroy(self.keyboard);
  if (self.seat != nullptr) p::seat_destroy(self.seat);
  for (auto& buffer : self.buffers) {
    if (buffer.handle != nullptr) p::buffer_destroy(buffer.handle);
  }
  if (self.shm_map != MAP_FAILED) {
    ::munmap(self.shm_map, static_cast<std::size_t>(buffer_size * self.buffers.size()));
  }
  if (self.shm_fd >= 0) ::close(self.shm_fd);
  if (self.toplevel != nullptr) p::toplevel_destroy(self.toplevel);
  if (self.xdg_surface != nullptr) p::xdg_surface_destroy(self.xdg_surface);
  if (self.surface != nullptr) p::surface_destroy(self.surface);
  if (self.wm_base != nullptr) p::wm_destroy(self.wm_base);
  if (self.shm != nullptr) wl_proxy_destroy(reinterpret_cast<wl_proxy*>(self.shm));
  if (self.compositor != nullptr) wl_proxy_destroy(reinterpret_cast<wl_proxy*>(self.compositor));
  if (self.registry != nullptr) wl_proxy_destroy(reinterpret_cast<wl_proxy*>(self.registry));
  if (self.display != nullptr) wl_display_disconnect(self.display);
  self = {};
}

}  // namespace board::sim::wayland
