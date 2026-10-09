#pragma once

#include <dirent.h>
#include <sys/stat.h>
#include <zlib.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "core/capability.hpp"
#include "core/connectivity.hpp"
#include "core/display.hpp"
#include "core/event.hpp"
#include "core/fixed_queue.hpp"
#include "core/input.hpp"
#include "core/platform.hpp"
#include "core/storage.hpp"

namespace board::sim {

inline constexpr int width = 540;
inline constexpr int height = 960;
inline constexpr int stride = width / 2;
inline constexpr std::size_t framebuffer_size = static_cast<std::size_t>(stride) * height;
inline constexpr std::size_t input_queue_size = 128;
inline constexpr std::size_t max_files = 16;

/** @brief One in-memory simulator file used to emulate an SD-card root. */
struct file {
  const char* name = nullptr;
  const char* content = nullptr;
};

/** @brief Simulator runtime matching the M5Paper logical I/O profile. */
struct runtime {
  std::array<std::uint8_t, framebuffer_size> framebuffer{};
  fixed_queue::queue<event::value, input_queue_size> events{};
  int refresh_count = 0;
  refresh::mode last_refresh = refresh::mode::full;
  int battery = 87;
  std::uint32_t simulated_ms = 0;
  bool wifi_connected = false;
  const char* wifi_ssid = nullptr;
  std::array<file, max_files> files{};
  std::size_t file_count = 0;
  char storage_root[storage::path_max] = {0};
  bool host_storage = false;

  display::device display{};
  input::device input{};
  platform::device platform{};
  storage::device storage{};
  wifi::device wifi{};
  capability::registry capabilities{};
};

/** @brief Records a display refresh request for deterministic tests. */
inline void display_update(display::device& self, geometry::rect area, refresh::mode mode) {
  auto& sim = *static_cast<runtime*>(self.context);
  if (area.w <= 0 || area.h <= 0) {
    return;
  }
  ++sim.refresh_count;
  sim.last_refresh = mode;
}

/** @brief Polls one queued input event. */
inline bool input_poll(input::device& self, event::value& out) {
  auto& sim = *static_cast<runtime*>(self.context);
  return fixed_queue::pop(sim.events, out);
}

/** @brief Returns deterministic simulator time. */
inline std::uint32_t now_ms(platform::device& self) {
  return static_cast<runtime*>(self.context)->simulated_ms;
}

inline bool wall_time(platform::device& self, int& hour, int& minute) {
  auto& sim = *static_cast<runtime*>(self.context);
  const std::uint32_t total = 9U * 60U + 41U + sim.simulated_ms / 60000U;
  hour = static_cast<int>((total / 60U) % 24U);
  minute = static_cast<int>(total % 60U);
  return true;
}

/** @brief Returns the simulated battery percentage. */
inline int battery_percent(platform::device& self) {
  return static_cast<runtime*>(self.context)->battery;
}

inline void enter_deep_sleep(platform::device&, std::uint32_t) {}
inline bool woke_from_deep_sleep(platform::device&) { return false; }

/** @brief Enumerates immediate children of one simulated SD-card directory. */
inline bool storage_list(storage::device& self, const char* path, storage::entry_fn callback,
                         void* user) {
  auto& sim = *static_cast<runtime*>(self.context);
  if (sim.host_storage) {
    DIR* directory = ::opendir(path == nullptr || path[0] == '\0' ? sim.storage_root : path);
    if (directory == nullptr) return false;
    struct dirent* item = nullptr;
    while ((item = ::readdir(directory)) != nullptr) {
      if (std::strcmp(item->d_name, ".") == 0 || std::strcmp(item->d_name, "..") == 0) continue;
      char child[storage::path_max * 2] = {0};
      const char* base = path == nullptr || path[0] == '\0' ? sim.storage_root : path;
      std::snprintf(child, sizeof(child), "%s/%s", base, item->d_name);
      struct stat info{};
      if (::stat(child, &info) != 0) continue;
      const storage::entry value{
          item->d_name, S_ISDIR(info.st_mode),
          S_ISREG(info.st_mode) ? static_cast<std::uint32_t>(info.st_size) : 0};
      if (!callback(value, user)) break;
    }
    ::closedir(directory);
    return true;
  }
  char prefix[storage::path_max]{};
  const char* source = path == nullptr ? "" : path;
  while (*source == '/') ++source;
  if (*source != '\0') std::snprintf(prefix, sizeof(prefix), "%s/", source);

  char emitted[32][128]{};
  std::size_t emitted_count = 0;
  for (std::size_t i = 0; i < sim.file_count; ++i) {
    const char* full = sim.files[i].name;
    if (full == nullptr) continue;
    if (prefix[0] != '\0' && std::strncmp(full, prefix, std::strlen(prefix)) != 0) continue;
    const char* relative = full + std::strlen(prefix);
    if (*relative == '\0') continue;
    const char* slash = std::strchr(relative, '/');
    char name[128]{};
    const bool directory = slash != nullptr;
    const std::size_t name_length =
        directory ? static_cast<std::size_t>(slash - relative) : std::strlen(relative);
    if (name_length == 0 || name_length >= sizeof(name)) continue;
    std::memcpy(name, relative, name_length);
    name[name_length] = '\0';
    bool duplicate = false;
    for (std::size_t j = 0; j < emitted_count; ++j)
      if (std::strcmp(emitted[j], name) == 0) duplicate = true;
    if (duplicate) continue;
    if (emitted_count < 32) std::snprintf(emitted[emitted_count++], sizeof(emitted[0]), "%s", name);
    const storage::entry item{
        .name = name,
        .directory = directory,
        .size = directory || sim.files[i].content == nullptr
                    ? 0U
                    : static_cast<std::uint32_t>(std::strlen(sim.files[i].content))};
    if (!callback(item, user)) break;
  }
  return true;
}

/** @brief Reads bytes from a simulated file. */
inline bool storage_read(storage::device& self, const char* path, std::uint32_t offset,
                         void* destination, std::uint32_t size) {
  auto& sim = *static_cast<runtime*>(self.context);
  if (sim.host_storage) {
    char full[storage::path_max * 2] = {0};
    const char* value = path == nullptr ? "" : path;
    const std::size_t root_length = std::strlen(sim.storage_root);
    if (std::strncmp(value, sim.storage_root, root_length) == 0 &&
        (value[root_length] == '/' || value[root_length] == '\0'))
      std::snprintf(full, sizeof(full), "%s", value);
    else
      std::snprintf(full, sizeof(full), "%s/%s", sim.storage_root, value);
    std::FILE* file = std::fopen(full, "rb");
    if (file == nullptr) return false;
    const bool ok = std::fseek(file, static_cast<long>(offset), SEEK_SET) == 0 &&
                    std::fread(destination, 1, size, file) == size;
    std::fclose(file);
    return ok;
  }
  const char* name = path;
  if (name != nullptr && name[0] == '/') {
    ++name;
  }
  for (std::size_t i = 0; i < sim.file_count; ++i) {
    if (name != nullptr && std::strcmp(sim.files[i].name, name) == 0) {
      const auto length = std::strlen(sim.files[i].content);
      if (offset > length || size > length - offset) {
        return false;
      }
      std::memcpy(destination, sim.files[i].content + offset, size);
      return true;
    }
  }
  return false;
}

/** @brief Returns the size of a simulated file. */
inline bool storage_size(storage::device& self, const char* path, std::uint32_t& out) {
  auto& sim = *static_cast<runtime*>(self.context);
  if (sim.host_storage) {
    char full[storage::path_max * 2] = {0};
    const char* value = path == nullptr ? "" : path;
    const std::size_t root_length = std::strlen(sim.storage_root);
    if (std::strncmp(value, sim.storage_root, root_length) == 0 &&
        (value[root_length] == '/' || value[root_length] == '\0'))
      std::snprintf(full, sizeof(full), "%s", value);
    else
      std::snprintf(full, sizeof(full), "%s/%s", sim.storage_root, value);
    struct stat info{};
    if (::stat(full, &info) != 0 || !S_ISREG(info.st_mode)) return false;
    out = static_cast<std::uint32_t>(info.st_size);
    return true;
  }
  const char* name = path;
  if (name != nullptr && name[0] == '/') {
    ++name;
  }
  for (std::size_t i = 0; i < sim.file_count; ++i) {
    if (name != nullptr && std::strcmp(sim.files[i].name, name) == 0) {
      out = static_cast<std::uint32_t>(std::strlen(sim.files[i].content));
      return true;
    }
  }
  return false;
}

/** @brief Inflates one raw DEFLATE stream for EPUB ZIP entries. */
inline bool storage_inflate(storage::device&, const void* source, std::uint32_t source_size,
                            void* destination, std::uint32_t destination_size) {
  z_stream stream{};
  stream.next_in = reinterpret_cast<Bytef*>(const_cast<void*>(source));
  stream.avail_in = source_size;
  stream.next_out = reinterpret_cast<Bytef*>(destination);
  stream.avail_out = destination_size;
  if (::inflateInit2(&stream, -MAX_WBITS) != Z_OK) return false;
  const int result = ::inflate(&stream, Z_FINISH);
  ::inflateEnd(&stream);
  return result == Z_STREAM_END && stream.total_out == destination_size;
}

/** @brief Enumerates deterministic simulator Wi-Fi networks. */
inline bool wifi_scan(wifi::device&, wifi::scan_fn callback, void* user) {
  const wifi::access_point networks[] = {
      {.ssid = "Reader-Lab", .rssi = -38, .secured = true},
      {.ssid = "Guest", .rssi = -67, .secured = false},
  };
  for (const auto& item : networks) {
    if (!callback(item, user)) {
      break;
    }
  }
  return true;
}

/** @brief Connects simulator Wi-Fi when a non-empty SSID is supplied. */
inline bool wifi_connect(wifi::device& self, const char* ssid, const char*) {
  auto& sim = *static_cast<runtime*>(self.context);
  if (ssid == nullptr || ssid[0] == '\0') {
    return false;
  }
  sim.wifi_connected = true;
  sim.wifi_ssid = ssid;
  return true;
}

/** @brief Disconnects simulator Wi-Fi. */
inline void wifi_disconnect(wifi::device& self) {
  auto& sim = *static_cast<runtime*>(self.context);
  sim.wifi_connected = false;
  sim.wifi_ssid = nullptr;
}

/** @brief Reports simulator Wi-Fi connection state. */
inline bool wifi_connected(wifi::device& self) {
  return static_cast<runtime*>(self.context)->wifi_connected;
}

/** @brief Initializes simulator devices and publishes their generic capabilities. */
inline void init(runtime& self) {
  self.files[0] = {.name = "books/classics/pride_and_prejudice.epub",
                   .content = "EPUB simulated content A"};
  self.files[1] = {.name = "books/scifi/the_time_machine.epub",
                   .content = "EPUB simulated content B"};
  self.files[2] = {.name = "notes.txt", .content = "ignored"};
  self.file_count = 3;

  self.display = {.width = width,
                  .height = height,
                  .stride = stride,
                  .format = display::pixel_format::gray4,
                  .framebuffer = self.framebuffer.data(),
                  .context = &self,
                  .update = display_update};
  self.input = {.context = &self, .poll = input_poll};
  self.platform = {.context = &self,
                   .now_ms = now_ms,
                   .wall_time = wall_time,
                   .battery_percent = battery_percent,
                   .enter_deep_sleep = enter_deep_sleep,
                   .woke_from_deep_sleep = woke_from_deep_sleep};
  self.storage = {.context = &self,
                  .root = "/",
                  .list = storage_list,
                  .read = storage_read,
                  .size = storage_size,
                  .inflate = storage_inflate};
  self.wifi = {.context = &self,
               .scan = wifi_scan,
               .connect = wifi_connect,
               .disconnect = wifi_disconnect,
               .connected = wifi_connected};

  capability::set(self.capabilities, capability::id::display, &self.display);
  capability::set(self.capabilities, capability::id::input, &self.input);
  capability::set(self.capabilities, capability::id::platform, &self.platform);
  capability::set(self.capabilities, capability::id::storage, &self.storage);
  capability::set(self.capabilities, capability::id::wifi, &self.wifi);
}

/** @brief Switches the interactive simulator to a host-backed SD-card directory. */
inline bool mount(runtime& self, const char* root) {
  if (root == nullptr || root[0] == '\0') return false;
  struct stat info{};
  if (::stat(root, &info) != 0 || !S_ISDIR(info.st_mode)) return false;
  std::snprintf(self.storage_root, sizeof(self.storage_root), "%s", root);
  self.host_storage = true;
  self.storage.root = self.storage_root;
  return true;
}

/** @brief Injects an event into the simulator queue. */
inline bool inject(runtime& self, event::value value) {
  return fixed_queue::push(self.events, value);
}

/** @brief Injects M5Paper rotary-left behavior. */
inline bool rotary_left(runtime& self) { return inject(self, event::key(event::key_code::up)); }

/** @brief Injects M5Paper rotary-right behavior. */
inline bool rotary_right(runtime& self) { return inject(self, event::key(event::key_code::down)); }

/** @brief Injects M5Paper rotary-push behavior. */
inline bool rotary_push(runtime& self) { return inject(self, event::key(event::key_code::ok)); }

/** @brief Injects a touchscreen tap in M5Paper logical coordinates. */
inline bool touch(runtime& self, int x, int y) {
  if (x < 0 || y < 0 || x >= width || y >= height) {
    return false;
  }
  return inject(self, event::tap(x, y));
}

/** @brief Advances deterministic simulator time. */
inline void advance(runtime& self, std::uint32_t milliseconds) {
  self.simulated_ms += milliseconds;
}

/** @brief Writes the current gray4 framebuffer as a portable PGM image. */
inline bool dump_framebuffer(const runtime& self, const char* path) {
  std::FILE* output = std::fopen(path, "wb");
  if (output == nullptr) {
    return false;
  }
  std::fprintf(output, "P5\n%d %d\n255\n", width, height);
  for (int y = 0; y < height; ++y) {
    for (int x = 0; x < width; ++x) {
      const auto byte = self.framebuffer[static_cast<std::size_t>(y) * stride + x / 2];
      const auto nibble = (x & 1) == 0 ? static_cast<std::uint8_t>(byte >> 4U)
                                       : static_cast<std::uint8_t>(byte & 0x0fU);
      const auto value = static_cast<std::uint8_t>(nibble * 17U);
      std::fwrite(&value, 1, 1, output);
    }
  }
  return std::fclose(output) == 0;
}

}  // namespace board::sim
