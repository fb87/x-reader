#pragma once

#include "../../core/canvas.hpp"
#include "../../core/capability.hpp"
#include "../../core/connectivity.hpp"
#include "../../core/display.hpp"
#include "../../core/event.hpp"
#include "../../core/geometry.hpp"
#include "../../core/input.hpp"
#include "../../core/platform.hpp"
#include "../../core/refresh.hpp"
#include "../../core/storage.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <dirent.h>
#include <sys/stat.h>
#include <zlib.h>

/**
 * @brief Headless simulator board: the M5Paper logical profile (540x960
 * GRAY4, portrait) over host storage/zlib, ported from `port/sim/sim_port.c`
 * and `port/sim/sim_fatfs.c`. A GUI backend (boards/sim/gui.cpp) sits on
 * top of this for interactive development; autonomous tests use this
 * runtime directly through the injection queue below, with no display
 * server required.
 */
namespace board::sim {

inline constexpr int width = 540;
inline constexpr int height = 960;
inline constexpr auto pixel_format = display::pixel_format::gray4;
inline constexpr std::size_t input_queue_size = 64;

/** @brief A simulated board instance: framebuffer, input queue, and all capability backends. */
struct runtime {
  std::array<std::uint8_t, canvas::buffer_size(width, height, pixel_format)> framebuffer{};
  std::array<event::value, input_queue_size> events{};
  std::size_t event_head = 0;
  std::size_t event_tail = 0;

  std::uint32_t simulated_ms = 0;
  int battery = 87;
  int refresh_count = 0;  ///< bumped on every display update; a GUI backend diffs this to
                          ///< know when to re-present without needing dump_dir enabled.

  char storage_mount[storage::path_max] = {0};  ///< see detail::resolve's doc comment.

  display::device display{};
  platform::device platform{};
  input::device input{};
  storage::device storage{};
  wifi::device wifi{};
  capability::registry capabilities{};

  // Optional diagnostic frame dump (PGM + CSV refresh log), mirroring sim_display_t.
  const char* dump_dir = nullptr;
  const char* dump_name = "sim";
  std::FILE* dump_log = nullptr;
  int dump_frame = 0;
};

namespace detail {

inline const char* refresh_mode_name(refresh::mode mode) {
  switch (mode) {
    case refresh::mode::fast:
      return "FAST";
    case refresh::mode::quality:
      return "QUALITY";
    case refresh::mode::full:
      return "FULL";
    default:
      return "NONE";
  }
}

inline void display_update(display::device& self, geometry::rect area, refresh::mode mode) {
  auto& self_runtime = *static_cast<runtime*>(self.context);
  ++self_runtime.refresh_count;
  if (self_runtime.dump_dir == nullptr) return;
  canvas::surface view{};
  canvas::init(view, self.framebuffer, self.width, self.height, self.stride, self.format);

  char path[512];
  std::snprintf(path, sizeof(path), "%s/%s_%03d.pgm", self_runtime.dump_dir,
                self_runtime.dump_name, self_runtime.dump_frame);
  if (std::FILE* f = std::fopen(path, "wb")) {
    std::fprintf(f, "P5\n%d %d\n255\n", self.width, self.height);
    for (int y = 0; y < self.height; ++y) {
      for (int x = 0; x < self.width; ++x) std::fputc(canvas::get_pixel(view, x, y), f);
    }
    std::fclose(f);
  }
  if (self_runtime.dump_log != nullptr) {
    std::fprintf(self_runtime.dump_log, "%d,%s,%d,%d,%d,%d\n", self_runtime.dump_frame,
                 refresh_mode_name(mode), area.x, area.y, area.w, area.h);
  }
  ++self_runtime.dump_frame;
}

inline std::uint32_t platform_now_ms(platform::device& self) {
  return static_cast<runtime*>(self.context)->simulated_ms;
}

/** @brief Deterministic wall clock starting at 9:41 (the simulator's fixed epoch), advancing
 * with simulated_ms -- the same convention the old sim_port.c used. */
inline void platform_wall_time(platform::device& self, int& hour, int& minute) {
  auto& self_runtime = *static_cast<runtime*>(self.context);
  const std::uint32_t total_minutes = 9 * 60 + 41 + self_runtime.simulated_ms / 60000;
  hour = static_cast<int>(total_minutes / 60) % 24;
  minute = static_cast<int>(total_minutes % 60);
}

inline int platform_battery_percent(platform::device& self) {
  return static_cast<runtime*>(self.context)->battery;
}

inline bool input_poll(input::device& self, event::value& out) {
  auto& self_runtime = *static_cast<runtime*>(self.context);
  if (self_runtime.event_head == self_runtime.event_tail) return false;
  out = self_runtime.events[self_runtime.event_head % input_queue_size];
  ++self_runtime.event_head;
  return true;
}

/**
 * @brief Resolves a storage path the same asymmetric way the old sim_fatfs.c did:
 * `list()` uses its path as-is (BFS always passes already-absolute directory paths),
 * while `read()`/`inflate()`/`file_size()` mount-prefix a relative path (so a book
 * path discovered relative to the scan root, per reader/library.hpp, resolves back
 * to the same host file when later opened for reading).
 */
inline void resolve(const runtime& self, const char* path, char* out, std::size_t out_size) {
  if (path[0] == '/' || self.storage_mount[0] == '\0') {
    std::snprintf(out, out_size, "%s", path);
  } else {
    std::snprintf(out, out_size, "%s/%s", self.storage_mount, path);
  }
}

inline bool storage_list(storage::device& self, const char* path, storage::entry_fn callback,
                         void* user) {
  (void)self;
  DIR* dir = opendir(path);
  if (dir == nullptr) return false;
  struct dirent* ent;
  while ((ent = readdir(dir)) != nullptr) {
    if (std::strcmp(ent->d_name, ".") == 0 || std::strcmp(ent->d_name, "..") == 0) continue;
    char full[1024];
    std::snprintf(full, sizeof(full), "%s/%s", path, ent->d_name);
    struct stat st{};
    const bool directory = stat(full, &st) == 0 && S_ISDIR(st.st_mode);
    storage::entry value{ent->d_name, directory, 0};
    if (!callback(value, user)) {
      closedir(dir);
      return false;
    }
  }
  closedir(dir);
  return true;
}

inline bool storage_read(storage::device& self, const char* path, std::uint32_t offset,
                         void* destination, std::uint32_t size) {
  auto& self_runtime = *static_cast<runtime*>(self.context);
  char full[1024];
  resolve(self_runtime, path, full, sizeof(full));
  std::FILE* f = std::fopen(full, "rb");
  if (f == nullptr) return false;
  const bool ok = std::fseek(f, static_cast<long>(offset), SEEK_SET) == 0 &&
                  std::fread(destination, 1, size, f) == size;
  std::fclose(f);
  return ok;
}

inline bool storage_file_size(storage::device& self, const char* path, std::uint32_t& out) {
  auto& self_runtime = *static_cast<runtime*>(self.context);
  char full[1024];
  resolve(self_runtime, path, full, sizeof(full));
  struct stat st{};
  if (stat(full, &st) != 0) return false;
  out = static_cast<std::uint32_t>(st.st_size);
  return true;
}

inline bool storage_inflate(storage::device& self, const char* path, std::uint32_t source_offset,
                            std::uint32_t source_size, void* destination,
                            std::uint32_t destination_size) {
  auto& self_runtime = *static_cast<runtime*>(self.context);
  char full[1024];
  resolve(self_runtime, path, full, sizeof(full));
  std::FILE* f = std::fopen(full, "rb");
  if (f == nullptr) return false;
  std::uint8_t input[512];
  z_stream stream;
  std::uint32_t remaining = source_size;
  std::memset(&stream, 0, sizeof(stream));
  if (std::fseek(f, static_cast<long>(source_offset), SEEK_SET) != 0 ||
      inflateInit2(&stream, -MAX_WBITS) != Z_OK) {
    std::fclose(f);
    return false;
  }
  stream.next_out = static_cast<Bytef*>(destination);
  stream.avail_out = destination_size;
  bool complete = false;
  for (;;) {
    if (stream.avail_in == 0 && remaining != 0) {
      const std::size_t chunk = remaining > sizeof(input) ? sizeof(input) : remaining;
      if (std::fread(input, 1, chunk, f) != chunk) break;
      remaining -= static_cast<std::uint32_t>(chunk);
      stream.next_in = input;
      stream.avail_in = static_cast<unsigned>(chunk);
    }
    const int result = inflate(&stream, Z_NO_FLUSH);
    if (result == Z_STREAM_END) {
      complete = stream.total_out == destination_size && remaining == 0 && stream.avail_in == 0;
      break;
    }
    if (result != Z_OK || stream.avail_out == 0 || (remaining == 0 && stream.avail_in == 0)) break;
  }
  inflateEnd(&stream);
  std::fclose(f);
  return complete;
}

/** @brief A deterministic, fixed two-network Wi-Fi scan result; connect always succeeds. */
inline bool wifi_scan(wifi::device&, wifi::scan_fn callback, void* user) {
  static constexpr wifi::access_point points[] = {
      {"Reader-Lab", -45, true},
      {"Guest", -70, false},
  };
  for (const auto& point : points) {
    if (!callback(point, user)) return true;
  }
  return true;
}

inline bool wifi_connect(wifi::device&, const char*, const char*) { return true; }
inline void wifi_disconnect(wifi::device&) {}
inline bool wifi_connected(wifi::device&) { return true; }

}  // namespace detail

/** @brief Sets the host directory relative paths are resolved against for reads/inflate. */
inline void mount(runtime& self, const char* root) {
  std::snprintf(self.storage_mount, sizeof(self.storage_mount), "%s", root != nullptr ? root : "");
}

/** @brief Enables PGM+CSV frame dumping to `dir` (framebuffer-dump validation tier). */
inline void enable_dump(runtime& self, const char* dir, const char* name) {
  self.dump_dir = dir;
  self.dump_name = name;
  char path[512];
  std::snprintf(path, sizeof(path), "%s/%s_log.csv", dir, name);
  self.dump_log = std::fopen(path, "w");
  if (self.dump_log != nullptr) std::fprintf(self.dump_log, "frame,mode,x,y,w,h\n");
}

/** @brief Initializes a fresh simulator instance and registers all of its capabilities. */
inline void init(runtime& self) {
  self = runtime{};
  self.display.width = width;
  self.display.height = height;
  self.display.format = pixel_format;
  self.display.framebuffer = self.framebuffer.data();
  self.display.stride = canvas::stride_for(width, pixel_format);
  self.display.update_align = 4;  // matches the real IT8951's 4-pixel alignment.
  self.display.context = &self;
  self.display.update = detail::display_update;

  self.platform.context = &self;
  self.platform.now_ms = detail::platform_now_ms;
  self.platform.wall_time = detail::platform_wall_time;
  self.platform.battery_percent = detail::platform_battery_percent;

  self.input.context = &self;
  self.input.poll = detail::input_poll;

  self.storage.context = &self;
  self.storage.list = detail::storage_list;
  self.storage.read = detail::storage_read;
  self.storage.file_size = detail::storage_file_size;
  self.storage.inflate = detail::storage_inflate;

  self.wifi.context = &self;
  self.wifi.scan = detail::wifi_scan;
  self.wifi.connect = detail::wifi_connect;
  self.wifi.disconnect = detail::wifi_disconnect;
  self.wifi.connected = detail::wifi_connected;

  capability::set(self.capabilities, capability::id::display, &self.display);
  capability::set(self.capabilities, capability::id::input, &self.input);
  capability::set(self.capabilities, capability::id::platform, &self.platform);
  capability::set(self.capabilities, capability::id::storage, &self.storage);
  capability::set(self.capabilities, capability::id::wifi, &self.wifi);
  // bluetooth/power/rtc/front_light/usb/secret intentionally left unregistered: no board
  // backs them yet (secret lands in a later phase; the rest have no real hardware at all).
}

/** @brief Appends one event to the injection queue; drops it when the queue is full. */
inline bool inject(runtime& self, event::value value) {
  if (self.event_tail - self.event_head >= input_queue_size) return false;
  self.events[self.event_tail % input_queue_size] = value;
  ++self.event_tail;
  return true;
}

inline bool rotary_left(runtime& self) { return inject(self, event::key(event::key_code::up)); }
inline bool rotary_right(runtime& self) { return inject(self, event::key(event::key_code::down)); }
inline bool rotary_push(runtime& self) { return inject(self, event::key(event::key_code::ok)); }
inline bool touch(runtime& self, int x, int y) { return inject(self, event::tap(x, y)); }

/** @brief Advances the deterministic simulated clock by `milliseconds`. */
inline void advance(runtime& self, std::uint32_t milliseconds) { self.simulated_ms += milliseconds; }

}  // namespace board::sim
