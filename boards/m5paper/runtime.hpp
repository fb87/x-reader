#pragma once

#include "../../core/canvas.hpp"
#include "../../core/capability.hpp"
#include "../../core/display.hpp"
#include "../../core/event.hpp"
#include "../../core/geometry.hpp"
#include "../../core/input.hpp"
#include "../../core/platform.hpp"
#include "../../core/refresh.hpp"
#include "../../core/storage.hpp"
#include "../../drivers/gt911/gt911.hpp"
#include "../../drivers/inflate/inflate.hpp"
#include "../../drivers/it8951/it8951.hpp"
#include "board.hpp"
#include "pins.hpp"

#include <climits>
#include <cstdio>
#include <cstring>
#include <dirent.h>
#include <driver/gpio.h>
#include <esp_heap_caps.h>
#include <esp_log.h>
#include <esp_timer.h>
#include <esp_vfs_fat.h>
#include <driver/sdspi_host.h>
#include <sys/stat.h>

/**
 * @brief M5Paper hardware board: composes the IT8951/GT911/inflate drivers
 * and ESP-IDF's SD/FatFS VFS into the same generic capability surface
 * `boards/sim/runtime.hpp` publishes, ported from the composition logic in
 * `port/m5paper/app_main.cpp` (display_update/poll_input/epub_read/
 * epub_inflate/list_sd_directory/mount_and_scan_sd).
 *
 * UNVERIFIED IN THIS SANDBOX: no ESP-IDF toolchain is available here. This
 * file is code + build wiring believed correct via careful preservation of
 * the old composition logic, not a compiled/tested result -- see
 * docs/MIGRATION.md. It is not claimed hardware-verified until the user
 * builds and flashes it onto real M5Paper hardware (`idf.py build`/
 * `idf.py flash monitor`) and runs the manual checklist in docs/MIGRATION.md
 * (rotation correctness, page-turn latency/no watchdog reset, touch at
 * both I2C addresses, battery sanity, sleep/wake via both ext0 and timer).
 *
 * Gap closed during this port: the old `enter_deep_sleep()` existed in
 * board.cpp but was never called from app_main.cpp's loop. Here it is
 * wired through `platform::device.enter_deep_sleep`, which Phase 7g's
 * sleep page already calls generically.
 */
namespace board::m5paper {

inline constexpr const char* log_tag = "m5paper";

inline constexpr int width = 540;   // logical (post-rotation) width, matches board::sim.
inline constexpr int height = 960;  // logical (post-rotation) height, matches board::sim.
inline constexpr auto pixel_format = display::pixel_format::gray4;
inline constexpr std::size_t input_queue_size = 8;
inline constexpr int fast_refresh_budget = 15;  ///< force a full flash after N fast updates.
inline constexpr std::uint32_t rotary_hold_ms = 500;
inline constexpr std::uint32_t battery_sample_interval_ms = 60000;
inline constexpr std::uint32_t battery_display_interval_ms = 600000;

struct runtime {
  std::uint8_t* framebuffer = nullptr;
  std::array<event::value, input_queue_size> events{};
  std::size_t event_head = 0;
  std::size_t event_tail = 0;

  drivers::it8951::device_t epd{};
  drivers::gt911::device_t touch{};
  sdmmc_card_t* sd_card = nullptr;
  char storage_root[storage::path_max] = "/sdcard";

  // Edge-detect state for the rotary (active-low: level 0 = pressed), and the touch
  // down/up transition -- mirrors app_main.cpp's port_t fields exactly.
  bool touch_down = false;
  std::uint16_t touch_x = 0;
  std::uint16_t touch_y = 0;
  bool rotary_right = false;
  bool rotary_left = false;
  bool rotary_press = false;
  std::uint32_t rotary_right_at = 0;
  std::uint32_t rotary_left_at = 0;
  bool rotary_right_long = false;
  bool rotary_left_long = false;
  std::uint8_t fast_updates = 0;
  int battery_cached = -1;
  std::uint32_t battery_sample_ms = 0;
  int battery_displayed = -1;
  std::uint32_t battery_display_ms = 0;

  display::device display{};
  platform::device platform{};
  input::device input{};
  storage::device storage{};
  capability::registry capabilities{};
};

namespace detail {

inline bool inject(runtime& self, event::value value) {
  if (self.event_tail - self.event_head >= input_queue_size) return false;
  self.events[self.event_tail % input_queue_size] = value;
  ++self.event_tail;
  return true;
}

inline void display_update(display::device& self, geometry::rect area, refresh::mode mode) {
  auto& rt = *static_cast<runtime*>(self.context);
  if (area.w <= 0 || area.h <= 0) return;

  int x = area.x > 0 ? area.x : 0;
  int y = area.y > 0 ? area.y : 0;
  int right = area.x + area.w < width ? area.x + area.w : width;
  int bottom = area.y + area.h < height ? area.y + area.h : height;
  x &= ~3;
  right = (right + 3) & ~3;
  if (right > width) right = width;
  if (right <= x || bottom <= y) return;

  const int stride = width / 2;
  const std::uint16_t transfer_width = static_cast<std::uint16_t>(right - x);
  const std::uint16_t transfer_height = static_cast<std::uint16_t>(bottom - y);
  const std::size_t transfer_bytes = static_cast<std::size_t>(transfer_width / 2) * transfer_height;
  auto* transfer = static_cast<std::uint8_t*>(
      heap_caps_malloc(transfer_bytes, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
  if (transfer == nullptr) transfer = static_cast<std::uint8_t*>(heap_caps_malloc(transfer_bytes, MALLOC_CAP_8BIT));
  if (transfer == nullptr) return;

  for (std::uint16_t row = 0; row < transfer_height; ++row) {
    const std::uint8_t* source =
        rt.framebuffer + static_cast<std::size_t>(y + row) * stride + x / 2;
    std::memcpy(transfer + static_cast<std::size_t>(row) * transfer_width / 2, source,
                transfer_width / 2);
  }

  bool force_full = false;
  if (mode == refresh::mode::fast) {
    force_full = ++rt.fast_updates >= fast_refresh_budget;
    if (force_full) rt.fast_updates = 0;
  } else {
    rt.fast_updates = 0;
  }
  if (force_full) {
    x = 0;
    y = 0;
    right = width;
    bottom = height;
  }
  const auto panel_mode = force_full || mode == refresh::mode::full || mode == refresh::mode::quality
                              ? drivers::it8951::refresh_gc16
                              : mode == refresh::mode::reader_quality ? drivers::it8951::refresh_gl16
                                                                       : drivers::it8951::refresh_du;
  if (force_full) {
    heap_caps_free(transfer);
    const std::size_t full_bytes = static_cast<std::size_t>(width / 2) * height;
    transfer = static_cast<std::uint8_t*>(
        heap_caps_malloc(full_bytes, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
    if (transfer == nullptr) transfer = static_cast<std::uint8_t*>(heap_caps_malloc(full_bytes, MALLOC_CAP_8BIT));
    if (transfer == nullptr) return;
    for (int row = 0; row < height; ++row) {
      std::memcpy(transfer + static_cast<std::size_t>(row) * width / 2,
                  rt.framebuffer + static_cast<std::size_t>(row) * stride, width / 2);
    }
  }
  bool refreshed = false;
  if (drivers::it8951::write_image_4bpp(&rt.epd, transfer, static_cast<std::uint16_t>(x),
                                        static_cast<std::uint16_t>(y),
                                        force_full ? width : transfer_width,
                                        force_full ? height : transfer_height) == ESP_OK) {
    refreshed = drivers::it8951::refresh(&rt.epd, static_cast<std::uint16_t>(x),
                                         static_cast<std::uint16_t>(y),
                                         force_full ? width : transfer_width,
                                         force_full ? height : transfer_height, panel_mode) == ESP_OK;
  }
  heap_caps_free(transfer);
  if (refreshed && (force_full || mode == refresh::mode::full)) {
    rt.battery_display_ms = static_cast<std::uint32_t>(esp_timer_get_time() / 1000);
  }
}

inline std::uint32_t platform_now_ms(platform::device&) {
  return static_cast<std::uint32_t>(esp_timer_get_time() / 1000);
}

inline bool platform_wall_time(platform::device&, int& hour, int& minute) {
  // No RTC on this board, matching the old app_main.cpp exactly.
  hour = -1;
  minute = -1;
  return false;
}

inline int platform_battery_percent(platform::device& self) {
  auto& rt = *static_cast<runtime*>(self.context);
  const std::uint32_t now = platform_now_ms(self);
  if (rt.battery_cached < 0 || now - rt.battery_sample_ms >= battery_sample_interval_ms) {
    std::uint16_t millivolts = 0;
    if (battery_voltage_mv(&millivolts) == ESP_OK) {
      rt.battery_cached = battery_percent(millivolts);
      rt.battery_sample_ms = now;
    }
  }
  if (rt.battery_cached < 0) return -1;
  if (rt.battery_displayed < 0 || now - rt.battery_display_ms >= battery_display_interval_ms) {
    rt.battery_displayed = rt.battery_cached;
    rt.battery_display_ms = now;
  }
  return rt.battery_displayed;
}

inline void platform_enter_deep_sleep(platform::device&, std::uint32_t wake_after_ms) {
  // Closes the gap: the old app_main.cpp never called this. wake_after_ms == 0 means
  // rely on the ext0 rotary-press wakeup alone, matching enter_deep_sleep(0)'s old
  // "no timer fallback" meaning.
  (void)enter_deep_sleep(static_cast<std::uint64_t>(wake_after_ms) * 1000ULL);
}

inline bool input_poll(input::device& self, event::value& out) {
  auto& rt = *static_cast<runtime*>(self.context);
  if (rt.event_head != rt.event_tail) {
    out = rt.events[rt.event_head % input_queue_size];
    ++rt.event_head;
    return true;
  }

  // Queue empty: sample hardware once and queue whatever edges fired, mirroring
  // app_main.cpp's poll_input() exactly (dispatch on button RELEASE, not press).
  drivers::gt911::state_t touch_state{};
  if (drivers::gt911::read(&rt.touch, &touch_state) == ESP_OK && touch_state.ready) {
    const bool active = touch_state.count != 0;
    if (active) {
      rt.touch_x = touch_state.points[0].x < width ? touch_state.points[0].x : width - 1;
      rt.touch_y = touch_state.points[0].y < height ? touch_state.points[0].y : height - 1;
    }
    if (active != rt.touch_down) {
      rt.touch_down = active;
      if (active) inject(rt, event::tap(rt.touch_x, rt.touch_y));
    }
  }

  const std::uint32_t now = platform_now_ms(rt.platform);
  const bool right = gpio_get_level(pins::rotary_right_pin) == 0;
  const bool left = gpio_get_level(pins::rotary_left_pin) == 0;
  const bool press = gpio_get_level(pins::rotary_press_pin) == 0;
  if (right && !rt.rotary_right) {
    rt.rotary_right_at = now;
    rt.rotary_right_long = false;
  } else if (right && !rt.rotary_right_long && now - rt.rotary_right_at >= rotary_hold_ms) {
    inject(rt, event::key(event::key_code::down, true));
    rt.rotary_right_long = true;
  } else if (!right && rt.rotary_right && !rt.rotary_right_long) {
    inject(rt, event::key(event::key_code::down));
  }
  if (left && !rt.rotary_left) {
    rt.rotary_left_at = now;
    rt.rotary_left_long = false;
  } else if (left && !rt.rotary_left_long && now - rt.rotary_left_at >= rotary_hold_ms) {
    inject(rt, event::key(event::key_code::up, true));
    rt.rotary_left_long = true;
  } else if (!left && rt.rotary_left && !rt.rotary_left_long) {
    inject(rt, event::key(event::key_code::up));
  }
  if (rt.rotary_press && !press) inject(rt, event::key(event::key_code::ok));
  rt.rotary_right = right;
  rt.rotary_left = left;
  rt.rotary_press = press;

  if (rt.event_head == rt.event_tail) return false;
  out = rt.events[rt.event_head % input_queue_size];
  ++rt.event_head;
  return true;
}

inline void resolve(const runtime& self, const char* path, char* out, std::size_t out_size) {
  if (path[0] == '/') std::snprintf(out, out_size, "%s", path);
  else std::snprintf(out, out_size, "%s/%s", self.storage_root, path);
}

inline bool storage_list(storage::device&, const char* path, storage::entry_fn callback,
                         void* user) {
  DIR* dir = opendir(path);
  if (dir == nullptr) return false;
  struct dirent* entry;
  while ((entry = readdir(dir)) != nullptr) {
    if (std::strcmp(entry->d_name, ".") == 0 || std::strcmp(entry->d_name, "..") == 0) continue;
    char child[512];
    const int written = std::snprintf(child, sizeof(child), "%s/%s", path, entry->d_name);
    if (written < 0 || static_cast<std::size_t>(written) >= sizeof(child)) continue;
    struct stat file_stat {};
    bool is_directory = entry->d_type == DT_DIR ||
                        (stat(child, &file_stat) == 0 && S_ISDIR(file_stat.st_mode));
    if (!is_directory && entry->d_type == DT_UNKNOWN) {
      DIR* probe = opendir(child);
      if (probe != nullptr) {
        closedir(probe);
        is_directory = true;
      }
    }
    storage::entry value{entry->d_name, is_directory, 0};
    if (!callback(value, user)) break;
  }
  closedir(dir);
  return true;
}

inline bool storage_read(storage::device& self, const char* path, std::uint32_t offset,
                         void* destination, std::uint32_t size) {
  auto& rt = *static_cast<runtime*>(self.context);
  char full[512];
  resolve(rt, path, full, sizeof(full));
  std::FILE* f = std::fopen(full, "rb");
  if (f == nullptr) return false;
  const bool ok = offset <= static_cast<std::uint32_t>(LONG_MAX) &&
                  std::fseek(f, static_cast<long>(offset), SEEK_SET) == 0 &&
                  std::fread(destination, 1, size, f) == size;
  std::fclose(f);
  return ok;
}

inline bool storage_file_size(storage::device& self, const char* path, std::uint32_t& out) {
  auto& rt = *static_cast<runtime*>(self.context);
  char full[512];
  resolve(rt, path, full, sizeof(full));
  struct stat st {};
  if (stat(full, &st) != 0) return false;
  out = static_cast<std::uint32_t>(st.st_size);
  return true;
}

inline bool storage_inflate(storage::device& self, const char* path, std::uint32_t source_offset,
                            std::uint32_t source_size, void* destination,
                            std::uint32_t destination_size) {
  // No zlib on this target: read the whole compressed range into a SPIRAM buffer and
  // run the custom RFC 1951 decoder, matching the old app_main.cpp's epub_inflate.
  if (source_size == 0) return destination_size == 0;
  auto* source =
      static_cast<std::uint8_t*>(heap_caps_malloc(source_size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
  if (source == nullptr) return false;
  const bool ok_read = storage_read(self, path, source_offset, source, source_size);
  std::size_t output_size = 0;
  const esp_err_t error =
      ok_read ? drivers::inflate::decode(source, source_size, static_cast<std::uint8_t*>(destination),
                                         destination_size, &output_size)
              : ESP_FAIL;
  heap_caps_free(source);
  return error == ESP_OK && output_size == destination_size;
}

}  // namespace detail

/** @brief Powers on, initializes the IT8951/GT911 drivers and rotary GPIOs. Does not mount SD. */
inline bool init(runtime& self) {
  self = runtime{};
  self.framebuffer = static_cast<std::uint8_t*>(
      heap_caps_calloc(canvas::buffer_size(width, height, pixel_format), 1,
                       MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
  if (self.framebuffer == nullptr) {
    ESP_LOGE(log_tag, "framebuffer allocation failed");
    return false;
  }
  if (power_on() != ESP_OK) {
    ESP_LOGE(log_tag, "power rails failed");
    return false;
  }

  const gpio_config_t rotary_config = {
      .pin_bit_mask = (1ULL << pins::rotary_right_pin) | (1ULL << pins::rotary_left_pin) |
                     (1ULL << pins::rotary_press_pin),
      .mode = GPIO_MODE_INPUT,
      // GPIO37-39 are input-only on ESP32 and have no internal pull-ups; the M5Paper
      // board provides the required external bias.
      .pull_up_en = GPIO_PULLUP_DISABLE,
      .pull_down_en = GPIO_PULLDOWN_DISABLE,
      .intr_type = GPIO_INTR_DISABLE,
  };
  if (gpio_config(&rotary_config) != ESP_OK) {
    ESP_LOGE(log_tag, "rotary GPIO setup failed");
    return false;
  }

  const drivers::it8951::config_t epd_config{
      pins::epd_spi_host, pins::epd_sck_pin, pins::epd_mosi_pin, pins::epd_miso_pin,
      pins::epd_cs_pin,   pins::epd_busy_pin, pins::panel_width,  pins::panel_height,
      pins::panel_rotation, 10000000,
  };
  ESP_LOGI(log_tag, "initializing IT8951");
  if (drivers::it8951::init(&self.epd, &epd_config) != ESP_OK) {
    ESP_LOGE(log_tag, "IT8951 setup failed");
    return false;
  }

  const drivers::gt911::config_t touch_config{I2C_NUM_0, pins::touch_sda_pin, pins::touch_scl_pin,
                                              100000};
  ESP_LOGI(log_tag, "initializing GT911");
  if (drivers::gt911::init(&self.touch, &touch_config) != ESP_OK) {
    ESP_LOGE(log_tag, "GT911 setup failed");
    return false;
  }

  self.display.width = width;
  self.display.height = height;
  self.display.format = pixel_format;
   self.display.framebuffer = self.framebuffer;
  self.display.stride = canvas::stride_for(width, pixel_format);
  self.display.update_align = 4;  // IT8951 partial updates require 4-pixel horizontal alignment.
  self.display.context = &self;
  self.display.update = detail::display_update;

  self.platform.context = &self;
  self.platform.now_ms = detail::platform_now_ms;
  self.platform.wall_time = detail::platform_wall_time;
  self.platform.battery_percent = detail::platform_battery_percent;
  self.platform.enter_deep_sleep = detail::platform_enter_deep_sleep;

  self.input.context = &self;
  self.input.poll = detail::input_poll;

  self.storage.context = &self;
  self.storage.list = detail::storage_list;
  self.storage.read = detail::storage_read;
  self.storage.file_size = detail::storage_file_size;
  self.storage.inflate = detail::storage_inflate;

  capability::set(self.capabilities, capability::id::display, &self.display);
  capability::set(self.capabilities, capability::id::input, &self.input);
  capability::set(self.capabilities, capability::id::platform, &self.platform);
  capability::set(self.capabilities, capability::id::storage, &self.storage);
  // wifi/bluetooth/secret/power/rtc/front_light/usb intentionally unregistered: no
  // real driver exists for any of them, matching the old app_main.cpp exactly (it
  // never initialized a WiFi stack either -- Settings' Wi-Fi row was always a
  // UI-only boolean, even on real hardware).
  return true;
}

/** @brief Mounts the SD card over SPI at `root` (always "/sdcard" in practice). */
inline bool mount(runtime& self, const char* root) {
  std::snprintf(self.storage_root, sizeof(self.storage_root), "%s", root);

  sdmmc_host_t host = SDSPI_HOST_DEFAULT();
  host.slot = pins::epd_spi_host;
  sdspi_device_config_t device_config = SDSPI_DEVICE_CONFIG_DEFAULT();
  device_config.host_id = pins::epd_spi_host;
  device_config.gpio_cs = pins::sd_cs_pin;
  esp_vfs_fat_mount_config_t mount_config = {
      .format_if_mount_failed = false,
      .max_files = 8,
      .allocation_unit_size = 16 * 1024,
      .disk_status_check_enable = false,
      .use_one_fat = false,
  };
  return esp_vfs_fat_sdspi_mount(root, &host, &device_config, &mount_config, &self.sd_card) ==
         ESP_OK;
}

}  // namespace board::m5paper
