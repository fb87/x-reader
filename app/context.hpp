#pragma once

#include <cstdint>

#include "../core/capability.hpp"
#include "../core/connectivity.hpp"
#include "../core/input.hpp"
#include "../core/platform.hpp"
#include "../core/shell.hpp"
#include "../core/state.hpp"
#include "../core/storage.hpp"
#include "../reader/library.hpp"
#include "../reader/session.hpp"

/**
 * @brief The single shared app context, replacing the old `app_t g_app`
 * global (app/app_internal.h). Pages recover it the same way old pages
 * recovered `g_app`: not through a global, but through a back-pointer
 * stored on each page's/dialog's own struct (set when the page is
 * created) -- see app/page_*.hpp.
 *
 * Settings/current-book/connectivity scalars that need to survive a
 * restart live in `*memory` (checkpointed to `*persistent`, see
 * checkpoint()/restore()) rather than as plain fields here, per
 * docs/DESIGN.md s19/s20. `library`/`session` are domain caches, not
 * state scalars (s22): rebuilt by scanning/opening, not persisted
 * directly.
 */
namespace app {

/** @brief State-store keys this app reads/writes. Centralized so pages agree on spelling. */
namespace key {
inline constexpr const char* book_current = "reader.book.current";
inline constexpr const char* font_size = "reader.settings.font_size";
inline constexpr const char* full_refresh_every = "reader.settings.full_refresh_every";
inline constexpr const char* show_progress = "reader.settings.show_progress";
inline constexpr const char* sleep_timeout_minutes = "reader.settings.sleep_timeout_minutes";
inline constexpr const char* wifi_connected = "network.wifi.connected";
inline constexpr const char* bluetooth_enabled = "network.bluetooth.enabled";
}  // namespace key

/** @brief Durable keys checkpointed between memory and persistent storage. */
inline constexpr const char* const durable_keys[] = {
    key::book_current,          key::font_size,      key::full_refresh_every, key::show_progress,
    key::sleep_timeout_minutes, key::wifi_connected, key::bluetooth_enabled,
};

struct context {
  capability::registry* capabilities = nullptr;
  state::store* memory = nullptr;
  state::store* persistent = nullptr;
  input::device* input = nullptr;  ///< cached from capabilities; shell::context has no input field.
  shell::context shell{};
  reader::session session{};
  library::index library{};
  char storage_root[storage::path_max] = {0};
};

/** @brief Copies durable keys from `*persistent` into `*memory`, if a persistent store is set. */
inline void restore_persistent(context& self) {
  if (self.persistent == nullptr) return;
  for (const char* key : durable_keys) {
    if (auto* entry = state::find(*self.persistent, key)) {
      switch (entry->data.type) {
        case state::value_type::boolean:
          state::set(*self.memory, key, entry->data.boolean);
          break;
        case state::value_type::integer:
          state::set(*self.memory, key, entry->data.integer);
          break;
        case state::value_type::string:
          state::set(*self.memory, key, entry->data.string.data());
          break;
        case state::value_type::empty:
          break;
      }
    }
  }
}

/** @brief Copies durable keys from `*memory` into `*persistent` and saves it, if set. */
inline void checkpoint(context& self) {
  if (self.persistent == nullptr) return;
  for (const char* key : durable_keys) {
    if (auto* entry = state::find(*self.memory, key)) {
      switch (entry->data.type) {
        case state::value_type::boolean:
          state::set(*self.persistent, key, entry->data.boolean);
          break;
        case state::value_type::integer:
          state::set(*self.persistent, key, entry->data.integer);
          break;
        case state::value_type::string:
          state::set(*self.persistent, key, entry->data.string.data());
          break;
        case state::value_type::empty:
          break;
      }
    }
  }
}

/** @brief The shell's current theme, for pages/dialogs that only have a `page::context&`. */
inline const shell::theme& theme_of(const page::context& p) { return *p.shell->theme_ptr; }
inline const shell::theme& theme_of(const dialog::context& d) { return *d.shell->theme_ptr; }

}  // namespace app
