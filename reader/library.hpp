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

/**
 * @brief The in-memory set of discovered/added books. `books` has one extra slot beyond
 * `max_books`: a single, reused "transient" overflow entry (index `max_books`) that lets
 * File Manager always open a book even when the regular scan-populated library is full,
 * ported from the old app's dedicated 17th slot (`APP_MAX_BOOKS` in `app_internal.h`).
 */
struct index {
  std::array<book::item, max_books + 1> books{};
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

/** @brief True when slot `max_books` currently holds a live transient overflow book. */
inline bool has_transient_overflow(const index& lib) {
  return lib.count == max_books + 1 && lib.books[max_books].transient;
}

/**
 * @brief Opens `path`/`title` through the transient-overflow slot, ported from the old
 * app's `app_open_storage_epub`: reuses an existing entry with the same path if one is
 * already in the library; else adds normally while under `max_books`; else writes into
 * the single reused slot `max_books`, marking it transient. Always succeeds (returns a
 * valid index) unless `path`/`title` is null. Used by File Manager, which must be able to
 * open a book even when the regularly-scanned library is already full.
 */
inline int open_transient(index& lib, const char* path, const char* title) {
  if (path == nullptr || title == nullptr) return -1;
  int found = -1;
  for (int i = 0; i < lib.count; ++i) {
    if (lib.books[i].epub_source && std::strcmp(lib.books[i].path.data(), path) == 0) {
      found = i;
      break;
    }
  }
  if (found >= 0) return found;
  if (lib.count < max_books) {
    if (!add(lib, path, title)) return -1;
    return lib.count - 1;
  }
  const int slot = max_books;
  if (lib.count <= slot) lib.count = slot + 1;
  book::item& item = lib.books[slot];
  book::make_title(item.title.data(), item.title.size(), title);
  item.author = "EPUB";
  item.format = "EPUB";
  item.epub_source = true;
  item.favorite = false;
  item.transient = true;
  item.progress = 0;
  std::snprintf(item.path.data(), item.path.size(), "%s", path);
  return slot;
}

/**
 * @brief Removes book `index_to_remove`, compacting the array; invalid indices are a no-op.
 * Guards the transient overflow slot (`max_books`): deleting a regular book while the slot
 * is occupied drops the transient entry first, so it's never shifted into a lower, regular
 * index -- preserving the invariant that only slot `max_books` can ever be transient.
 */
inline void remove(index& lib, int index_to_remove) {
  if (index_to_remove < 0 || index_to_remove >= lib.count) return;
  if (!lib.books[index_to_remove].transient && has_transient_overflow(lib)) --lib.count;
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
