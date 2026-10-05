#pragma once

#include "../core/storage.hpp"
#include "book.hpp"

#include <array>
#include <cstddef>
#include <cstdio>
#include <cstring>
#include <strings.h>

/**
 * @brief Library discovery and the in-memory book index, ported from
 * `app_scan_library`/`app_library_add_path` (app/app.c, app/page_library.c).
 *
 * The old scan called FatFS `f_opendir`/`f_readdir` through an
 * `app_dir_entry_fn` callback supplied per-board. This version goes
 * through `storage::list` exclusively (core/storage.hpp) so it runs
 * unchanged against the simulator's in-memory storage and, later,
 * M5Paper's real FatFS-backed storage capability -- preserving the
 * breadth-first traversal order and bounded queue depth of the original.
 */
namespace library {

inline constexpr int max_books = 16;
inline constexpr int scan_queue_depth = 16;
inline constexpr std::size_t scan_path_max = 256;

/** @brief The in-memory set of discovered/added books. */
struct index {
  std::array<book::item, max_books> books{};
  int count = 0;
};

/** @brief True when `name` has a `.epub` or `.epu` extension (case-insensitive). */
inline bool supported(const char* name) {
  const std::size_t n = std::strlen(name);
  return (n > 5 && strcasecmp(name + n - 5, ".epub") == 0) ||
         (n > 4 && strcasecmp(name + n - 4, ".epu") == 0);
}

/** @brief Empties the index. */
inline void clear(index& lib) { lib.count = 0; }

/** @brief Adds one book by path/title, repairing the display title; false when full. */
inline bool add(index& lib, const char* path, const char* title) {
  if (path == nullptr || title == nullptr || lib.count >= max_books) return false;
  book::item& item = lib.books[lib.count++];
  book::make_title(item.title.data(), item.title.size(), title);
  item.author = "EPUB";
  item.format = "EPUB";
  item.epub_source = true;
  item.favorite = false;
  item.transient = false;
  item.progress = 0;
  std::snprintf(item.path.data(), item.path.size(), "%s", path);
  return true;
}

/** @brief Removes book `index_to_remove`, compacting the array; invalid indices are a no-op. */
inline void remove(index& lib, int index_to_remove) {
  if (index_to_remove < 0 || index_to_remove >= lib.count) return;
  for (int i = index_to_remove; i < lib.count - 1; ++i) lib.books[i] = lib.books[i + 1];
  --lib.count;
}

namespace detail {

struct scan_state {
  index* lib = nullptr;
  const char* mount = nullptr;  ///< scan root; stripped from stored book paths, like the old scan.
  std::array<std::array<char, scan_path_max>, scan_queue_depth> queue{};
  unsigned head = 0;
  unsigned tail = 0;
};

inline bool scan_entry(const storage::entry& value, void* user) {
  auto& state = *static_cast<scan_state*>(user);
  const char* dir = state.queue[state.head - 1].data();
  char path[scan_path_max * 2];
  const int written = std::snprintf(path, sizeof(path), "%s/%s", dir, value.name);
  if (written < 0 || static_cast<std::size_t>(written) >= sizeof(path)) return true;

  if (value.directory) {
    const std::size_t length = std::strlen(path);
    if (state.tail < scan_queue_depth && length < scan_path_max) {
      std::memcpy(state.queue[state.tail].data(), path, length + 1);
      ++state.tail;
    }
    return true;
  }
  if (supported(value.name)) {
    // Store a path relative to the scan root, matching the old FatFS scanner
    // (it stripped s_scan_mount the same way) rather than the absolute path
    // used internally to list directories.
    const std::size_t mount_length = std::strlen(state.mount);
    const char* relative = path + mount_length + (path[mount_length] == '/' ? 1 : 0);
    add(*state.lib, relative, value.name);
  }
  return true;
}

}  // namespace detail

/**
 * @brief Recursively scans `root` through the board's storage capability,
 * clearing and repopulating `lib`. Breadth-first over a bounded queue of
 * pending directories (matching the old FatFS scanner's capacity), so a
 * very deep tree stops discovering new directories rather than overflowing.
 * Stored book paths are relative to `root`, matching the old scanner.
 */
inline void scan(index& lib, storage::device& device, const char* root) {
  clear(lib);
  detail::scan_state state{};
  state.lib = &lib;
  state.mount = root;
  std::snprintf(state.queue[state.tail++].data(), scan_path_max, "%s", root);
  while (state.head < state.tail) {
    ++state.head;
    if (!storage::list(device, state.queue[state.head - 1].data(), detail::scan_entry, &state)) {
      break;
    }
  }
}

}  // namespace library
